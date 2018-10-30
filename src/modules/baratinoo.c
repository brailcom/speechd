/*
 * baratinoo.c - Speech Dispatcher backend for Baratinoo (VoxyGen)
 *
 * Copyright (C) 2016 Brailcom, o.p.s.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*
 * Input and output choices.
 *
 * - The input is sent to the engine through a BCinputTextBuffer.  There is
 *   a single one of those at any given time, and it is filled in
 *   module_speak() and consumed in the synthesis thread.
 *
 *   This doesn't use an input callback generating a continuous flow (and
 *   blocking waiting for more data) even though it would be a fairly nice
 *   design and would allow not to set speech attributes like volume, pitch and
 *   rate as often.  This is because the Baratinoo engine has 2 limitations on
 *   the input callback:
 *
 *   * It consumes everything (or at least a lot) up until the callbacks
 *     reports the input end by returning 0.  Alternatively one could use the
 *     \flush command followed by a newline, so this is not really limiting.
 *
 *   * More problematic, as the buffer callback is expected to feed a single
 *     input, calling BCpurge() (for handling stop events) unregisters it,
 *     requiring to re-add it afterward.  This renders the continuous flow a
 *     lot less useful, as speech attributes like volume, pitch and rate would
 *     have to be set again.
 *
 * - The output uses the signal buffer instead of callback.
 * The output callback sends sound to the output module phonem by
 * phonem, which cause noise parasits with ALSA due to a reset of
 * parameters for each sound call.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <semaphore.h>

#define BARATINOO_C_API
#include "baratinoo.h"
#include "baratinooio.h"

#include "spd_audio.h"

#include <speechd_types.h>

#include "module_utils.h"

#define MODULE_NAME     "baratinoo"
#define DBG_MODNAME     "Baratinoo: "
#define MODULE_VERSION  "0.1"

#define DEBUG_MODULE 1
DECLARE_DEBUG();

typedef struct {
	/* Thread primitives */
	pthread_t thread;
	sem_t semaphore;

	BCengine engine;
	/* The buffer consumed by the TTS engine.  It is NULL when the TTS
	 * thread is ready to accept new input.  Otherwise, the thread is in
	 * the process of synthesizing speech. */
	BCinputTextBuffer buffer;
        /* The output signal */
        BCoutputSignalBuffer output_signal;

	SPDVoice **voice_list;

	/* settings */
	int voice;

	/* request flags */
	gboolean pause_requested;
	gboolean stop_requested;
	gboolean close_requested;

	SPDMarks marks;
} Engine;

/* engine and state */
static Engine baratinoo_engine = {
	.engine = NULL,
	.buffer = NULL,
	.output_signal = NULL,
	.voice_list = NULL,
	.voice = 0,
	.pause_requested = FALSE,
	.stop_requested = FALSE,
	.close_requested = FALSE
};

/* Internal functions prototypes */
static void *_baratinoo_speak(void *);
static SPDVoice **baratinoo_list_voices(BCengine *engine);
/* Parameters */
static void baratinoo_set_voice_type(SPDVoiceType voice);
static void baratinoo_set_language(char *lang);
static void baratinoo_set_synthesis_voice(char *synthesis_voice);
/* Engine callbacks */
static void baratinoo_trace_cb(BaratinooTraceLevel level, int engine_num, const char *source, const void *data, const char *format, va_list args);
static int baratinoo_output_signal(void *privateData, const void *address, int length);
/* SSML conversion functions */
static void append_ssml_as_proprietary(const Engine *engine, GString *buf, const char *data, gsize size);

/* Module configuration options */
MOD_OPTION_1_STR(BaratinooConfigPath);
MOD_OPTION_1_INT(BaratinooSampleRate);
MOD_OPTION_1_INT(BaratinooMinRate);
MOD_OPTION_1_INT(BaratinooNormalRate);
MOD_OPTION_1_INT(BaratinooMaxRate);
MOD_OPTION_1_STR(BaratinooPunctuationList);
MOD_OPTION_1_STR(BaratinooIntonationList);
MOD_OPTION_1_STR(BaratinooNoIntonationList);

/* Public functions */

int module_load(void)
{
	const char *conf_env;
	char *default_config = NULL;

	INIT_SETTINGS_TABLES();

	REGISTER_DEBUG();

	/* BaratinooConfigPath default value comes from the environment or
	 * user XDG configuration location */
	conf_env = getenv("BARATINOO_CONFIG_PATH");
	if (conf_env && conf_env[0] != '\0') {
		default_config = g_strdup(conf_env);
	} else {
		default_config = g_build_filename(g_get_user_config_dir(),
						  "baratinoo.cfg", NULL);
	}
	MOD_OPTION_1_STR_REG(BaratinooConfigPath, default_config);
	g_free(default_config);

	/* Sample rate. 16000Hz is the voices default, not requiring resampling */
	MOD_OPTION_1_INT_REG(BaratinooSampleRate, 16000);

	/* Speech rate */
	MOD_OPTION_1_INT_REG(BaratinooMinRate, -100);
	MOD_OPTION_1_INT_REG(BaratinooNormalRate, 0);
	MOD_OPTION_1_INT_REG(BaratinooMaxRate, 100);

	/* Punctuation */
	MOD_OPTION_1_STR_REG(BaratinooPunctuationList, "@/+-_");
	MOD_OPTION_1_STR_REG(BaratinooIntonationList, "?!;:,.");
	MOD_OPTION_1_STR_REG(BaratinooNoIntonationList, "");

	return 0;
}

