#include <glib.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "common.h"
#include "spd_module_main.h"
#include "module_utils.h"
#include "spd_audio_plugin.h"
#include "speechd_types.h"

#define MODULE_NAME "piper"
#define MODULE_VERSION "0.1"

DECLARE_DEBUG();

MOD_OPTION_1_INT(PiperAudioBufferMsec);
MOD_OPTION_1_STR(PiperBinPath);
MOD_OPTION_1_STR(PiperModelsPath);
MOD_OPTION_1_FLOAT(PiperNoiseScale);
MOD_OPTION_1_FLOAT(PiperNoiseWidth);
MOD_OPTION_1_FLOAT(PiperSentenceSilence);

int piper_exec(char*, size_t, SPDMessageType, SPDMsgSettings*);
void* _speak_thread_fn(void*);

pthread_t _speak_thread = {0};
volatile int _speak_thread_running = 1;
volatile int _synth_stop = 0;
volatile int _synth_pause = 0;

GAsyncQueue* _p_cmd_queue = NULL;

typedef struct
{
    char* data;
    size_t len;
    SPDMessageType msg_type;
    SPDMsgSettings settings;

} speak_cmd_t;

int module_load(void)
{
    INIT_SETTINGS_TABLES();
    REGISTER_DEBUG();

    MOD_OPTION_1_STR_REG(PiperBinPath, "/usr/bin/piper");
    MOD_OPTION_1_STR_REG(PiperModelsPath, "/usr/share/piper/models");
    MOD_OPTION_1_INT_REG(PiperAudioBufferMsec, 200);
    MOD_OPTION_1_FLOAT_REG(PiperNoiseScale, 0.667);
    MOD_OPTION_1_FLOAT_REG(PiperNoiseWidth, 0.8);
    MOD_OPTION_1_FLOAT_REG(PiperSentenceSilence, 0.2);

    module_register_settings_voices();

    return 0;
}

int module_init(char** msg)
{
    DBG("module_init");
    DBG("PiperBinPath = %s\n", PiperBinPath);
    DBG("PiperModelsPath = %s\n", PiperModelsPath);
    DBG("PiperAudioBufferMsec = %d\n", PiperAudioBufferMsec);
    DBG("PiperNoiseScale = %f\n", PiperNoiseScale);
    DBG("PiperNoiseWidth = %f\n", PiperNoiseWidth);
    DBG("PiperSentenceSilence = %f\n", PiperSentenceSilence);

    _p_cmd_queue = g_async_queue_new();

    module_audio_set_server();

    spd_pthread_create(&_speak_thread, NULL, &_speak_thread_fn, NULL);

    MSG(1, "Module initialized\n");
    *msg = strdup("SUCCESS");
    return 0;
}

SPDVoice** module_list_voices()
{
    DBG("module_list_voices");
    return module_list_registered_voices();
}

int module_speak(char *data, size_t bytes, SPDMessageType msgtype)
{
    DBG("module_speak");
    DBG("Full message: %s\n", data);

    _synth_stop = 0;
    _synth_pause = 0;

    speak_cmd_t* p_cmd = (speak_cmd_t*)calloc(1, sizeof(speak_cmd_t));
    if (!p_cmd)
        return ENOMEM;

    p_cmd->len = bytes;
    p_cmd->msg_type = msgtype;
    p_cmd->data = calloc(1, bytes);
    if (!p_cmd->data)
    {
        free (p_cmd);
        return ENOMEM;
    }

    memcpy(p_cmd->data, data, bytes);
    memcpy(&p_cmd->settings, &msg_settings, sizeof(msg_settings));

    g_async_queue_push(_p_cmd_queue, p_cmd);

    return 1;
}

int module_stop()
{
    DBG("module_stop");
    _synth_stop = 1;
    return 0;
}

size_t module_pause()
{
    DBG("module_pause");
    _synth_pause = 1;
    _synth_stop = 1;
    return 0;
}

int module_close(void)
{
    DBG("module_close");

    _synth_stop = 1;
    _speak_thread_running = 0;
    pthread_join(_speak_thread, NULL);

    MSG(1, "Exiting module\n");
    return 0;
}

void* _speak_thread_fn(void* args)
{
    DBG("_speak_thread_fn");

    while (_speak_thread_running)
    {
        gpointer p_data = g_async_queue_timeout_pop(_p_cmd_queue,
                1000000 /* 1sec */);

        if (p_data)
        {
            speak_cmd_t* p_cmd = (speak_cmd_t*)p_data;

            int ret = piper_exec(p_cmd->data, p_cmd->len, p_cmd->msg_type,
                    &p_cmd->settings);

            if (ret)
                MSG(1, "Failed to exec piper: %d\n", ret);

            free(p_cmd->data);
            free(p_cmd);
        }
    }

    return NULL;
}

