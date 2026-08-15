/*
 * piper.cpp - Speech Dispatcher Piper output module
 *
 * Copyright (C) 2022 Michael Hansen
 * Copyright (C) 2025 Derek L Davies
 * Copyright (C) 2026 Sola
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1, or (at your option) any later
 * version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <piper.h>

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#elif __has_include(<json.hpp>)
#include <json.hpp>
#endif

#include <rubberband/RubberBandStretcher.h>

#include "spd_audio.h"
#include <speechd_types.h>
#include "module_utils.h"

#define MODULE_NAME     "piper"
#define DBG_MODNAME     "piper"
#define MODULE_VERSION  "0.1"
#define DEBUG_MODULE 5
DECLARE_DEBUG();

namespace piper_module {
    using json = nlohmann::json;
    using RubberBand::RubberBandStretcher;

    MOD_OPTION_1_STR(ModelPath)
    MOD_OPTION_1_STR(ConfigPath)
    MOD_OPTION_1_STR(SoundIconFolder)
    MOD_OPTION_1_STR(ESpeakNGDataDirPath)

    static const int bs = 1024;
    static piper_synthesizer *synth = NULL;
    static json model_config;
    static int model_sample_rate = 22050;
    static int model_num_speakers = 1;
    static std::string model_language = "en_US";
    static std::atomic<bool> stop_requested{false};
    static int current_speaker_id = 0;
    static const char *default_voice_name = NULL;
    static SPDVoice **piper_voice_list = NULL;

    static RubberBandStretcher::Options initOptions()
    {
        RubberBandStretcher::Options options = 0;
        options |= RubberBandStretcher::OptionProcessRealTime;
        options |= RubberBandStretcher::OptionPhaseIndependent;
        options |= RubberBandStretcher::OptionThreadingAuto;
        options |= RubberBandStretcher::OptionTransientsCrisp;
        options |= RubberBandStretcher::OptionDetectorCompound;
        return options;
    }

    static void piper_stretch_and_copy(const int samplerate, const int channels, const float *in_samples, const size_t num_samples, std::vector<int16_t>& sharedAudioBuffer)
    {
        if (num_samples == 0 || in_samples == NULL) return;

        // Peak normalization: Piper neural vocoders output raw float amplitudes
        // peaking around 0.4 - 0.6. Normalize to 100% full scale (1.0 / max_sample).
        float max_sample = 0.01f;
        for (size_t i = 0; i < num_samples; ++i) {
            float abs_s = std::abs(in_samples[i]);
            if (abs_s > max_sample) {
                max_sample = abs_s;
            }
        }
        float norm_factor = (max_sample > 0.01f) ? (1.0f / max_sample) : 1.0f;

        // Speed multiplier: map -100..100 onto duration ratio
        double speed = 1.0;
        if (msg_settings.rate > 0) {
            speed = 1.0 + ((double)msg_settings.rate / 50.0);
        } else if (msg_settings.rate < 0) {
            speed = 1.0 + ((double)msg_settings.rate / 150.0);
        }
        double ratio = 1.0 / std::max(0.1, speed);
        if (ratio < 0.01) ratio = 0.01;

        // Pitch shift: map -100 .. 100 onto -10.0 .. 10.0 semitones
        double mS = std::clamp((double)msg_settings.pitch, -100.0, 100.0);
        double pitchshift = mS / 10.0;
        double frequencyshift = 1.0;
        if (pitchshift != 0.0) frequencyshift *= std::pow(2.0, pitchshift / 12.0);

        // Volume / gain: map SSIP -100..100 onto gain factor (0 is nominal 1.0f gain)
        float gain = 1.0f;
        if (msg_settings.volume < 0) {
            gain = ((float)msg_settings.volume + 100.0f) / 100.0f;
        } else if (msg_settings.volume > 0) {
            gain = 1.0f + ((float)msg_settings.volume / 100.0f);
        }
        gain = std::max(0.0f, gain);

        // Pre-allocate buffer capacity to avoid multiple dynamic reallocations during stretching
        sharedAudioBuffer.reserve(static_cast<size_t>(num_samples * std::max(1.0, ratio) + 1024));

        RubberBandStretcher ts(samplerate, channels, initOptions(), ratio, frequencyshift);
        ts.setExpectedInputDuration(num_samples);
        ts.setMaxProcessSize(bs);

        const float sample_scale = norm_factor * 32767.0f;
        float cbuf_arr[bs];
        float *cbuf_ptrs[1] = { cbuf_arr };

        size_t countIn = 0;
        while (countIn < num_samples) {
            size_t ibSize = std::min((size_t)bs, num_samples - countIn);
            for (size_t i = 0; i < ibSize; ++i) {
                // Apply peak normalization and scale to 16-bit amplitude range [-32767, 32767]
                cbuf_arr[i] = in_samples[countIn + i] * sample_scale;
            }
            countIn += ibSize;
            ts.process(cbuf_ptrs, ibSize, countIn >= num_samples);

            int avail;
            while ((avail = ts.available()) > 0) {
                size_t obSize = std::min((size_t)avail, (size_t)bs);
                ts.retrieve(cbuf_ptrs, obSize);
                for (size_t i = 0; i < obSize; ++i) {
                    float value = gain * cbuf_arr[i];
                    value = std::clamp(value, -32768.0f, 32767.0f);
                    sharedAudioBuffer.push_back(static_cast<int16_t>(value));
                }
            }
        }
    }

    static int piper_voice_name_to_speaker_id(const char *voice_name)
    {
        if (voice_name && model_config.contains("speaker_id_map")) {
            auto &spkr_map = model_config["speaker_id_map"];
            if (spkr_map.contains(voice_name)) {
                return spkr_map[voice_name].get<int>();
            }
        }
        return 0;
    }

    static SPDVoice **piper_allocate_voice_list()
    {
        std::filesystem::path modelPath(ModelPath ? ModelPath : "piper");
        std::string model_stem = modelPath.stem().string();

        if (model_num_speakers > 1 && model_config.contains("speaker_id_map") && !model_config["speaker_id_map"].empty()) {
            auto &speakerIdMap = model_config["speaker_id_map"];
            int count = speakerIdMap.size();
            SPDVoice **result = g_new0(SPDVoice *, count + 1);

            SPDVoice **reg_voices = module_list_registered_voices();
            int i = 0;
            for (auto &speakerItem : speakerIdMap.items()) {
                std::string spkr_name = speakerItem.key();
                SPDVoice *v = g_new0(SPDVoice, 1);
                v->name = g_strdup(spkr_name.c_str());
                v->language = g_strdup(model_language.c_str());
                const char *var_str = "none";
                for (int j = 0; reg_voices && reg_voices[j]; ++j) {
                    SPDVoice *r_v = reg_voices[j];
                    if (r_v->name && strcasecmp(v->name, r_v->name) == 0) {
                        if (r_v->variant && strlen(r_v->variant) > 0) {
                            var_str = r_v->variant;
                        }
                        break;
                    }
                }
                v->variant = g_strdup(var_str);
                result[i++] = v;
            }
            result[count] = NULL;
            return result;
        } else {
            SPDVoice **result = g_new0(SPDVoice *, 2);
            SPDVoice *v = g_new0(SPDVoice, 1);
            v->name = g_strdup(model_stem.c_str());
            v->language = g_strdup(model_language.c_str());
            v->variant = g_strdup("none");
            result[0] = v;
            result[1] = NULL;
            return result;
        }
    }

    static void piper_free_voice_list()
    {
        if (piper_voice_list != NULL) {
            for (int i = 0; piper_voice_list[i] != NULL; i++) {
                g_free(piper_voice_list[i]->name);
                g_free(piper_voice_list[i]->language);
                g_free(piper_voice_list[i]->variant);
                g_free(piper_voice_list[i]);
            }
            g_free(piper_voice_list);
            piper_voice_list = NULL;
        }
    }

    static const char *piper_search_for_sound_icon(const char *icon_name)
    {
        char *fn = NULL;
        if (!SoundIconFolder || strlen(SoundIconFolder) == 0) return fn;
        GString *filename = g_string_new(SoundIconFolder);
        filename = g_string_append(filename, icon_name);
        if (g_file_test(filename->str, G_FILE_TEST_EXISTS)) fn = filename->str;
        return g_string_free(filename, (fn == NULL));
    }

    static void piper_handle_text(const char *data)
    {
        if (!synth) {
            DBG("Synth not initialized");
            return;
        }
        char *cmdInp = (char *) module_strip_ssml(data);
        DBG("Input after strip XML: %s", cmdInp);

        piper_synthesize_options options = piper_default_synthesize_options(synth);
        options.speaker_id = current_speaker_id;

        int ret = piper_synthesize_start(synth, cmdInp, &options);
        g_free(cmdInp);
        if (ret != PIPER_OK) {
            DBG("Failed to start synthesis with piper_synthesize_start (error code: %d)", ret);
            return;
        }

        module_report_event_begin();

        piper_audio_chunk chunk;
        do {
            int status = piper_synthesize_next(synth, &chunk);
            if (status == PIPER_ERR_GENERIC) {
                DBG("piper_synthesize_next failed with PIPER_ERR_GENERIC");
                break;
            }
            if (stop_requested) {
                DBG("Stop requested during synthesis");
                break;
            }
            if (chunk.num_samples > 0 && chunk.samples != NULL) {
                std::vector<int16_t> chunkAudioBuffer;
                piper_stretch_and_copy(chunk.sample_rate, 1, chunk.samples, chunk.num_samples, chunkAudioBuffer);
                if (!chunkAudioBuffer.empty() && !stop_requested) {
                    AudioFormat format = SPD_AUDIO_LE;
                    AudioTrack track;
                    track.bits = 16;
                    track.num_samples = chunkAudioBuffer.size();
                    track.samples = chunkAudioBuffer.data();
                    track.num_channels = 1;
                    track.sample_rate = chunk.sample_rate;
                    module_tts_output_server(&track, format);
                }
            }
        } while (!chunk.is_last);

        module_report_event_end();
    }

    static void piper_handle_sound_icon(const char *icon_name)
    {
        char *icon_path = (char *) piper_search_for_sound_icon(icon_name);
        if (icon_path != NULL) {
            module_report_event_begin();
            (void)module_play_file(icon_path);
            module_report_icon(icon_path);
            module_report_event_end();
            g_free(icon_path);
        } else {
            MSG(3, "Warning: Speaking sound icon name, %s, as a fallback, since audio file can not be found.", icon_name);
            piper_handle_text(icon_name);
        }
    }

    static void piper_set_language(char *lang)
    {
        DBG("%s, lang=%s, voice_type=%d", __FUNCTION__, lang ? lang : "", (int)msg_settings.voice_type);

        if (msg_settings.voice_type > 0) {
            const char *vname = module_getvoice(lang ? lang : model_language.c_str(), msg_settings.voice_type);
            if (vname) {
                current_speaker_id = piper_voice_name_to_speaker_id(vname);
                return;
            }
        }

        SPDVoice **lst = piper_voice_list;
        int index = -1;
        if (lang && lst) {
            char *dash = strchr(lang, '-');
            char *langbase = dash ? g_strndup(lang, dash - lang + 1) : g_strdup_printf("%s-", lang);
            for (int i = 0; lst[i]; ++i) {
                if (lst[i]->language && !strcasecmp(lst[i]->language, lang)) {
                    current_speaker_id = piper_voice_name_to_speaker_id(lst[i]->name);
                    index = i;
                    break;
                }
                if (index == -1 && lst[i]->language) {
                    if (!strncasecmp(lst[i]->language, langbase, strlen(langbase))) {
                        current_speaker_id = piper_voice_name_to_speaker_id(lst[i]->name);
                        index = i;
                    }
                }
            }
            g_free(langbase);
        }
        if (index == -1) {
            if (piper_voice_list && piper_voice_list[0])
                current_speaker_id = piper_voice_name_to_speaker_id(piper_voice_list[0]->name);
        }
    }

    static void piper_set_voice_type(SPDVoiceType voice_type)
    {
        const char *lang = msg_settings.voice.language ? msg_settings.voice.language : model_language.c_str();
        const char *vname = module_getvoice(lang, voice_type);
        if (vname) {
            current_speaker_id = piper_voice_name_to_speaker_id(vname);
        } else {
            MSG(3, "Warning: No definition of voice type %d, check 'AddVoice' directives in piper.conf file.", (int)voice_type);
        }
    }

    static void piper_set_synthesis_voice(char *voice_name)
    {
        const char *vname = voice_name ? voice_name : (msg_settings.voice.name ? msg_settings.voice.name : default_voice_name);
        current_speaker_id = piper_voice_name_to_speaker_id(vname);
        if (current_speaker_id < 0) current_speaker_id = 0;
    }

    extern "C" {

        int module_load(void)
        {
            INIT_SETTINGS_TABLES();
            REGISTER_DEBUG();
            MOD_OPTION_1_STR_REG(ModelPath, "");
            MOD_OPTION_1_STR_REG(ConfigPath, "");
            MOD_OPTION_1_STR_REG(SoundIconFolder, "/usr/share/sounds/sound-icons/");
            MOD_OPTION_1_STR_REG(ESpeakNGDataDirPath, "/usr/share/espeak-ng-data/");
            module_register_available_voices();
            module_register_settings_voices();
            return 0;
        }

        int module_init(char **status_info)
        {
            module_audio_set_server();
            module_audio_init(status_info);

            if (!ModelPath || strlen(ModelPath) == 0) {
                *status_info = g_strdup("piper: ModelPath is not configured in piper.conf");
                return -1;
            }

            std::string config_path_str;
            if (ConfigPath && strlen(ConfigPath) > 0) {
                config_path_str = ConfigPath;
            } else {
                config_path_str = std::string(ModelPath) + ".json";
            }

            try {
                std::ifstream config_file(config_path_str);
                if (config_file.is_open()) {
                    model_config = json::parse(config_file);
                    if (model_config.contains("audio") && model_config["audio"].contains("sample_rate")) {
                        model_sample_rate = model_config["audio"]["sample_rate"].get<int>();
                    }
                    if (model_config.contains("num_speakers")) {
                        model_num_speakers = model_config["num_speakers"].get<int>();
                    }
                    if (model_config.contains("language") && model_config["language"].contains("code")) {
                        model_language = model_config["language"]["code"].get<std::string>();
                    }
                } else {
                    DBG(DBG_MODNAME " Warning: Could not open JSON config file at '%s'", config_path_str.c_str());
                }

                piper_create_options opts;
                piper_init_create_options(&opts);
                opts.model_path = ModelPath;
                opts.config_path = config_path_str.c_str();
                opts.espeak_data_path = (ESpeakNGDataDirPath && strlen(ESpeakNGDataDirPath) > 0) ? ESpeakNGDataDirPath : NULL;

                synth = piper_create_with_options(&opts);
                if (!synth) {
                    *status_info = g_strdup("piper: Failed to create piper synthesizer with piper_create_with_options");
                    return -1;
                }

                default_voice_name = module_getdefaultvoice();
                current_speaker_id = piper_voice_name_to_speaker_id(default_voice_name);
                if (current_speaker_id < 0) current_speaker_id = 0;

                piper_voice_list = piper_allocate_voice_list();
                *status_info = g_strdup(DBG_MODNAME " Initialized successfully.");
            }
            catch (const json::parse_error& e) {
                DBG(DBG_MODNAME " Could not initialize, caught JSON exception: %s", e.what());
                *status_info = g_strdup(e.what());
                return -1;
            }
            catch (const std::runtime_error& e) {
                DBG(DBG_MODNAME " Could not initialize, caught runtime_error exception: %s", e.what());
                *status_info = g_strdup(e.what());
                return -1;
            }
            catch (const std::exception& e) {
                DBG(DBG_MODNAME " Could not initialize, caught exception: %s", e.what());
                *status_info = g_strdup(e.what());
                return -1;
            }
            return 0;
        }

        SPDVoice **module_list_voices(void)
        {
            return piper_voice_list;
        }

        void module_speak_sync(const char *data, size_t bytes, SPDMessageType msgtype)
        {
            stop_requested = false;
            UPDATE_STRING_PARAMETER(voice.language, piper_set_language);
            UPDATE_PARAMETER(voice_type, piper_set_voice_type);
            UPDATE_STRING_PARAMETER(voice.name, piper_set_synthesis_voice);
            module_speak_ok();
            switch (msgtype) {
            case SPD_MSGTYPE_CHAR:
            case SPD_MSGTYPE_KEY:
            case SPD_MSGTYPE_SPELL:
            case SPD_MSGTYPE_TEXT:
                piper_handle_text(data);
                break;
            case SPD_MSGTYPE_SOUND_ICON:
                piper_handle_sound_icon(data);
                break;
            }
        }

        int module_stop(void)
        {
            stop_requested = true;
            return 0;
        }

        size_t module_pause(void)
        {
            stop_requested = true;
            return 0;
        }

        int module_close(void)
        {
            if (synth) {
                piper_free(synth);
                synth = NULL;
            }
            piper_free_voice_list();
            return 0;
        }

    } // extern "C"

} // namespace piper_module