int module_init(char **status_info)
{
	Engine *engine = &baratinoo_engine;
	int ret;
	BARATINOOC_STATE state;

	DBG(DBG_MODNAME "Module init");
	INIT_INDEX_MARKING();

	DBG(DBG_MODNAME "BaratinooPunctuationList = %s", BaratinooPunctuationList);
	DBG(DBG_MODNAME "BaratinooIntonationList = %s", BaratinooIntonationList);
	DBG(DBG_MODNAME "BaratinooNoIntonationList = %s", BaratinooNoIntonationList);

	*status_info = NULL;

	engine->pause_requested = FALSE;
	engine->stop_requested = FALSE;
	engine->close_requested = FALSE;

	/* Init Baratinoo */
	if (BCinitlib(baratinoo_trace_cb) != BARATINOO_INIT_OK) {
		DBG(DBG_MODNAME "Failed to initialize library");
		*status_info = g_strdup("Failed to initialize Baratinoo. "
					"Make sure your installation is "
					"properly set up.");
		return -1;
	}
	DBG(DBG_MODNAME "Using Baratinoo %s", BCgetBaratinooVersion());

	engine->engine = BCnew(NULL);
	if (!engine->engine) {
		DBG(DBG_MODNAME "Failed to allocate engine");
		*status_info = g_strdup("Failed to create Baratinoo engine.");
		return -1;
	}

	BCinit(engine->engine, BaratinooConfigPath);
	state = BCgetState(engine->engine);
	if (state != BARATINOO_INITIALIZED) {
		DBG(DBG_MODNAME "Failed to initialize engine");
		*status_info = g_strdup("Failed to initialize Baratinoo engine. "
					"Make sure your setup is OK.");
		return -1;
	}

	/* Find voices */
	engine->voice_list = baratinoo_list_voices(engine->engine);
	if (!engine->voice_list) {
		DBG(DBG_MODNAME "No voice available");
		*status_info = g_strdup("No voice found. Make sure your setup "
					"includes at least one voice.");
		return -1;
	}

	/* Setup output (audio) signal handling */
	DBG(DBG_MODNAME "Using PCM output at %dHz", BaratinooSampleRate);
	engine->output_signal = BCoutputSignalBufferNew(BARATINOO_PCM, BaratinooSampleRate);
	if (!engine->output_signal) {
		  DBG(DBG_MODNAME "Cannot allocate BCoutputSignalBufferNew");
		  return -1;
	}
	if (BCgetState(engine->engine) != BARATINOO_INITIALIZED) {
		DBG(DBG_MODNAME "Failed to initialize output signal handler");
		*status_info = g_strdup("Failed to initialize Baratinoo output "
					"signal handler. Is the configured "
					"sample rate correct?");
		return -1;
	}
	BCoutputTextBufferSetInEngine(engine->output_signal, engine->engine);

	BCsetWantedEvent(engine->engine, BARATINOO_WAITMARKER_EVENT);

	/* Setup TTS thread */
	sem_init(&engine->semaphore, 0, 0);

	DBG(DBG_MODNAME "creating new thread for baratinoo_speak");
	ret = pthread_create(&engine->thread, NULL, _baratinoo_speak, engine);
	if (ret != 0) {
		DBG(DBG_MODNAME "thread creation failed");
		*status_info =
		    g_strdup("The module couldn't initialize threads. "
			     "This could be either an internal problem or an "
			     "architecture problem. If you are sure your architecture "
			     "supports threads, please report a bug.");
		return -1;
	}

	module_marks_init(&engine->marks);

	DBG(DBG_MODNAME "Initialization successfully.");
	*status_info = g_strdup("Baratinoo initialized successfully.");

	return 0;
}

SPDVoice **module_list_voices(void)
{
	Engine *engine = &baratinoo_engine;

	return engine->voice_list;
}