const char* piper_get_voice(SPDMsgSettings* p_settings)
{
    DBG("p_settings->voice.name: %s\n", p_settings->voice.name);
    DBG("p_settings->voice.language: %s\n", p_settings->voice.language);
    DBG("p_settings->voice.variant: %s\n", p_settings->voice.variant);
    DBG("p_settings->voice_type: %d\n", p_settings->voice_type);

    const char* voice_name = module_getdefaultvoice();

    if (p_settings->voice.name && module_existsvoice(p_settings->voice.name))
    {
        voice_name = p_settings->voice.name;

    } else
    {
        const char* voice_preset = module_getvoice(
                p_settings->voice.language,
                p_settings->voice_type);

        if (module_existsvoice(voice_preset))
            voice_name = voice_preset;
    }

    DBG("Using voice_name = %s\n", voice_name);
    return voice_name;
}

int piper_get_voice_sample_rate(const char* manifest_path)
{
    int rate = -1;
    FILE* p_file = NULL;
    char* json_buff = NULL;
    GRegex* regex = NULL;
    GMatchInfo* match_info = NULL;
    gchar* sample_rate_str = NULL;

    p_file = fopen(manifest_path, "r");
    if (!p_file)
        goto cleanup;

    fseek(p_file, 0, SEEK_END);
    size_t filesize = ftell(p_file);
    fseek(p_file, 0, SEEK_SET);

    json_buff = (char*)malloc(filesize + 1);
    if (!json_buff)
        goto cleanup;

    char* p_out = json_buff;
    while (p_out - json_buff < filesize)
    {
        p_out += fread(p_out, 1, filesize - (p_out - json_buff), p_file);
    }

    json_buff[filesize] = '\0';

    fclose(p_file);
    p_file = NULL;

    regex = g_regex_new("\"sample_rate\"\\s*:\\s*(\\d+)",
                    G_REGEX_CASELESS | G_REGEX_MULTILINE,
                    G_REGEX_MATCH_NEWLINE_ANY,
                    NULL);

    gboolean found = g_regex_match(regex, json_buff, 0, &match_info);

    if (!found || match_info == NULL)
    {
        MSG(1, "No sample rate info found in model manifest (%s)\n",
                manifest_path);

        rate = 22050; // most common sample rate
        goto cleanup;
    }

    sample_rate_str = g_match_info_fetch(match_info, 1);
    if (!sample_rate_str)
    {
        MSG(1, "GLib error: regex provided no match group\n");

        rate = 22050; // most common sample rate
        goto cleanup;
    }

    rate = atoi(sample_rate_str);

    DBG("Read sample_rate=%d from manifest (%s)\n", rate, manifest_path);

cleanup:
    if (p_file)
        fclose(p_file);
    if (sample_rate_str)
        g_free(sample_rate_str);
    if (match_info)
        g_match_info_free(match_info);
    if (regex)
        g_regex_unref(regex);
    if (json_buff)
        free(json_buff);

    return rate;
}

int piper_process_message(char** p_msg_out, size_t* p_msg_out_len,
        char* message, size_t message_len, SPDMessageType msg_type,
        SPDMsgSettings* p_settings)
{
    int ret = 0;
    char* stripped_text = NULL;
    size_t stripped_len = 0;

    switch (msg_type)
    {
        case SPD_MSGTYPE_TEXT:
            stripped_text = module_strip_ssml(message);
            if (!stripped_text)
                return ENOMEM;
            stripped_len = strlen(stripped_text);
            break;

        case SPD_MSGTYPE_CHAR:
            stripped_text = g_utf8_substring(message, 0, 1);
            stripped_len = strlen(stripped_text);
            break;

        case SPD_MSGTYPE_KEY:
        case SPD_MSGTYPE_SOUND_ICON:
        case SPD_MSGTYPE_SPELL:
        default:
            stripped_text = strdup(message);
            if (!stripped_text)
                return ENOMEM;
            stripped_len = message_len;
            break;
    }

    if (msg_type == SPD_MSGTYPE_SPELL ||
            p_settings->spelling_mode == SPD_SPELL_ON)
    {
        *p_msg_out_len = 2 * stripped_len + 1;
        *p_msg_out = calloc(1, *p_msg_out_len);
        if (!*p_msg_out)
        {
            ret = ENOMEM;
            goto cleanup;
        }

        char* cur_char = stripped_text;
        char* p_out = *p_msg_out;

        while (cur_char < stripped_text + stripped_len)
        {
            char* next_char = g_utf8_find_next_char(cur_char, NULL);

            size_t char_len = next_char - cur_char;
            if (char_len > 4)
            {
                ret = EINVAL;
                goto cleanup;
            }

            memcpy(p_out, cur_char, char_len);
            p_out += char_len;

            gunichar cur_unichar = g_utf8_get_char(cur_char);
            GUnicodeType char_type = g_unichar_type(cur_unichar);

            if (char_type != G_UNICODE_LINE_SEPARATOR &&
                    char_type != G_UNICODE_PARAGRAPH_SEPARATOR &&
                    char_type != G_UNICODE_SPACE_SEPARATOR &&
                    next_char < stripped_text + stripped_len)
            {
                *p_out++ = ' ';
            }

            cur_char = next_char;
        }

        assert(p_out <= *p_msg_out + *p_msg_out_len);

        free(stripped_text);
        stripped_text = NULL;

    } else
    {
        *p_msg_out = stripped_text;
        *p_msg_out_len = stripped_len;
        stripped_text = NULL;
    }

cleanup:
    if (stripped_text)
        free(stripped_text);

    return ret;
}