int module_speak(gchar *data, size_t bytes, SPDMessageType msgtype)
{
	Engine *engine = &baratinoo_engine;
	GString *buffer = NULL;
	int rate;

	DBG(DBG_MODNAME "Speech requested");

	assert(msg_settings.rate >= -100 && msg_settings.rate <= +100);
	assert(msg_settings.pitch >= -100 && msg_settings.pitch <= +100);
	assert(msg_settings.pitch_range >= -100 && msg_settings.pitch_range <= +100);
	assert(msg_settings.volume >= -100 && msg_settings.volume <= +100);

	if (engine->buffer != NULL) {
		DBG(DBG_MODNAME "WARNING: module_speak() called during speech");
		return 0;
	}

	engine->pause_requested = FALSE;
	engine->stop_requested = FALSE;

	/* select voice following parameters.  we don't use tags for this as
	 * we need to do some computation on our end anyway and need pass an
	 * ID when creating the buffer too */
	/* NOTE: these functions access the engine, which wouldn't be safe if
	 *       we didn't know that the thread is sleeping.  But we do know it
	 *       is, as @c Engine::buffer is NULL */
	UPDATE_STRING_PARAMETER(voice.language, baratinoo_set_language);
	UPDATE_PARAMETER(voice_type, baratinoo_set_voice_type);
	UPDATE_STRING_PARAMETER(voice.name, baratinoo_set_synthesis_voice);

	engine->buffer = BCinputTextBufferNew(BARATINOO_PROPRIETARY_PARSING,
					      BARATINOO_UTF8, engine->voice, 0);
	if (!engine->buffer) {
		DBG(DBG_MODNAME "Failed to allocate input buffer");
		goto err;
	}

	buffer = g_string_new(NULL);

	/* Apply speech parameters */
	if (msg_settings.rate < 0)
		rate = BaratinooNormalRate + (BaratinooNormalRate - BaratinooMinRate) * msg_settings.rate / 100;
	else
		rate = BaratinooNormalRate + (BaratinooMaxRate - BaratinooNormalRate) * msg_settings.rate / 100;

	if (rate != 0) {
		g_string_append_printf(buffer, "\\rate{%+d%%}", rate);
	}
	if (msg_settings.pitch != 0 || msg_settings.pitch_range != 0) {
		g_string_append_printf(buffer, "\\pitch{%+d%% %+d%%}",
				       msg_settings.pitch,
				       msg_settings.pitch_range);
	}
	if (msg_settings.volume != 0) {
		g_string_append_printf(buffer, "\\volume{%+d%%}",
				       msg_settings.volume);
	}

	switch (msgtype) {
	case SPD_MSGTYPE_SPELL: /* FIXME: use \spell one day? */
	case SPD_MSGTYPE_CHAR:
		g_string_append(buffer, "\\sayas<{characters}");
		append_ssml_as_proprietary(engine, buffer, data, bytes);
		g_string_append(buffer, "\\sayas>{}");
		break;
	default: /* FIXME: */
	case SPD_MSGTYPE_TEXT:
		append_ssml_as_proprietary(engine, buffer, data, bytes);
		break;
	}

	DBG(DBG_MODNAME "SSML input: %s", data);
	DBG(DBG_MODNAME "Sending buffer: %s", buffer->str);
	if (!BCinputTextBufferInit(engine->buffer, buffer->str)) {
		DBG(DBG_MODNAME "Failed to initialize input buffer");
		goto err;
	}

	g_string_free(buffer, TRUE);

	sem_post(&engine->semaphore);

	DBG(DBG_MODNAME "leaving module_speak() normally");
	return bytes;

err:
	if (buffer)
		g_string_free(buffer, TRUE);
	if (engine->buffer) {
		BCinputTextBufferDelete(engine->buffer);
		engine->buffer = NULL;
	}

	return 0;
}

int module_stop(void)
{
	Engine *engine = &baratinoo_engine;

	DBG(DBG_MODNAME "Stop requested");
	engine->stop_requested = TRUE;
	module_marks_stop(&engine->marks);
	if (module_audio_id) {
		DBG(DBG_MODNAME "Stopping audio currently playing.");
		if (spd_audio_stop(module_audio_id) != 0)
			DBG(DBG_MODNAME "spd_audio_stop() returned non-zero value.");
	}

	return 0;
}

size_t module_pause(void)
{
	Engine *engine = &baratinoo_engine;

	DBG(DBG_MODNAME "Pause requested");
	engine->pause_requested = TRUE;
	module_marks_stop(&engine->marks);

	return 0;
}

int module_close(void)
{
	Engine *engine = &baratinoo_engine;

	DBG(DBG_MODNAME "close()");

	DBG(DBG_MODNAME "Terminating threads");

	/* Politely ask the thread to terminate */
	engine->stop_requested = TRUE;
	engine->close_requested = TRUE;
	module_marks_stop(&engine->marks);
	sem_post(&engine->semaphore);
	/* ...and give it a chance to actually quit. */
	g_usleep(25000);

	/* Make sure the thread has really exited */
	pthread_cancel(engine->thread);
	DBG(DBG_MODNAME "Joining threads.");
	if (pthread_join(engine->thread, NULL) != 0)
		DBG(DBG_MODNAME "Failed to join threads.");

	sem_destroy(&engine->semaphore);

	/* destroy voice list */
	if (engine->voice_list != NULL) {
		int i;
		for (i = 0; engine->voice_list[i] != NULL; i++) {
			g_free(engine->voice_list[i]->name);
			g_free(engine->voice_list[i]->language);
			g_free(engine->voice_list[i]->variant);
			g_free(engine->voice_list[i]);
		}
		g_free(engine->voice_list);
		engine->voice_list = NULL;
	}

	/* destroy output signal */
	BCoutputSignalBufferDelete(engine->output_signal);
	engine->output_signal = NULL;

	/* destroy engine */
	if (engine->engine) {
	    BCdelete(engine->engine);
	    engine->engine = NULL;
	}

	/* uninitialize */
	BCterminatelib();

	module_marks_clear(&engine->marks);

	DBG(DBG_MODNAME "Module closed.");

	return 0;
}

/* Internal functions */

/**
 * @brief Lists voices in SPD format
 * @param engine An engine.
 * @returns A NULL-terminated list of @c SPDVoice, or NULL if no voice found.
 */
static SPDVoice **baratinoo_list_voices(BCengine *engine)
{
    SPDVoice **voices;
    int n_voices;
    int i;

    n_voices = BCgetNumberOfVoices(engine);
    if (n_voices < 1)
	return NULL;

    voices = g_malloc_n(n_voices + 1, sizeof *voices);
    DBG(DBG_MODNAME "Got %d available voices:", n_voices);
    for (i = 0; i < n_voices; i++) {
	SPDVoice *voice;
	const char *dash;
	BaratinooVoiceInfo voice_info = BCgetVoiceInfo(engine, i);

	DBG(DBG_MODNAME "\tVoice #%d: name=%s, language=%s, gender=%s",
	    i, voice_info.name, voice_info.language, voice_info.gender);

	voice = g_malloc0(sizeof *voice);
	voice->name = g_strdup(voice_info.name);

	dash = strchr(voice_info.language, '-');
	if (dash) {
	    voice->language = g_strndup(voice_info.language,
					dash - voice_info.language);
	    voice->variant = g_ascii_strdown(dash + 1, -1);
	} else {
	    voice->language = g_strdup(voice_info.language);
	}

	voices[i] = voice;
    }
    voices[i] = NULL;

    return voices;
}

/**
 * @brief Internal TTS thread.
 * @param data An Engine structure.
 * @returns NULL.
 *
 * The TTS thread.  It waits on @c Engine::semaphore to consume input data
 * from @c Engine::buffer.
 *
 * @see Engine::pause_requested
 * @see Engine::stop_requested
 * @see Engine::close_requested
 */
static void *_baratinoo_speak(void *data)
{
	Engine *engine = data;
	BARATINOOC_STATE state;

	set_speaking_thread_parameters();

	while (!engine->close_requested) {
		sem_wait(&engine->semaphore);
		DBG(DBG_MODNAME "Semaphore on");
		engine->stop_requested = FALSE;

		if (!engine->buffer)
			continue;

		state = BCinputTextBufferSetInEngine(engine->buffer, engine->engine);
		if (state != BARATINOO_READY) {
			DBG(DBG_MODNAME "Failed to set input buffer");
			continue;
		}

		module_report_event_begin();
		while (1) {
			if (engine->stop_requested || engine->close_requested) {
				DBG(DBG_MODNAME "Stop in child, terminating");
				BCinputTextBufferDelete(engine->buffer);
				engine->buffer = NULL;
				module_report_event_stop();
				break;
			}

			do {
				state = BCprocessLoop(engine->engine, -1);
				if (state == BARATINOO_EVENT) {
					BaratinooEvent event = BCgetEvent(engine->engine);
					if (event.type == BARATINOO_WAITMARKER_EVENT) {
						DBG(DBG_MODNAME "Reached wait mark '%s' time %f samples %d", event.data.waitMarker.name, event.data.waitMarker.duration, event.data.waitMarker.samples);
						module_marks_add(&engine->marks, event.data.waitMarker.samples, event.data.waitMarker.name);
						/* if reached a spd mark and pausing requested, stop */
						if (engine->pause_requested &&
						    g_str_has_prefix(event.data.waitMarker.name, INDEX_MARK_BODY)) {
							DBG(DBG_MODNAME "Pausing in thread");
							state = BCpurge(engine->engine);
							engine->pause_requested = FALSE;
							module_report_event_pause();
						}
					}
				} else if (state == BARATINOO_INPUT_ERROR ||
					   state == BARATINOO_ENGINE_ERROR) {
					/* CANCEL would be better I guess, but
					 * that's good enough */
					module_report_event_stop();
				}
			} while (state == BARATINOO_RUNNING || state == BARATINOO_EVENT);

			BCinputTextBufferDelete(engine->buffer);
			engine->buffer = NULL;

			DBG(DBG_MODNAME "Trying to synthesize text");
			if (BCoutputSignalBufferIsError(engine->output_signal) || engine->close_requested) {
				DBG(DBG_MODNAME "Error with the output signal");
				BCoutputSignalBufferResetSignal(engine->output_signal);
				module_report_event_stop();
			} else {
				baratinoo_output_signal(engine, BCoutputSignalBufferGetSignalBuffer(engine->output_signal), BCoutputSignalBufferGetSignalLength(engine->output_signal));
				BCoutputSignalBufferResetSignal(engine->output_signal);
				if (engine->stop_requested || engine->close_requested) {
					DBG(DBG_MODNAME "Stop in child, terminating");
					module_report_event_stop();
				} else {
					module_report_event_end();
				}
			}
			break;
		}
		engine->stop_requested = FALSE;
	}

	DBG(DBG_MODNAME "leaving thread with state=%d", state);

	pthread_exit(NULL);
}