int piper_exec(char* data, size_t len, SPDMessageType msg_type,
        SPDMsgSettings* p_settings)
{
    int ret = 0;

    int pipe_in[2] = {-1, -1};
    int pipe_out[2] = {-1, -1};
    char* audio_buf = NULL;
    char* stripped_text = NULL;
    size_t stripped_len = 0;
    int child_pid = -1;

    DBG("Message is %lu bytes\n", len);

    ret = piper_process_message(&stripped_text, &stripped_len,
            data, len, msg_type, p_settings);
    if (ret)
    {
        MSG(1, "Failed to process message: %d\n", ret);
        goto cleanup;
    }

    DBG("Processed message, %lu bytes to send to piper\n", stripped_len);

    if (stripped_len == 0)
    {
        ret = 0;
        goto cleanup;
    }

    const char* voice_name = piper_get_voice(p_settings);
    if (!voice_name)
    {
        MSG(1, "No valid voice model was found. "
               "Configure available voice models using PiperModelsPath and "
               "AddVoice config options\n");
        ret = 1;
        goto cleanup;
    }

    char model_path[1024] = {0};
    char manifest_path[1024] = {0};

    snprintf(model_path, sizeof(model_path) - 1, "%s/%s",
            PiperModelsPath, voice_name);

    snprintf(manifest_path, sizeof(manifest_path) - 1, "%s/%s.json",
            PiperModelsPath, voice_name);

    int sample_rate = piper_get_voice_sample_rate(manifest_path);
    if (sample_rate <= 0)
    {
        MSG(1, "Error reading sample rate [errno: %d]\n", errno);
        MSG(1, "Using sample_rate=22050\n");
        sample_rate = 22050;
    }

    size_t audio_buf_len = 2 * sample_rate * PiperAudioBufferMsec / 1000;
    audio_buf = calloc(1, audio_buf_len);
    if (!audio_buf)
    {
        ret = ENOMEM;
        goto cleanup;
    }

    ret = pipe(pipe_out);
    if (ret)
    {
        MSG(1, "pipe() failed with code %d", ret);
        goto cleanup;
    }

    ret = pipe(pipe_in);
    if (ret)
    {
        MSG(1, "pipe() failed with code %d", ret);
        goto onerror;
    }

    child_pid = fork();
    if (child_pid == -1)
    {
        MSG(1, "Failed to fork");
        ret = 1;
        goto onerror;

    } else if (child_pid == 0)
    {
        /* child */
        dup2(pipe_in[0], STDIN_FILENO);
        close(pipe_in[0]);
        close(pipe_in[1]);

        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_out[0]);
        close(pipe_out[1]);

        char length_scale_str[10] = {0};
        char noise_scale_str[10] = {0};
        char noise_width_str[10] = {0};
        char sentence_silence_str[10] = {0};

        /* [-100, 100] -> [2.0, 0.5] */
        double length_scale = pow(2.0, (double)p_settings->rate / -100.0);

        snprintf(length_scale_str, sizeof(length_scale_str) - 1,
                "%.2f", length_scale);

        snprintf(noise_scale_str, sizeof(noise_scale_str) - 1,
                "%.3f", PiperNoiseScale);

        snprintf(noise_width_str, sizeof(noise_width_str) - 1,
                "%.3f", PiperNoiseWidth);

        snprintf(sentence_silence_str, sizeof(sentence_silence_str) - 1,
                "%.2f", PiperSentenceSilence);

        DBG("Executing: %s --model %s "
                "--length_scale=%s "
                "--noise_scale=%s "
                "--noise_w=%s "
                "--sentence_silence=%s "
                "--output_raw\n",
                PiperBinPath, model_path,
                length_scale_str,
                noise_scale_str,
                noise_width_str,
                sentence_silence_str
        );

        execl(PiperBinPath, PiperBinPath,
                "--model", model_path,
                "--length_scale", length_scale_str,
                "--noise_scale", noise_scale_str,
                "--noise_w", noise_width_str,
                "--sentence_silence", sentence_silence_str,
                "--output_raw", NULL);

        MSG(1, "execl failed with errno=%d\n", errno);
        exit(errno);

    } else
    {
        /* parent */
        DBG("piper process started [pid: %d]\n", child_pid);

        int* p_child_stdin = &pipe_in[1];
        int* p_child_stdout = &pipe_out[0];

        close(pipe_in[0]);
        close(pipe_out[1]);
        pipe_in[0] = -1;
        pipe_out[1] = -1;

        DBG("Writing %lu bytes to piper\n", stripped_len);

        const char* p_data = stripped_text;
        while ((p_data - stripped_text) < stripped_len)
        {
            int n = write(*p_child_stdin, p_data,
                    stripped_len - (p_data - stripped_text));

            if (n < 0)
            {
                if (errno == EINTR)
                    continue;

                ret = errno;
                goto onerror;
            }

            p_data += n;
        }

        DBG("Wrote %lu bytes to piper\n", stripped_len);

        close(*p_child_stdin);
        *p_child_stdin = -1;

        module_report_event_begin();

        AudioTrack track = {
            .bits = 16,
            .sample_rate = sample_rate,
            .num_channels = 1
        };

        AudioFormat format = SPD_AUDIO_LE;

        DBG("Streaming audio data from piper to server\n");

        char* p_buf = audio_buf;
        size_t total_samples = 0;
        int n = 0;

        do
        {
            n = read(*p_child_stdout, p_buf,
                    audio_buf_len - (p_buf - audio_buf));

            if (n < 0)
                continue;

            p_buf += n;

            if (n == 0 || p_buf >= audio_buf + audio_buf_len)
            {
                track.num_samples = (p_buf - audio_buf) / 2;
                track.samples = (signed short*)audio_buf;

                if (track.num_samples > 0)
                {
                    module_tts_output_server(&track, format);
                }

                total_samples += track.num_samples;
                p_buf = audio_buf;
            }

        } while (!_synth_stop && (n > 0 || (n < 0 && errno == EINTR)));

        if (_synth_stop)
        {
            DBG("Server sent %s message. Wrote %lu samples to server\n",
                    _synth_pause ? "PAUSE" : "STOP",
                    total_samples);

            ret = 0;

            if (_synth_pause)
                module_report_event_pause();
            else
                module_report_event_stop();

            goto onerror;
        }

        if (total_samples == 0)
        {
            DBG("Failed to read any samples from piper, read errno: %d\n",
                    errno);
        } else
        {
            DBG("Wrote %lu samples to server\n", total_samples);
        }

        module_report_event_end();

        int wstatus = 0;
        waitpid(child_pid, &wstatus, 0);
        child_pid = -1;

        if (WIFEXITED(wstatus))
        {
            DBG("piper exited with return code %d\n", WEXITSTATUS(wstatus));
        } else
        {
            DBG("piper exited abnormally\n");
        }

        close(*p_child_stdout);
        *p_child_stdout = -1;
    }

    goto cleanup;

onerror:
    if (pipe_in[0] != -1)
        close(pipe_in[0]);
    if (pipe_in[1] != -1)
        close(pipe_in[1]);
    if (pipe_out[0] != -1)
        close(pipe_out[0]);
    if (pipe_out[1] != -1)
        close(pipe_out[1]);

    if (child_pid >= 0)
    {
        int wstatus = 0;
        int reaped_pid = waitpid(child_pid, &wstatus, WNOHANG);

        if (!reaped_pid || (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus)))
        {
            DBG("Sending SIGKILL to piper [pid: %d]\n", child_pid);
            kill(child_pid, SIGKILL);
        }

        // call this even when child has exited to avoid zombie pid
        DBG("waiting for child process (blocking) [pid: %d]\n", child_pid);
        waitpid(child_pid, &wstatus, 0);
    }

cleanup:
    if (stripped_text)
        free(stripped_text);
    if (audio_buf)
        free(audio_buf);

    return ret;
}