/* Voice selection */

/**
 * @brief Matches a Baratinoo voice info against a SPD language
 * @param info A voice info to match.
 * @param lang A SPD language to match against.
 * @returns The quality of the match: the higher the better.
 *
 * Gives a score to a voice based on its compatibility with @p lang.
 */
static int lang_match_level(const BaratinooVoiceInfo *info, const char *lang)
{
	int level = 0;

	if (g_ascii_strcasecmp(lang, info->language) == 0)
		level += 10;
	else {
		gchar **a = g_strsplit_set(info->language, "-", 2);
		gchar **b = g_strsplit_set(lang, "-", 2);

		/* language */
		if (g_ascii_strcasecmp(a[0], b[0]) == 0)
			level += 8;
		else if (g_ascii_strcasecmp(info->iso639, b[0]) == 0)
			level += 8;
		else if (g_ascii_strncasecmp(a[0], b[0], 2) == 0)
			level += 5; /* partial match */
		/* region */
		if (a[1] && b[1] && g_ascii_strcasecmp(a[1], b[1]) == 0)
			level += 2;
		else if (b[1] && g_ascii_strcasecmp(info->iso3166, b[1]) == 0)
			level += 2;
		else if (a[1] && b[1] && g_ascii_strncasecmp(a[1], b[1], 2) == 0)
			level += 1; /* partial match */

		g_strfreev(a);
		g_strfreev(b);
	}

	DBG(DBG_MODNAME "lang_match_level({language=%s, iso639=%s, iso3166=%s}, lang=%s) = %d",
	    info->language, info->iso639, info->iso3166, lang, level);

	return level;
}

/**
 * @brief Sort two Baratinoo voices by SPD criteria.
 * @param a A voice info.
 * @param b Another voice info.
 * @param lang A SPD language.
 * @param voice_code A SPD voice code.
 * @returns < 0 if @p a is best, > 0 if @p b is best, and 0 if they are equally
 *          matching.  Larger divergence from 0 means better match.
 */
static int sort_voice(const BaratinooVoiceInfo *a, const BaratinooVoiceInfo *b, const char *lang, SPDVoiceType voice_code)
{
	int cmp = 0;

	cmp -= lang_match_level(a, lang);
	cmp += lang_match_level(b, lang);

	if (strcmp(a->gender, b->gender) != 0) {
		const char *gender;

		switch (voice_code) {
		default:
		case SPD_MALE1:
		case SPD_MALE2:
		case SPD_MALE3:
		case SPD_CHILD_MALE:
			gender = "male";
			break;

		case SPD_FEMALE1:
		case SPD_FEMALE2:
		case SPD_FEMALE3:
		case SPD_CHILD_FEMALE:
			gender = "female";
			break;
		}

		if (strcmp(gender, a->gender) == 0)
			cmp--;
		if (strcmp(gender, b->gender) == 0)
			cmp++;
	}

	switch (voice_code) {
	case SPD_CHILD_MALE:
	case SPD_CHILD_FEMALE:
		if (a->age && a->age <= 15)
			cmp--;
		if (b->age && b->age <= 15)
			cmp++;
		break;
	default:
		/* we expect mostly adult voices, so only compare if age is set */
		if (a->age && b->age) {
			if (a->age > 15)
				cmp--;
			if (b->age > 15)
				cmp++;
		}
		break;
	}

	DBG(DBG_MODNAME "Comparing %s <> %s gives %d", a->name, b->name, cmp);

	return cmp;
}

/* Given a language code and SD voice code, gets the Baratinoo voice. */
static int baratinoo_find_voice(const Engine *engine, const char *lang, SPDVoiceType voice_code)
{
	int i;
	int best_match = -1;
	int nth_match = 0;
	int offset = 0; /* nth voice we'd like */
	BaratinooVoiceInfo best_info;

	DBG(DBG_MODNAME "baratinoo_find_voice(lang=%s, voice_code=%d)",
	    lang, voice_code);

	switch (voice_code) {
	case SPD_MALE3:
	case SPD_FEMALE3:
		offset++;
	case SPD_MALE2:
	case SPD_FEMALE2:
		offset++;
	default:
		break;
	}

	for (i = 0; i < BCgetNumberOfVoices(engine->engine); i++) {
		if (i == 0) {
			best_match = i;
			best_info = BCgetVoiceInfo(engine->engine, i);
			nth_match++;
		} else {
			BaratinooVoiceInfo info = BCgetVoiceInfo(engine->engine, i);
			int cmp = sort_voice(&best_info, &info, lang, voice_code);

			if (cmp >= 0) {
				if (cmp > 0)
					nth_match = 0;
				if (nth_match <= offset) {
					best_match = i;
					best_info = info;
				}
				nth_match++;
			}
		}
	}

	return best_match;
}

/* Given a language code and SD voice code, sets the voice. */
static void baratinoo_set_language_and_voice(Engine *engine, const char *lang, SPDVoiceType voice_code)
{
	int voice = baratinoo_find_voice(engine, lang, voice_code);

	if (voice < 0) {
		DBG(DBG_MODNAME "No voice match found, not changing voice.");
	} else {
		DBG(DBG_MODNAME "Best voice match is %d.", voice);
		engine->voice = voice;
	}
}

/* UPDATE_PARAMETER callback to set the voice type */
static void baratinoo_set_voice_type(SPDVoiceType voice)
{
	Engine *engine = &baratinoo_engine;

	assert(msg_settings.voice.language);
	baratinoo_set_language_and_voice(engine, msg_settings.voice.language, voice);
}

/* UPDATE_PARAMETER callback to set the voice language */
static void baratinoo_set_language(char *lang)
{
	Engine *engine = &baratinoo_engine;

	baratinoo_set_language_and_voice(engine, lang, msg_settings.voice_type);
}

/* UPDATE_PARAMETER callback to set the voice by name */
static void baratinoo_set_synthesis_voice(char *synthesis_voice)
{
	Engine *engine = &baratinoo_engine;
	int i;

	if (synthesis_voice == NULL)
		return;

	for (i = 0; i < BCgetNumberOfVoices(engine->engine); i++) {
		BaratinooVoiceInfo info = BCgetVoiceInfo(engine->engine, i);

		if (g_ascii_strcasecmp(synthesis_voice, info.name) == 0) {
			engine->voice = i;
			return;
		}
	}

	DBG(DBG_MODNAME "Failed to set synthesis voice to '%s': not found.",
	    synthesis_voice);
}

/* Engine callbacks */

/**
 * @brief Logs a message from Baratinoo
 * @param level Message importance.
 * @param engine_num ID of the engine that emitted the message, or 0 if it is a
 *                   library message.
 * @param source Message category.
 * @param data Private data, unused.
 * @param format printf-like @p format.
 * @param args arguments for @p format.
 */
static void baratinoo_trace_cb(BaratinooTraceLevel level, int engine_num, const char *source, const void *data, const char *format, va_list args)
{
	const char *prefix = "";

	if (!Debug) {
		switch (level) {
		case BARATINOO_TRACE_INIT:
		case BARATINOO_TRACE_INFO:
		case BARATINOO_TRACE_DEBUG:
			return;
		default:
			break;
		}
	}

	switch (level) {
	case BARATINOO_TRACE_ERROR:
		prefix = "ERROR";
		break;
	case BARATINOO_TRACE_INIT:
		prefix = "INIT";
		break;
	case BARATINOO_TRACE_WARNING:
		prefix = "WARNING";
		break;
	case BARATINOO_TRACE_INFO:
		prefix = "INFO";
		break;
	case BARATINOO_TRACE_DEBUG:
		prefix = "DEBUG";
		break;
	}

	if (engine_num == 0)
		fprintf(stderr, "Baratinoo library: ");
	else
		fprintf(stderr, "Baratinoo engine #%d: ", engine_num);
	fprintf(stderr, "%s: %s ", prefix, source);
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
}

/**
 * @brief Output (sound) callback
 * @param private_data An Engine structure.
 * @param address Audio samples.
 * @param length Length of @p address, in bytes.
 * @returns Whether to break out of the process loop.
 *
 * Called by the engine during speech synthesis.
 *
 * @see BCprocessLoop()
 */
static int baratinoo_output_signal(void *private_data, const void *address, int length)
{
	Engine *engine = private_data;
	AudioTrack track;
#if defined(BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)
	AudioFormat format = SPD_AUDIO_BE;
#else
	AudioFormat format = SPD_AUDIO_LE;
#endif

	/* If stop is requested during synthesis, abort here to stop speech as
	 * early as possible, even if the engine didn't finish its cycle yet. */
	if (engine->stop_requested) {
		DBG(DBG_MODNAME "Not playing message because it got stopped");
		return engine->stop_requested;
	}

	/* We receive 16 bits PCM data */
	track.num_samples = length / 2; /* 16 bits per sample = 2 bytes */
	track.num_channels = 1;
	track.sample_rate = BaratinooSampleRate;
	track.bits = 16;
	track.samples = (short *) address;

	DBG(DBG_MODNAME "Playing part of the message");
	if (module_tts_output_marks(track, format, &engine->marks) < 0)
		DBG(DBG_MODNAME "ERROR: failed to play the track");

	module_marks_clear(&engine->marks);

	return engine->stop_requested;
}

/* SSML conversion functions */

typedef struct {
	const Engine *engine;
	GString *buffer;
	/* Voice ID stack for the current element */
	int voice_stack[32];
	unsigned int voice_stack_len;
} SsmlPraserState;

/* Adds a language change command for @p lang if appropriate */
static void ssml2baratinoo_push_lang(SsmlPraserState *state, const char *lang)
{
	int voice;

	if (state->voice_stack_len > 0)
		voice = state->voice_stack[state->voice_stack_len - 1];
	else
		voice = state->engine->voice;

	if (lang) {
		DBG(DBG_MODNAME "Processing xml:lang=\"%s\"", lang);
		int new_voice = baratinoo_find_voice(&baratinoo_engine, lang,
						     msg_settings.voice_type);
		if (new_voice >= 0 && new_voice != voice) {
			g_string_append_printf(state->buffer, "\\vox{%d}", new_voice);
			voice = new_voice;
		}
	}

	if (state->voice_stack_len >= G_N_ELEMENTS(state->voice_stack)) {
		DBG(DBG_MODNAME "WARNING: voice stack exhausted, expect incorrect voices.");
	} else {
		state->voice_stack[state->voice_stack_len++] = voice;
	}
}

/* Pops a language pushed with @c ssml2baratinoo_push_lang() */
static void ssml2baratinoo_pop_lang(SsmlPraserState *state)
{
	if (state->voice_stack_len > 0) {
		int cur_voice = state->voice_stack[--state->voice_stack_len];

		if (state->voice_stack_len > 0) {
			int new_voice = state->voice_stack[state->voice_stack_len - 1];

			if (new_voice != cur_voice)
				g_string_append_printf(state->buffer, "\\vox{%d}", new_voice);
		}
	}
}

/* locates a string in a NULL-terminated array of strings
 * Returns -1 if not found, the index otherwise. */
static int attribute_index(const char **names, const char *name)
{
	int i;

	for (i = 0; names && names[i] != NULL; i++) {
		if (strcmp(names[i], name) == 0)
			return i;
	}

	return -1;
}

/* Markup element start callback */
static void ssml2baratinoo_start_element(GMarkupParseContext *ctx,
					 const gchar *element,
					 const gchar **attribute_names,
					 const gchar **attribute_values,
					 gpointer data, GError **error)
{
	SsmlPraserState *state = data;
	int lang_id;

	/* handle voice changes */
	lang_id = attribute_index(attribute_names, "xml:lang");
	ssml2baratinoo_push_lang(state, lang_id < 0 ? NULL : attribute_values[lang_id]);

	/* handle elements */
	if (strcmp(element, "mark") == 0) {
		char *wait_mark;
		int i = attribute_index(attribute_names, "name");
		const char *mark = i < 0 ? "" : attribute_values[i];

		asprintf(&wait_mark, "\\mark{%s wait}", mark);
		g_string_prepend(state->buffer, wait_mark);
		free(wait_mark);
		g_string_append_printf(state->buffer, "\\mark{%s}", mark);
	} else if (strcmp(element, "emphasis") == 0) {
		int i = attribute_index(attribute_names, "level");
		g_string_append_printf(state->buffer, "\\emph<{%s}",
				       i < 0 ? "" : attribute_values[i]);
	} else {
		/* ignore other elements */
		/* TODO: handle more elements */
	}
}

/* Markup element end callback */
static void ssml2baratinoo_end_element(GMarkupParseContext *ctx,
				       const gchar *element,
				       gpointer data, GError **error)
{
	SsmlPraserState *state = data;

	if (strcmp(element, "emphasis") == 0) {
		g_string_append(state->buffer, "\\emph>{}");
	}

	ssml2baratinoo_pop_lang(state);
}

/* Markup text node callback.
 *
 * This not only converts to the proprietary format (by escaping things that
 * would be interpreted by it), but also pre-processes the text for some
 * features that are missing from Baratinoo.
 *
 * - Punctuation speaking
 *
 * As the engine doesn't support speaking of the punctuation itself, we alter
 * the input to explicitly tell the engine to do it.  It is kind of tricky,
 * because we want to keep the punctuation meaning of the characters, e.g. how
 * they affect speech as means of intonation and pauses.
 *
 * The approach here is that for every punctuation character included in the
 * selected mode (none/some/all), we wrap it in "\sayas<{characters}" markup
 * so that it is spoken by the engine.  But in order to keep the punctuation
 * meaning of the character, in case it has some, we duplicate it outside the
 * markup with a heuristic on whether it will or not affect speech intonation
 * and pauses, and whether or not the engine would speak the character itself
 * already (as we definitely don't want to get duplicated speech for a
 * character).
 * This heuristic is as follows:
 *   - If the character is listed in BaratinooIntonationList and the next
 *     character is not punctuation or alphanumeric, duplicate the character.
 *   - Always append a space after a duplicated character, hoping the engine
 *     won't consider speaking it.
 *
 * This won't always give the same results as the engine would by itself, but
 * it isn't really possible as the engine behavior is language-dependent in a
 * non-obvious fashion.  For example, a French voice will speak "1.2.3" as
 * "Un. Deux. Trois", while an English one will speak it as "One dot two dot
 * three": the dot here didn't have the same interpretation, and wasn't spoken
 * the same (once altering the voice, the other spoken plain and simple).
 *
 * However, the heuristic here should be highly unlikely to lead to duplicate
 * character speaking, and catch most of the intonation and pause cases.
 *
 * - Why is this done that way?
 *
 * Another, possibly more robust, approach could be using 2 passes in the
 * engine itself, and relying on events to get information on how the engine
 * interprets the input in the first (silent) pass, and alter it as needed for
 * a second (spoken) pass.  This wouldn't guarantee the altered input would be
 * interpreted the same, but it would seem like a safe enough bet.
 *
 * However, the engine is too slow for this to be viable in a real-time
 * processing environment for anything but tiny input.  Even about 25 lines of
 * IRC conversation can easily take several seconds to process in the first
 * pass (even without doing any actual pre-processing on our end), delaying
 * the actual speech by an unacceptable amount of time.
 *
 * Ideally, the engine will some day support speaking punctuation itself, and
 * this part of the pre-processing could be dropped.
 */
static void ssml2baratinoo_text(GMarkupParseContext *ctx,
				const gchar *text, gsize len,
				gpointer data, GError **error)
{
	SsmlPraserState *state = data;
	const gchar *p;

	for (p = text; p < (text + len); p = g_utf8_next_char(p)) {
		if (*p == '\\') {
			/* escape the \ by appending a comment so it won't be
			 * interpreted as a command */
			g_string_append(state->buffer, "\\\\{}");
		} else {
			gboolean say_as_char, do_not_say;
			gunichar ch = g_utf8_get_char(p);

			/* if punctuation mode is not NONE and the character
			 * should be spoken, manually wrap it with \sayas */
			say_as_char = ((msg_settings.punctuation_mode == SPD_PUNCT_SOME &&
					g_utf8_strchr(BaratinooPunctuationList, -1, ch)) ||
				       (msg_settings.punctuation_mode == SPD_PUNCT_ALL &&
					g_unichar_ispunct(ch)));
			do_not_say = ((msg_settings.punctuation_mode == SPD_PUNCT_NONE &&
					g_utf8_strchr(BaratinooNoIntonationList, -1, ch)));

			if (say_as_char)
				g_string_append(state->buffer, "\\sayas<{characters}");
			if (!do_not_say)
				g_string_append_unichar(state->buffer, ch);
			if (say_as_char) {
				g_string_append(state->buffer, "\\sayas>{}");

				/* if the character should influence intonation,
				 * add it back, but *only* if it wouldn't be spoken */
				if (g_utf8_strchr(BaratinooIntonationList, -1, ch)) {
					const gchar *next = g_utf8_next_char(p);
					gunichar ch_next;

					if (next < text + len)
						ch_next = g_utf8_get_char(next);
					else
						ch_next = '\n';

					if (!g_unichar_isalnum(ch_next) &&
					    !g_unichar_ispunct(ch_next)) {
						g_string_append_unichar(state->buffer, ch);
						/* Append an extra space to try and
						 * make sure it's considered as
						 * punctuation and isn't spoken. */
						g_string_append_c(state->buffer, ' ');
					}
				}
			}
		}
	}
}

/**
 * @brief Converts SSML data to Baratinoo's proprietary format.
 * @param buf A buffer to write to.
 * @param data SSML data to convert.
 * @param size Length of @p data
 *
 * @warning Only a subset of the input SSML is currently translated, the rest
 *          being discarded.
 */
static void append_ssml_as_proprietary(const Engine *engine, GString *buf, const char *data, gsize size)
{
	/* FIXME: we could possibly use SSML mode, but the Baratinoo parser is
	 * very strict and *requires* "xmlns", "version" and "lang" attributes
	 * on the <speak> tag, which speech-dispatcher doesn't provide.
	 *
	 * Moreover, we need to add tags for volume/rate/pitch so we'd have to
	 * amend the data anyway. */
	static const GMarkupParser parser = {
		.start_element = ssml2baratinoo_start_element,
		.end_element = ssml2baratinoo_end_element,
		.text = ssml2baratinoo_text,
	};
	SsmlPraserState state = {
		.engine = engine,
		.buffer = buf,
		.voice_stack_len = 0,
	};
	GMarkupParseContext *ctx;
	GError *err = NULL;

	ctx = g_markup_parse_context_new(&parser, G_MARKUP_TREAT_CDATA_AS_TEXT,
					 &state, NULL);
	if (!g_markup_parse_context_parse(ctx, data, size, &err) ||
	    !g_markup_parse_context_end_parse(ctx, &err)) {
		DBG(DBG_MODNAME "Failed to convert SSML: %s", err->message);
		g_error_free(err);
	}

	g_markup_parse_context_free(ctx);
}
