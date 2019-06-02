/*
 * ibmtts.c - Speech Dispatcher backend for IBM TTS
 *
 * Copyright (C) 2006, 2007 Brailcom, o.p.s.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * @author  Gary Cramblitt <garycramblitt@comcast.net> (original author)
 *
 * $Id: ibmtts.c,v 1.30 2008-06-30 14:34:02 gcasse Exp $
 */

/* This output module operates with four threads:

        The main thread called from Speech Dispatcher (module_*()).
        A synthesis thread that accepts messages, parses them, and forwards
            them to the IBM TTS via the Eloquence Command Interface (ECI).
            This thread receives audio and index mark callbacks from
            IBM TTS and queues them into a playback queue. See _ibmtts_synth().
        A playback thread that acts on entries in the playback queue,
            either sending them to the audio output module (module_tts_output()),
            or emitting Speech Dispatcher events.  See _ibmtts_play().
        A thread which is used to stop or pause the synthesis and
            playback threads.  See _ibmtts_stop_or_pause().

   Semaphores and mutexes are used to mediate between the 4 threads.

   TODO:
   - Support list_synthesis_voices()
   - Limit amout of waveform data synthesised in advance.
   - Use SSML mark feature of ibmtts instead of handcrafted parsing.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* System includes. */
#include <string.h>
#include <glib.h>
#include <semaphore.h>

/* IBM Eloquence Command Interface. */
#include <eci.h>

/* Speech Dispatcher includes. */
#include "spd_audio.h"
#include <speechd_types.h>
#include "module_utils.h"

typedef enum { IBMTTS_FALSE, IBMTTS_TRUE } TIbmttsBool;
typedef enum {
	FATAL_ERROR = -1,
	OK = 0,
	ERROR = 1
} TIbmttsSuccess;

/* TODO: These defines are in src/server/index_marking.h, but including that
         file here causes a redefinition error on FATAL macro in speechd.h. */
#define SD_MARK_BODY_LEN 6
#define SD_MARK_BODY "__spd_"
#define SD_MARK_HEAD "<mark name=\""SD_MARK_BODY
#define SD_MARK_TAIL "\"/>"

#define SD_MARK_HEAD_ONLY "<mark name=\""
#define SD_MARK_HEAD_ONLY_LEN 12
#define SD_MARK_TAIL_LEN 3

#define MODULE_NAME     "ibmtts"
#define MODULE_VERSION  "0.1"

#define DEBUG_MODULE 1
DECLARE_DEBUG();

/* Define a hash table where each entry is a double-linked list
   loaded from the config file.  Each entry in the config file
   is 3 strings, where the 1st string is used to access a list
   of the 2nd and 3rd strings. */
#define MOD_OPTION_3_STR_HT_DLL(name, arg1, arg2, arg3) \
	typedef struct{ \
		char* arg2; \
		char* arg3; \
	}T ## name; \
	GHashTable *name; \
	\
	DOTCONF_CB(name ## _cb) \
	{ \
		T ## name *new_item; \
		char *new_key; \
		GList *dll = NULL; \
		new_item = (T ## name *) g_malloc(sizeof(T ## name)); \
		new_key = g_strdup(cmd->data.list[0]); \
		if (NULL != cmd->data.list[1]) \
			new_item->arg2 = g_strdup(cmd->data.list[1]); \
		else \
			new_item->arg2 = NULL; \
		if (NULL != cmd->data.list[2]) \
			new_item->arg3 = g_strdup(cmd->data.list[2]); \
		else \
			new_item->arg3 = NULL; \
		dll = g_hash_table_lookup(name, new_key); \
		dll = g_list_append(dll, new_item); \
		g_hash_table_insert(name, new_key, dll); \
		return NULL; \
	}

/* Load a double-linked list from config file. */
#define MOD_OPTION_HT_DLL_REG(name) \
	name = g_hash_table_new(g_str_hash, g_str_equal); \
	module_dc_options = module_add_config_option(module_dc_options, \
	                    &module_num_dc_options, #name, \
	                    ARG_LIST, name ## _cb, NULL, 0);

/* Define a hash table mapping a string to 7 integer values. */
#define MOD_OPTION_6_INT_HT(name, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
	typedef struct{ \
		int arg1; \
		int arg2; \
		int arg3; \
		int arg4; \
		int arg5; \
		int arg6; \
		int arg7; \
	}T ## name; \
	GHashTable *name; \
	\
	DOTCONF_CB(name ## _cb) \
	{ \
		T ## name *new_item; \
		char* new_key; \
		new_item = (T ## name *) g_malloc(sizeof(T ## name)); \
		if (cmd->data.list[0] == NULL) return NULL; \
		new_key = g_strdup(cmd->data.list[0]); \
		new_item->arg1 = (int) strtol(cmd->data.list[1], NULL, 10); \
		new_item->arg2 = (int) strtol(cmd->data.list[2], NULL, 10); \
		new_item->arg3 = (int) strtol(cmd->data.list[3], NULL, 10); \
		new_item->arg4 = (int) strtol(cmd->data.list[4], NULL, 10); \
		new_item->arg5 = (int) strtol(cmd->data.list[5], NULL, 10); \
		new_item->arg6 = (int) strtol(cmd->data.list[6], NULL, 10); \
		new_item->arg7 = (int) strtol(cmd->data.list[7], NULL, 10); \
		g_hash_table_insert(name, new_key, new_item); \
		return NULL; \
	}

/* Thread and process control. */
static pthread_t ibmtts_synth_thread;
static pthread_t ibmtts_play_thread;
static pthread_t ibmtts_stop_or_pause_thread;

static sem_t ibmtts_synth_semaphore;
static sem_t ibmtts_play_semaphore;
static sem_t ibmtts_stop_or_pause_semaphore;

static pthread_mutex_t ibmtts_synth_suspended_mutex;
static pthread_mutex_t ibmtts_play_suspended_mutex;
static pthread_mutex_t ibmtts_stop_or_pause_suspended_mutex;

static TIbmttsBool ibmtts_thread_exit_requested = IBMTTS_FALSE;
static TIbmttsBool ibmtts_stop_synth_requested = IBMTTS_FALSE;
static TIbmttsBool ibmtts_stop_play_requested = IBMTTS_FALSE;
static TIbmttsBool ibmtts_pause_requested = IBMTTS_FALSE;

/* Current message from Speech Dispatcher. */
static char **ibmtts_message;
static SPDMessageType ibmtts_message_type;

/* ECI */
static ECIHand eciHandle = NULL_ECI_HAND;
static int eci_sample_rate = 0;

/* ECI sends audio back in chunks to this buffer.
   The smaller the buffer, the higher the overhead, but the better
   the index mark resolution. */
typedef signed short int TEciAudioSamples;
static TEciAudioSamples *audio_chunk;

/* For some reason, these were left out of eci.h. */
typedef enum {
	eciTextModeDefault = 0,
	eciTextModeAlphaSpell = 1,
	eciTextModeAllSpell = 2,
	eciIRCSpell = 3
} ECITextMode;

/* The playback queue. */
typedef enum {
	IBMTTS_QET_AUDIO,	/* Chunk of audio. */
	IBMTTS_QET_INDEX_MARK,	/* Index mark event. */
	IBMTTS_QET_SOUND_ICON,	/* A Sound Icon */
	IBMTTS_QET_BEGIN,	/* Beginning of speech. */
	IBMTTS_QET_END		/* Speech completed. */
} EPlaybackQueueEntryType;

typedef struct {
	long num_samples;
	TEciAudioSamples *audio_chunk;
} TPlaybackQueueAudioChunk;

typedef struct {
	EPlaybackQueueEntryType type;
	union {
		long markId;
		TPlaybackQueueAudioChunk audio;
		char *sound_icon_filename;
	} data;
} TPlaybackQueueEntry;

static GSList *playback_queue = NULL;
static pthread_mutex_t playback_queue_mutex;

/* A lookup table for index mark name given integer id. */
GHashTable *ibmtts_index_mark_ht = NULL;
#define IBMTTS_MSG_END_MARK 0

pthread_mutex_t sound_stop_mutex;

/* When a voice is set, this is the baseline pitch of the voice.
   SSIP PITCH commands then adjust relative to this. */
int ibmtts_voice_pitch_baseline;
/* When a voice is set, this the default speed of the voice.
   SSIP RATE commands then adjust relative to this. */
int ibmtts_voice_speed;

/* Expected input encoding for current language dialect. */
static char *ibmtts_input_encoding = "cp1252";

/* list of voices */
static SPDVoice **ibmtts_voice_list = NULL;
static int *ibmtts_voice_index = NULL;

/* Internal function prototypes for main thread. */
static void ibmtts_set_language(char *lang);
static void ibmtts_set_voice(SPDVoiceType voice);
static char *ibmtts_voice_enum_to_str(SPDVoiceType voice);
static void ibmtts_set_language_and_voice(char *lang, SPDVoiceType voice,
					  char *dialect);
static void ibmtts_set_synthesis_voice(char *);
static void ibmtts_set_rate(signed int rate);
static void ibmtts_set_pitch(signed int pitch);
static void ibmtts_set_punctuation_mode(SPDPunctuation punct_mode);
static void ibmtts_set_volume(signed int pitch);

/* locale_index_atomic stores the current index of the eciLocales array.
The main thread writes this information, the synthesis thread reads it.
*/
static gint locale_index_atomic;

/* Internal function prototypes for synthesis thread. */
static char *ibmtts_extract_mark_name(char *mark);
static char *ibmtts_next_part(char *msg, char **mark_name);
static int ibmtts_replace(char *from, char *to, GString * msg);
static void ibmtts_subst_keys_cb(gpointer data, gpointer user_data);
static char *ibmtts_subst_keys(char *key);
static char *ibmtts_search_for_sound_icon(const char *icon_name);
static TIbmttsBool ibmtts_add_sound_icon_to_playback_queue(char *filename);
static void ibmtts_load_user_dictionary();

static enum ECICallbackReturn eciCallback(ECIHand hEngine,
					  enum ECIMessage msg,
					  long lparam, void *data);

/* Internal function prototypes for playback thread. */
static TIbmttsBool ibmtts_add_audio_to_playback_queue(TEciAudioSamples *
						      audio_chunk,
						      long num_samples);
static TIbmttsBool ibmtts_add_mark_to_playback_queue(long markId);
static TIbmttsBool ibmtts_add_flag_to_playback_queue(EPlaybackQueueEntryType
						     type);
static void ibmtts_delete_playback_queue_entry(TPlaybackQueueEntry *
					       playback_queue_entry);
static TIbmttsBool ibmtts_send_to_audio(TPlaybackQueueEntry *
					playback_queue_entry);

/* Miscellaneous internal function prototypes. */
static TIbmttsBool is_thread_busy(pthread_mutex_t * suspended_mutex);
static void ibmtts_log_eci_error();
static void ibmtts_clear_playback_queue();
static void alloc_voice_list();
static void free_voice_list();

/* The synthesis thread start routine. */
static void *_ibmtts_synth(void *);
/* The playback thread start routine. */
static void *_ibmtts_play(void *);
/* The stop_or_pause start routine. */
static void *_ibmtts_stop_or_pause(void *);

/* Module configuration options. */
MOD_OPTION_1_INT(IbmttsUseSSML);
MOD_OPTION_1_STR(IbmttsPunctuationList);
MOD_OPTION_1_INT(IbmttsUseAbbreviation);
MOD_OPTION_1_STR(IbmttsDictionaryFolder);
MOD_OPTION_1_INT(IbmttsAudioChunkSize);
MOD_OPTION_1_STR(IbmttsSoundIconFolder);
MOD_OPTION_6_INT_HT(IbmttsVoiceParameters,
		    gender, breathiness, head_size, pitch_baseline,
		    pitch_fluctuation, roughness, speed);
MOD_OPTION_3_STR_HT_DLL(IbmttsKeySubstitution, lang, key, newkey);

typedef struct _eciLocale {
	char *name;
	char *lang;
	char *variant;
	enum ECILanguageDialect langID;
	char *charset;
} eciLocale, *eciLocaleList;

static eciLocale eciLocales[] = {
	{"American_English", "en-US", NULL, eciGeneralAmericanEnglish, "ISO-8859-1"},
	{"British_English", "en-GB", NULL, eciBritishEnglish, "ISO-8859-1"},
	{"Castilian_Spanish", "es-ES", NULL, eciCastilianSpanish, "ISO-8859-1"},
	{"Mexican_Spanish", "es-MX", NULL, eciMexicanSpanish, "ISO-8859-1"},
	{"French", "fr-FR", NULL, eciStandardFrench, "ISO-8859-1"},
	{"Canadian_French", "fr-CA", NULL, eciCanadianFrench, "ISO-8859-1"},
	{"German", "de-DE", NULL, eciStandardGerman, "ISO-8859-1"},
	{"Italian", "it-IT", NULL, eciStandardItalian, "ISO-8859-1"},
	{"Mandarin_Chinese", "zh-CN", NULL, eciMandarinChinese, "GBK"},
	{"Mandarin_Chinese GB", "zh-CN", "GB", eciMandarinChineseGB, "GBK"},
	{"Mandarin_Chinese PinYin", "zh-CN", "PinYin", eciMandarinChinesePinYin, "GBK"},
	{"Mandarin_Chinese UCS", "zh-CN", "UCS2", eciMandarinChineseUCS, "UCS2"},
	{"Taiwanese_Mandarin", "zh-TW", NULL, eciTaiwaneseMandarin, "BIG5"},
	{"Taiwanese_Mandarin Big 5", "zh-TW", "Big5", eciTaiwaneseMandarinBig5, "BIG5"},
	{"Taiwanese_Mandarin ZhuYin", "zh-TW", "ZhuYin", eciTaiwaneseMandarinZhuYin, "BIG5"},
	{"Taiwanese_Mandarin PinYin", "zh-TW", "PinYin", eciTaiwaneseMandarinPinYin, "BIG5"},
	{"Taiwanese_Mandarin UCS", "zh-TW", "UCS", eciTaiwaneseMandarinUCS, "UCS2"},
	{"Brazilian_Portuguese", "pt-BR", NULL, eciBrazilianPortuguese, "ISO-8859-1"},
	{"Japanese", "ja-JP", NULL, eciStandardJapanese, "SJIS"},
	{"Japanese_SJIS", "ja-JP", "SJIS", eciStandardJapaneseSJIS, "SJIS"},
	{"Japanese_UCS", "ja-JP", "UCS", eciStandardJapaneseUCS, "UCS2"},
	{"Finnish", "fi-FI", NULL, eciStandardFinnish, "ISO-8859-1"},
	{"Korean", "ko-KR", NULL, eciStandardKorean, "UHC"},
	{"Korean_UHC", "ko-KR", "UHC", eciStandardKoreanUHC, "UHC"},
	{"Korean_UCS", "ko-KR", "UCS", eciStandardKoreanUCS, "UCS2"},
	{"Cantonese", "zh-HK", NULL, eciStandardCantonese, "GBK"},
	{"Cantonese_GB", "zh-HK", "GB", eciStandardCantoneseGB, "GBK"},
	{"Cantonese_UCS", "zh-HK", "UCS", eciStandardCantoneseUCS, "UCS2"},
	{"HongKong_Cantonese", "zh-HK", NULL, eciHongKongCantonese, "BIG5"},
	{"HongKong_Cantonese Big 5", "zh-HK", "BIG5", eciHongKongCantoneseBig5, "BIG5"},
	{"HongKong_Cantonese UCS", "zh-HK", "UCS", eciHongKongCantoneseUCS, "UCS-2"},
	{"Dutch", "nl-BE", NULL, eciStandardDutch, "ISO-8859-1"},
	{"Norwegian", "no-NO", NULL, eciStandardNorwegian, "ISO-8859-1"},
	{"Swedish", "sv-SE", NULL, eciStandardSwedish, "ISO-8859-1"},
	{"Danish", "da-DK", NULL, eciStandardDanish, "ISO-8859-1"},
	{"Reserved", "en-US", NULL, eciStandardReserved, "ISO-8859-1"},
	{"Thai", "th-TH", NULL, eciStandardThai, "TIS-620"},
	{"ThaiTIS", "th-TH", "TIS", eciStandardThaiTIS, "TIS-620"},
	{NULL, 0, NULL}
};

#define MAX_NB_OF_LANGUAGES (sizeof(eciLocales)/sizeof(eciLocales[0]) - 1)

/* dictionary_filename: its index corresponds to the ECIDictVolume enumerate */
static char *dictionary_filenames[] = {
	"main.dct",
	"root.dct",
	"abbreviation.dct",
	"extension.dct"
};

#define NB_OF_DICTIONARY_FILENAMES (sizeof(dictionary_filenames)/sizeof(dictionary_filenames[0]))

/* Public functions */

int module_load(void)
{
	INIT_SETTINGS_TABLES();

	REGISTER_DEBUG();

	MOD_OPTION_1_INT_REG(IbmttsUseSSML, 1);
	MOD_OPTION_1_INT_REG(IbmttsUseAbbreviation, 1);
	MOD_OPTION_1_STR_REG(IbmttsPunctuationList, "()?");
	MOD_OPTION_1_STR_REG(IbmttsDictionaryFolder,
			     "/var/opt/IBM/ibmtts/dict");

	MOD_OPTION_1_INT_REG(IbmttsAudioChunkSize, 20000);
	MOD_OPTION_1_STR_REG(IbmttsSoundIconFolder,
			     "/usr/share/sounds/sound-icons/");

	/* Register voices. */
	module_register_settings_voices();

	/* Register voice parameters */
	MOD_OPTION_HT_REG(IbmttsVoiceParameters);

	/* Register key substitutions. */
	MOD_OPTION_HT_DLL_REG(IbmttsKeySubstitution);

	return OK;
}

int module_init(char **status_info)
{
	int ret;
	char ibmVersion[20];
	int ibm_sample_rate;

	DBG("Ibmtts: Module init().");
	INIT_INDEX_MARKING();

	*status_info = NULL;
	ibmtts_thread_exit_requested = IBMTTS_FALSE;

	/* Report versions. */
	eciVersion(ibmVersion);
	DBG("Ibmtts: IBM TTS Output Module version %s, IBM TTS Engine version %s", MODULE_VERSION, ibmVersion);

	/* Setup IBM TTS engine. */
	DBG("Ibmtts: Creating ECI instance.");
	eciHandle = eciNew();
	if (NULL_ECI_HAND == eciHandle) {
		DBG("Ibmtts: Could not create ECI instance.\n");
		*status_info = g_strdup("Could not create ECI instance. "
					"Is the IBM TTS engine installed?");
		return FATAL_ERROR;
	}

	/* Get ECI audio sample rate. */
	ibm_sample_rate = eciGetParam(eciHandle, eciSampleRate);
	switch (ibm_sample_rate) {
	case 0:
		eci_sample_rate = 8000;
		break;
	case 1:
		eci_sample_rate = 11025;
		break;
	case 2:
		eci_sample_rate = 22050;
		break;
	default:
		DBG("Ibmtts: Invalid audio sample rate returned by ECI = %i",
		    ibm_sample_rate);
	}

	/* Allocate a chunk for ECI to return audio. */
	audio_chunk =
	    (TEciAudioSamples *) g_malloc((IbmttsAudioChunkSize) *
					  sizeof(TEciAudioSamples));

	DBG("Ibmtts: Registering ECI callback.");
	eciRegisterCallback(eciHandle, eciCallback, NULL);

	DBG("Ibmtts: Registering an ECI audio buffer.");
	if (!eciSetOutputBuffer(eciHandle, IbmttsAudioChunkSize, audio_chunk)) {
		DBG("Ibmtts: Error registering ECI audio buffer.");
		ibmtts_log_eci_error();
	}

	eciSetParam(eciHandle, eciDictionary, !IbmttsUseAbbreviation);

	/* enable annotations */
	eciSetParam(eciHandle, eciInputType, 1);

	/* load possibly the ssml filter */
	eciAddText(eciHandle, " `gfa1 ");

	/* load possibly the punctuation filter */
	eciAddText(eciHandle, " `gfa2 ");

	ibmtts_set_punctuation_mode(msg_settings.punctuation_mode);

	alloc_voice_list();

	/* These mutexes are locked when the corresponding threads are suspended. */
	pthread_mutex_init(&ibmtts_synth_suspended_mutex, NULL);
	pthread_mutex_init(&ibmtts_play_suspended_mutex, NULL);

	/* This mutex is used to hold a stop request until audio actually stops. */
	pthread_mutex_init(&sound_stop_mutex, NULL);

	/* This mutex mediates access to the playback queue between the synthesis and
	   playback threads. */
	pthread_mutex_init(&playback_queue_mutex, NULL);

	DBG("Ibmtts: ImbttsAudioChunkSize = %d", IbmttsAudioChunkSize);

	ibmtts_message = g_malloc(sizeof(char *));
	*ibmtts_message = NULL;

	DBG("Ibmtts: Creating new thread for stop or pause.");
	sem_init(&ibmtts_stop_or_pause_semaphore, 0, 0);

	ret =
	    pthread_create(&ibmtts_stop_or_pause_thread, NULL,
			   _ibmtts_stop_or_pause, NULL);
	if (0 != ret) {
		DBG("Ibmtts: stop or pause thread creation failed.");
		*status_info =
		    g_strdup
		    ("The module couldn't initialize stop or pause thread. "
		     "This could be either an internal problem or an "
		     "architecture problem. If you are sure your architecture "
		     "supports threads, please report a bug.");
		return FATAL_ERROR;
	}

	DBG("Ibmtts: Creating new thread for playback.");
	sem_init(&ibmtts_play_semaphore, 0, 0);

	ret = pthread_create(&ibmtts_play_thread, NULL, _ibmtts_play, NULL);
	if (0 != ret) {
		DBG("Ibmtts: play thread creation failed.");
		*status_info =
		    g_strdup("The module couldn't initialize play thread. "
			     "This could be either an internal problem or an "
			     "architecture problem. If you are sure your architecture "
			     "supports threads, please report a bug.");
		return FATAL_ERROR;
	}

	DBG("Ibmtts: Creating new thread for IBM TTS synthesis.");
	sem_init(&ibmtts_synth_semaphore, 0, 0);

	ret = pthread_create(&ibmtts_synth_thread, NULL, _ibmtts_synth, NULL);
	if (0 != ret) {
		DBG("Ibmtts: synthesis thread creation failed.");
		*status_info =
		    g_strdup("The module couldn't initialize synthesis thread. "
			     "This could be either an internal problem or an "
			     "architecture problem. If you are sure your architecture "
			     "supports threads, please report a bug.");
		return FATAL_ERROR;
	}

	*status_info = g_strdup("Ibmtts: Initialized successfully.");

	return OK;
}

SPDVoice **module_list_voices(void)
{
	DBG("Ibmtts: %s", __FUNCTION__);
	return ibmtts_voice_list;
}

int module_speak(gchar * data, size_t bytes, SPDMessageType msgtype)
{
	DBG("Ibmtts: module_speak().");

	if (is_thread_busy(&ibmtts_synth_suspended_mutex) ||
	    is_thread_busy(&ibmtts_play_suspended_mutex) ||
	    is_thread_busy(&ibmtts_stop_or_pause_suspended_mutex)) {
		DBG("Ibmtts: Already synthesizing when requested to synthesize (module_speak).");
		return IBMTTS_FALSE;
	}

	DBG("Ibmtts: Type: %d, bytes: %lu, requested data: |%s|\n", msgtype,
	    (unsigned long)bytes, data);

	g_free(*ibmtts_message);
	*ibmtts_message = NULL;

	if (!g_utf8_validate(data, bytes, NULL)) {
		DBG("Ibmtts: Input is not valid utf-8.");
		/* Actually, we should just fail here, but let's assume input is latin-1 */
		*ibmtts_message =
		    g_convert(data, bytes, "utf-8", "iso-8859-1", NULL, NULL,
			      NULL);
		if (*ibmtts_message == NULL) {
			DBG("Ibmtts: Fallback conversion to utf-8 failed.");
			return FALSE;
		}
	} else {
		*ibmtts_message = g_strndup(data, bytes);
	}

	ibmtts_message_type = msgtype;
	if ((msgtype == SPD_MSGTYPE_TEXT)
	    && (msg_settings.spelling_mode == SPD_SPELL_ON))
		ibmtts_message_type = SPD_MSGTYPE_SPELL;

	/* Setting speech parameters. */
	UPDATE_STRING_PARAMETER(voice.language, ibmtts_set_language);
	UPDATE_PARAMETER(voice_type, ibmtts_set_voice);
	UPDATE_STRING_PARAMETER(voice.name, ibmtts_set_synthesis_voice);
	UPDATE_PARAMETER(rate, ibmtts_set_rate);
	UPDATE_PARAMETER(volume, ibmtts_set_volume);
	UPDATE_PARAMETER(pitch, ibmtts_set_pitch);
	UPDATE_PARAMETER(punctuation_mode, ibmtts_set_punctuation_mode);

	/* TODO: Handle these in _ibmtts_synth() ?
	   UPDATE_PARAMETER(cap_let_recogn, festival_set_cap_let_recogn);
	 */

	if (!IbmttsUseSSML) {
		/* Strip all SSML */
		char *tmp = *ibmtts_message;
		*ibmtts_message = module_strip_ssml(*ibmtts_message);
		g_free(tmp);
		/* Convert input to suitable encoding for current language dialect */
		tmp =
		    g_convert_with_fallback(*ibmtts_message, -1,
					    ibmtts_input_encoding, "utf-8", "?",
					    NULL, &bytes, NULL);
		if (tmp != NULL) {
			g_free(*ibmtts_message);
			*ibmtts_message = tmp;
		}
	}

	/* Send semaphore signal to the synthesis thread */
	sem_post(&ibmtts_synth_semaphore);

	DBG("Ibmtts: Leaving module_speak() normally.");
	return TRUE;
}

int module_stop(void)
{
	DBG("Ibmtts: module_stop().");

	if ((is_thread_busy(&ibmtts_synth_suspended_mutex) ||
	     is_thread_busy(&ibmtts_play_suspended_mutex)) &&
	    !is_thread_busy(&ibmtts_stop_or_pause_suspended_mutex)) {

		/* Request both synth and playback threads to stop what they are doing
		   (if anything). */
		ibmtts_stop_synth_requested = IBMTTS_TRUE;
		ibmtts_stop_play_requested = IBMTTS_TRUE;

		/* Wake the stop_or_pause thread. */
		sem_post(&ibmtts_stop_or_pause_semaphore);
	}

	return OK;
}

size_t module_pause(void)
{
	/* The semantics of module_pause() is the same as module_stop()
	   except that processing should continue until the next index mark is
	   reached before stopping.
	   Note that although IBM TTS offers an eciPause function, we cannot
	   make use of it because Speech Dispatcher doesn't have a module_resume
	   function. Instead, Speech Dispatcher resumes by calling module_speak
	   from the last index mark reported in the text. */
	DBG("Ibmtts: module_pause().");

	/* Request playback thread to pause.  Note we cannot stop synthesis or
	   playback until end of sentence or end of message is played. */
	ibmtts_pause_requested = IBMTTS_TRUE;

	/* Wake the stop_or_pause thread. */
	sem_post(&ibmtts_stop_or_pause_semaphore);

	return OK;
}

int module_close(void)
{

	DBG("Ibmtts: close().");

	if (is_thread_busy(&ibmtts_synth_suspended_mutex) ||
	    is_thread_busy(&ibmtts_play_suspended_mutex)) {
		DBG("Ibmtts: Stopping speech");
		module_stop();
	}

	DBG("Ibmtts: De-registering ECI callback.");
	eciRegisterCallback(eciHandle, NULL, NULL);

	DBG("Ibmtts: Destroying ECI instance.");
	eciDelete(eciHandle);
	eciHandle = NULL_ECI_HAND;

	/* Free buffer for ECI audio. */
	g_free(audio_chunk);

	/* Request each thread exit and wait until it exits. */
	DBG("Ibmtts: Terminating threads");
	ibmtts_thread_exit_requested = IBMTTS_TRUE;
	sem_post(&ibmtts_synth_semaphore);
	sem_post(&ibmtts_play_semaphore);
	sem_post(&ibmtts_stop_or_pause_semaphore);
	if (0 != pthread_join(ibmtts_synth_thread, NULL))
		return -1;
	if (0 != pthread_join(ibmtts_play_thread, NULL))
		return -1;
	if (0 != pthread_join(ibmtts_stop_or_pause_thread, NULL))
		return -1;

	ibmtts_clear_playback_queue();

	/* Free index mark lookup table. */
	if (ibmtts_index_mark_ht) {
		g_hash_table_destroy(ibmtts_index_mark_ht);
		ibmtts_index_mark_ht = NULL;
	}

	free_voice_list();
	sem_destroy(&ibmtts_synth_semaphore);
	sem_destroy(&ibmtts_play_semaphore);
	sem_destroy(&ibmtts_stop_or_pause_semaphore);

	return 0;
}

/* Internal functions */

/* Return true if the thread is busy, i.e., suspended mutex is not locked. */
static TIbmttsBool is_thread_busy(pthread_mutex_t * suspended_mutex)
{
	if (EBUSY == pthread_mutex_trylock(suspended_mutex))
		return IBMTTS_FALSE;
	else {
		pthread_mutex_unlock(suspended_mutex);
		return IBMTTS_TRUE;
	}
}

/* Given a string containing an index mark in the form
   <mark name="some_name"/>, returns some_name.  Calling routine is
   responsible for freeing returned string. If an error occurs,
   returns NULL. */
static char *ibmtts_extract_mark_name(char *mark)
{
	if ((SD_MARK_HEAD_ONLY_LEN + SD_MARK_TAIL_LEN + 1) > strlen(mark))
		return NULL;
	mark = mark + SD_MARK_HEAD_ONLY_LEN;
	char *tail = strstr(mark, SD_MARK_TAIL);
	if (NULL == tail)
		return NULL;
	return (char *)g_strndup(mark, tail - mark);
}

/* Returns the portion of msg up to, but not including, the next index
   mark, or end of msg if no index mark is found.  If msg begins with
   and index mark, returns the entire index mark clause (<mark name="whatever"/>)
   and returns the mark name.  If msg does not begin with an index mark,
   mark_name will be NULL. If msg is empty, returns a zero-length string (not NULL).
   Caller is responsible for freeing both returned string and mark_name (if not NULL). */
/* TODO: This routine needs to be more tolerant of custom index marks with spaces. */
/* TODO: Should there be a MaxChunkLength? Delimiters? */
static char *ibmtts_next_part(char *msg, char **mark_name)
{
	char *mark_head = strstr(msg, SD_MARK_HEAD_ONLY);
	if (NULL == mark_head)
		return (char *)g_strndup(msg, strlen(msg));
	else if (mark_head == msg) {
		*mark_name = ibmtts_extract_mark_name(mark_head);
		if (NULL == *mark_name)
			return strcat((char *)
				      g_strndup(msg, SD_MARK_HEAD_ONLY_LEN),
				      ibmtts_next_part(msg +
						       SD_MARK_HEAD_ONLY_LEN,
						       mark_name));
		else
			return (char *)g_strndup(msg,
						 SD_MARK_HEAD_ONLY_LEN +
						 strlen(*mark_name) +
						 SD_MARK_TAIL_LEN);
	} else
		return (char *)g_strndup(msg, mark_head - msg);
}

/* Stop or Pause thread. */
static void *_ibmtts_stop_or_pause(void *nothing)
{
	DBG("Ibmtts: Stop or pause thread starting.......\n");

	/* Block all signals to this thread. */
	set_speaking_thread_parameters();

	while (!ibmtts_thread_exit_requested) {
		/* If semaphore not set, set suspended lock and suspend until it is signaled. */
		if (0 != sem_trywait(&ibmtts_stop_or_pause_semaphore)) {
			pthread_mutex_lock
			    (&ibmtts_stop_or_pause_suspended_mutex);
			sem_wait(&ibmtts_stop_or_pause_semaphore);
			pthread_mutex_unlock
			    (&ibmtts_stop_or_pause_suspended_mutex);
			if (ibmtts_thread_exit_requested)
				break;
		}
		DBG("Ibmtts: Stop or pause semaphore on.");
		/* The following is a hack. The condition should never
		   be true, but sometimes it is true for unclear reasons. */
		if (!(ibmtts_stop_synth_requested || ibmtts_pause_requested))
			continue;

		if (ibmtts_stop_synth_requested) {
			/* Stop synthesis (if in progress). */
			if (eciHandle) {
				DBG("Ibmtts: Stopping synthesis.");
				eciStop(eciHandle);
			}

			/* Stop any audio playback (if in progress). */
			if (module_audio_id) {
				pthread_mutex_lock(&sound_stop_mutex);
				DBG("Ibmtts: Stopping audio.");
				int ret = spd_audio_stop(module_audio_id);
				if (0 != ret)
					DBG("Ibmtts: WARNING: Non 0 value from spd_audio_stop: %d", ret);
				pthread_mutex_unlock(&sound_stop_mutex);
			}
		}

		DBG("Ibmtts: Waiting for synthesis thread to suspend.");
		while (is_thread_busy(&ibmtts_synth_suspended_mutex))
			g_usleep(100);
		DBG("Ibmtts: Waiting for playback thread to suspend.");
		while (is_thread_busy(&ibmtts_play_suspended_mutex))
			g_usleep(100);

		DBG("Ibmtts: Clearing playback queue.");
		ibmtts_clear_playback_queue();

		DBG("Ibmtts: Clearing index mark lookup table.");
		if (ibmtts_index_mark_ht) {
			g_hash_table_destroy(ibmtts_index_mark_ht);
			ibmtts_index_mark_ht = NULL;
		}

		if (ibmtts_stop_synth_requested)
			module_report_event_stop();
		else
			module_report_event_pause();

		ibmtts_stop_synth_requested = IBMTTS_FALSE;
		ibmtts_stop_play_requested = IBMTTS_FALSE;
		ibmtts_pause_requested = IBMTTS_FALSE;

		DBG("Ibmtts: Stop or pause completed.");
	}
	DBG("Ibmtts: Stop or pause thread ended.......\n");

	pthread_exit(NULL);
}

static int process_text_mark(char *part, int part_len, char *mark_name)
{
	/* Handle index marks. */
	if (NULL != mark_name) {
		/* Assign the mark name an integer number and store in lookup table. */
		int *markId = (int *)g_malloc(sizeof(int));
		*markId = 1 + g_hash_table_size(ibmtts_index_mark_ht);
		g_hash_table_insert(ibmtts_index_mark_ht, markId, mark_name);
		if (!eciInsertIndex(eciHandle, *markId)) {
			DBG("Ibmtts: Error sending index mark to synthesizer.");
			ibmtts_log_eci_error();
			/* Try to keep going. */
		} else
			DBG("Ibmtts: Index mark |%s| (id %i) sent to synthesizer.", mark_name, *markId);
		/* If pause is requested, skip over rest of message,
		   but synthesize what we have so far. */
		if (ibmtts_pause_requested) {
			DBG("Ibmtts: Pause requested in synthesis thread.");
			return 1;
		}
		return 0;
	}

	/* Handle normal text. */
	if (part_len > 0) {
		DBG("Ibmtts: Returned %d bytes from get_part.", part_len);
		DBG("Ibmtts: Text to synthesize is |%s|\n", part);
		DBG("Ibmtts: Sending text to synthesizer.");
		if (!eciAddText(eciHandle, part)) {
			DBG("Ibmtts: Error sending text.");
			ibmtts_log_eci_error();
			return 2;
		}
		return 0;
	}

	/* Handle end of text. */
	DBG("Ibmtts: End of data in synthesis thread.");
	/*
	   Add index mark for end of message.
	   This also makes sure the callback gets called at least once
	 */
	eciInsertIndex(eciHandle, IBMTTS_MSG_END_MARK);
	DBG("Ibmtts: Trying to synthesize text.");
	if (!eciSynthesize(eciHandle)) {
		DBG("Ibmtts: Error synthesizing.");
		ibmtts_log_eci_error();
		return 2;;
	}

	/* Audio and index marks are returned in eciCallback(). */
	DBG("Ibmtts: Waiting for synthesis to complete.");
	if (!eciSynchronize(eciHandle)) {
		DBG("Ibmtts: Error waiting for synthesis to complete.");
		ibmtts_log_eci_error();
		return 2;
	}
	DBG("Ibmtts: Synthesis complete.");
	return 3;
}

/* Synthesis thread. */
static void *_ibmtts_synth(void *nothing)
{
	char *pos = NULL;
	char *part = NULL;
	int part_len = 0;
	int ret;

	DBG("Ibmtts: Synthesis thread starting.......\n");

	/* Block all signals to this thread. */
	set_speaking_thread_parameters();

	/* Allocate a place for index mark names to be placed. */
	char *mark_name = NULL;

	while (!ibmtts_thread_exit_requested) {
		/* If semaphore not set, set suspended lock and suspend until it is signaled. */
		if (0 != sem_trywait(&ibmtts_synth_semaphore)) {
			pthread_mutex_lock(&ibmtts_synth_suspended_mutex);
			sem_wait(&ibmtts_synth_semaphore);
			pthread_mutex_unlock(&ibmtts_synth_suspended_mutex);
			if (ibmtts_thread_exit_requested)
				break;
		}
		DBG("Ibmtts: Synthesis semaphore on.");

		/* This table assigns each index mark name an integer id for fast lookup when
		   ECI returns the integer index mark event. */
		if (ibmtts_index_mark_ht)
			g_hash_table_destroy(ibmtts_index_mark_ht);
		ibmtts_index_mark_ht =
		    g_hash_table_new_full(g_int_hash, g_int_equal, g_free,
					  g_free);

		pos = *ibmtts_message;
		ibmtts_load_user_dictionary();

		switch (ibmtts_message_type) {
		case SPD_MSGTYPE_TEXT:
			eciSetParam(eciHandle, eciTextMode, eciTextModeDefault);
			break;
		case SPD_MSGTYPE_SOUND_ICON:
			/* IBM TTS does not support sound icons.
			   If we can find a sound icon file, play that,
			   otherwise speak the name of the sound icon. */
			part = ibmtts_search_for_sound_icon(*ibmtts_message);
			if (NULL != part) {
				ibmtts_add_flag_to_playback_queue
				    (IBMTTS_QET_BEGIN);
				ibmtts_add_sound_icon_to_playback_queue(part);
				part = NULL;
				ibmtts_add_flag_to_playback_queue
				    (IBMTTS_QET_END);
				/* Wake up the audio playback thread, if not already awake. */
				if (!is_thread_busy
				    (&ibmtts_play_suspended_mutex))
					sem_post(&ibmtts_play_semaphore);
				continue;
			} else
				eciSetParam(eciHandle, eciTextMode,
					    eciTextModeDefault);
			break;
		case SPD_MSGTYPE_CHAR:
			eciSetParam(eciHandle, eciTextMode,
				    eciTextModeAllSpell);
			break;
		case SPD_MSGTYPE_KEY:
			/* Map unspeakable keys to speakable words. */
			DBG("Ibmtts: Key from Speech Dispatcher: |%s|", pos);
			pos = ibmtts_subst_keys(pos);
			DBG("Ibmtts: Key to speak: |%s|", pos);
			g_free(*ibmtts_message);
			*ibmtts_message = pos;
			eciSetParam(eciHandle, eciTextMode, eciTextModeDefault);
			break;
		case SPD_MSGTYPE_SPELL:
			if (SPD_PUNCT_NONE != msg_settings.punctuation_mode)
				eciSetParam(eciHandle, eciTextMode,
					    eciTextModeAllSpell);
			else
				eciSetParam(eciHandle, eciTextMode,
					    eciTextModeAlphaSpell);
			break;
		}

		ibmtts_add_flag_to_playback_queue(IBMTTS_QET_BEGIN);
		while (TRUE) {
			if (ibmtts_stop_synth_requested) {
				DBG("Ibmtts: Stop in synthesis thread, terminating.");
				break;
			}

			/* TODO: How to map these msg_settings to ibm tts?
			   ESpellMode spelling_mode;
			   SPELLING_ON already handled in module_speak()
			   ECapLetRecogn cap_let_recogn;
			   RECOGN_NONE = 0,
			   RECOGN_SPELL = 1,
			   RECOGN_ICON = 2
			 */

			part = ibmtts_next_part(pos, &mark_name);
			if (NULL == part) {
				DBG("Ibmtts: Error getting next part of message.");
				/* TODO: What to do here? */
				break;
			}
			part_len = strlen(part);
			pos += part_len;
			ret = process_text_mark(part, part_len, mark_name);
			g_free(part);
			part = NULL;
			mark_name = NULL;
			if (ret == 1)
				pos += strlen(pos);
			else if (ret > 1)
				break;
		}
	}

	DBG("Ibmtts: Synthesis thread ended.......\n");

	pthread_exit(NULL);
}

static void ibmtts_set_rate(signed int rate)
{
	/* Setting rate to midpoint is too fast.  An eci value of 50 is "normal".
	   See chart on pg 38 of the ECI manual. */
	assert(rate >= -100 && rate <= +100);
	int speed;
	/* Possible ECI range is 0 to 250. */
	/* Map rate -100 to 100 onto speed 0 to 140. */
	if (rate < 0)
		/* Map -100 to 0 onto 0 to ibmtts_voice_speed */
		speed = ((float)(rate + 100) * ibmtts_voice_speed) / (float)100;
	else
		/* Map 0 to 100 onto ibmtts_voice_speed to 140 */
		speed =
		    (((float)rate * (140 - ibmtts_voice_speed)) / (float)100)
		    + ibmtts_voice_speed;
	assert(speed >= 0 && speed <= 140);
	int ret = eciSetVoiceParam(eciHandle, 0, eciSpeed, speed);
	if (-1 == ret) {
		DBG("Ibmtts: Error setting rate %i.", speed);
		ibmtts_log_eci_error();
	} else
		DBG("Ibmtts: Rate set to %i.", speed);
}

static void ibmtts_set_volume(signed int volume)
{
	/* Setting volume to midpoint makes speech too soft.  An eci value
	   of 90 to 100 is "normal".
	   See chart on pg 38 of the ECI manual.
	   TODO: Rather than setting volume in the synth, maybe control volume on playback? */
	assert(volume >= -100 && volume <= +100);
	int vol;
	/* Possible ECI range is 0 to 100. */
	if (volume < 0)
		/* Map -100 to 0 onto 0 to 90 */
		vol = (((float)volume + 100) * 90) / (float)100;
	else
		/* Map 0 to 100 onto 90 to 100 */
		vol = ((float)(volume * 10) / (float)100) + 90;
	assert(vol >= 0 && vol <= 100);
	int ret = eciSetVoiceParam(eciHandle, 0, eciVolume, vol);
	if (-1 == ret) {
		DBG("Ibmtts: Error setting volume %i.", vol);
		ibmtts_log_eci_error();
	} else
		DBG("Ibmtts: Volume set to %i.", vol);
}

static void ibmtts_set_pitch(signed int pitch)
{
	/* Setting pitch to midpoint is to low.  eci values between 65 and 89
	   are "normal".
	   See chart on pg 38 of the ECI manual. */
	assert(pitch >= -100 && pitch <= +100);
	int pitchBaseline;
	/* Possible range 0 to 100. */
	if (pitch < 0)
		/* Map -100 to 0 onto 0 to ibmtts_voice_pitch_baseline */
		pitchBaseline =
		    ((float)(pitch + 100) * ibmtts_voice_pitch_baseline) /
		    (float)100;
	else
		/* Map 0 to 100 onto ibmtts_voice_pitch_baseline to 100 */
		pitchBaseline =
		    (((float)pitch * (100 - ibmtts_voice_pitch_baseline)) /
		     (float)100)
		    + ibmtts_voice_pitch_baseline;
	assert(pitchBaseline >= 0 && pitchBaseline <= 100);
	int ret =
	    eciSetVoiceParam(eciHandle, 0, eciPitchBaseline, pitchBaseline);
	if (-1 == ret) {
		DBG("Ibmtts: Error setting pitch %i.", pitchBaseline);
		ibmtts_log_eci_error();
	} else
		DBG("Ibmtts: Pitch set to %i.", pitchBaseline);
}

static void ibmtts_set_punctuation_mode(SPDPunctuation punct_mode)
{
	const char *fmt = "`Pf%d%s";
	char *msg = NULL;
	int real_punct_mode = 0;

	switch (punct_mode) {
	case SPD_PUNCT_NONE:
		real_punct_mode = 0;
		break;
	case SPD_PUNCT_SOME:
		real_punct_mode = 2;
		break;
	case SPD_PUNCT_ALL:
		real_punct_mode = 1;
		break;
	}

	msg = g_strdup_printf(fmt, real_punct_mode, IbmttsPunctuationList);
	eciAddText(eciHandle, msg);
	g_free(msg);
}

static char *ibmtts_voice_enum_to_str(SPDVoiceType voice)
{
	/* TODO: Would be better to move this to module_utils.c. */
	char *voicename;
	switch (voice) {
	case SPD_MALE1:
		voicename = g_strdup("male1");
		break;
	case SPD_MALE2:
		voicename = g_strdup("male2");
		break;
	case SPD_MALE3:
		voicename = g_strdup("male3");
		break;
	case SPD_FEMALE1:
		voicename = g_strdup("female1");
		break;
	case SPD_FEMALE2:
		voicename = g_strdup("female2");
		break;
	case SPD_FEMALE3:
		voicename = g_strdup("female3");
		break;
	case SPD_CHILD_MALE:
		voicename = g_strdup("child_male");
		break;
	case SPD_CHILD_FEMALE:
		voicename = g_strdup("child_female");
		break;
	default:
		voicename = g_strdup("no voice");
		break;
	}
	return voicename;
}

/* Given a language, dialect and SD voice codes sets the IBM voice */
static void
ibmtts_set_language_and_voice(char *lang, SPDVoiceType voice, char *variant)
{
	char *variant_name = variant;
	char *voicename = ibmtts_voice_enum_to_str(voice);
	int eciVoice;
	int ret = -1;
	int i = 0;
	int j = 0;

	DBG("Ibmtts: %s, lang=%s, voice=%d, dialect=%s",
	    __FUNCTION__, lang, (int)voice, variant ? variant : NULL);

	SPDVoice **v = ibmtts_voice_list;
	assert(v);

	if (variant_name) {
		for (i = 0; v[i]; i++) {
			DBG("%d. variant=%s", i, v[i]->variant);
			if (!strcmp(v[i]->variant, variant_name)) {
				j = ibmtts_voice_index[i];
				ret =
				    eciSetParam(eciHandle, eciLanguageDialect,
						eciLocales[j].langID);
				DBG("Ibmtts: set langID=0x%x (ret=%d)",
				    eciLocales[j].langID, ret);
				ibmtts_input_encoding = eciLocales[j].charset;
				break;
			}
		}
	} else {
		for (i = 0; v[i]; i++) {
			DBG("%d. language=%s", i, v[i]->language);
			if (!strcmp(v[i]->language, lang)) {
				j = ibmtts_voice_index[i];
				variant_name = v[i]->name;
				ret =
				    eciSetParam(eciHandle, eciLanguageDialect,
						eciLocales[j].langID);
				DBG("Ibmtts: set langID=0x%x (ret=%d)",
				    eciLocales[j].langID, ret);
				ibmtts_input_encoding = eciLocales[j].charset;
				break;
			}
		}
	}

	if (-1 == ret) {
		DBG("Ibmtts: Unable to set language");
		ibmtts_log_eci_error();
	} else {
		g_atomic_int_set(&locale_index_atomic, j);
	}

	/* Set voice parameters (if any are defined for this voice.) */
	TIbmttsVoiceParameters *params =
	    g_hash_table_lookup(IbmttsVoiceParameters, voicename);
	if (NULL == params) {
		DBG("Ibmtts: Setting default VoiceParameters for voice %s",
		    voicename);
		switch (voice) {
		case SPD_MALE1:
			eciVoice = 1;
			break;	/* Adult Male 1 */
		case SPD_MALE2:
			eciVoice = 4;
			break;	/* Adult Male 2 */
		case SPD_MALE3:
			eciVoice = 5;
			break;	/* Adult Male 3 */
		case SPD_FEMALE1:
			eciVoice = 2;
			break;	/* Adult Female 1 */
		case SPD_FEMALE2:
			eciVoice = 6;
			break;	/* Adult Female 2 */
		case SPD_FEMALE3:
			eciVoice = 7;
			break;	/* Elderly Female 1 */
		case SPD_CHILD_MALE:
			eciVoice = 3;
			break;	/* Child */
		case SPD_CHILD_FEMALE:
			eciVoice = 3;
			break;	/* Child */
		default:
			eciVoice = 1;
			break;	/* Adult Male 1 */
		}
		ret = eciCopyVoice(eciHandle, eciVoice, 0);
		if (-1 == ret)
			DBG("Ibmtts: ERROR: Setting default voice parameters (voice %i).", eciVoice);
	} else {
		DBG("Ibmtts: Setting custom VoiceParameters for voice %s",
		    voicename);
		ret = eciSetVoiceParam(eciHandle, 0, eciGender, params->gender);
		if (-1 == ret)
			DBG("Ibmtts: ERROR: Setting gender %i", params->gender);
		ret =
		    eciSetVoiceParam(eciHandle, 0, eciBreathiness,
				     params->breathiness);
		if (-1 == ret)
			DBG("Ibmtts: ERROR: Setting breathiness %i",
			    params->breathiness);
		ret =
		    eciSetVoiceParam(eciHandle, 0, eciHeadSize,
				     params->head_size);
		if (-1 == ret)
			DBG("Ibmtts: ERROR: Setting head size %i",
			    params->head_size);
		ret =
		    eciSetVoiceParam(eciHandle, 0, eciPitchBaseline,
				     params->pitch_baseline);
		if (-1 == ret)
			DBG("Ibmtts: ERROR: Setting pitch baseline %i",
			    params->pitch_baseline);
		ret =
		    eciSetVoiceParam(eciHandle, 0, eciPitchFluctuation,
				     params->pitch_fluctuation);
		if (-1 == ret)
			DBG("Ibmtts: ERROR: Setting pitch fluctuation %i",
			    params->pitch_fluctuation);
		ret =
		    eciSetVoiceParam(eciHandle, 0, eciRoughness,
				     params->roughness);
		if (-1 == ret)
			DBG("Ibmtts: ERROR: Setting roughness %i",
			    params->roughness);
		ret = eciSetVoiceParam(eciHandle, 0, eciSpeed, params->speed);
		if (-1 == ret)
			DBG("Ibmtts: ERROR: Setting speed %i", params->speed);
	}
	g_free(voicename);
	/* Retrieve the baseline pitch and speed of the voice. */
	ibmtts_voice_pitch_baseline =
	    eciGetVoiceParam(eciHandle, 0, eciPitchBaseline);
	if (-1 == ibmtts_voice_pitch_baseline)
		DBG("Ibmtts: Cannot get pitch baseline of voice.");
	ibmtts_voice_speed = eciGetVoiceParam(eciHandle, 0, eciSpeed);
	if (-1 == ibmtts_voice_speed)
		DBG("Ibmtts: Cannot get speed of voice.");
}

static void ibmtts_set_voice(SPDVoiceType voice)
{
	if (msg_settings.voice.language) {
		ibmtts_set_language_and_voice(msg_settings.voice.language,
					      voice, NULL);
	}
}

static void ibmtts_set_language(char *lang)
{
	ibmtts_set_language_and_voice(lang, msg_settings.voice_type, NULL);
}

/* sets the IBM voice according to its name. */
static void ibmtts_set_synthesis_voice(char *synthesis_voice)
{
	int i = 0;

	if (synthesis_voice == NULL) {
		return;
	}

	DBG("Ibmtts: %s, synthesis voice=%s", __FUNCTION__, synthesis_voice);

	for (i = 0; i < MAX_NB_OF_LANGUAGES; i++) {
		if (!strcasecmp(eciLocales[i].name, synthesis_voice)) {
			ibmtts_set_language_and_voice(eciLocales[i].lang,
						      msg_settings.voice_type,
						      eciLocales[i].variant);
			break;
		}
	}

}

static void ibmtts_log_eci_error()
{
	/* TODO: This routine is not working.  Not sure why. */
	char buf[100];
	eciErrorMessage(eciHandle, buf);
	DBG("Ibmtts: ECI Error Message: %s", buf);
}

/* IBM TTS calls back here when a chunk of audio is ready or an index mark
   has been reached.  The good news is that it returns the audio up to
   each index mark or when the audio buffer is full. */
static enum ECICallbackReturn eciCallback(ECIHand hEngine,
					  enum ECIMessage msg,
					  long lparam, void *data)
{
	/* This callback is running in the same thread as called eciSynchronize(),
	   i.e., the _ibmtts_synth() thread. */

	/* If module_stop was called, discard any further callbacks until module_speak is called. */
	if (ibmtts_stop_synth_requested || ibmtts_stop_play_requested)
		return eciDataProcessed;

	switch (msg) {
	case eciWaveformBuffer:
		DBG("Ibmtts: %ld audio samples returned from IBM TTS.", lparam);
		/* Add audio to output queue. */
		ibmtts_add_audio_to_playback_queue(audio_chunk, lparam);
		/* Wake up the audio playback thread, if not already awake. */
		if (!is_thread_busy(&ibmtts_play_suspended_mutex))
			sem_post(&ibmtts_play_semaphore);
		return eciDataProcessed;
		break;
	case eciIndexReply:
		DBG("Ibmtts: Index mark id %ld returned from IBM TTS.", lparam);
		if (lparam == IBMTTS_MSG_END_MARK) {
			ibmtts_add_flag_to_playback_queue(IBMTTS_QET_END);
		} else {
			/* Add index mark to output queue. */
			ibmtts_add_mark_to_playback_queue(lparam);
		}
		/* Wake up the audio playback thread, if not already awake. */
		if (!is_thread_busy(&ibmtts_play_suspended_mutex))
			sem_post(&ibmtts_play_semaphore);
		return eciDataProcessed;
		break;
	default:
		return eciDataProcessed;
	}
}

/* Adds a chunk of pcm audio to the audio playback queue. */
static TIbmttsBool
ibmtts_add_audio_to_playback_queue(TEciAudioSamples * audio_chunk,
				   long num_samples)
{
	TPlaybackQueueEntry *playback_queue_entry =
	    (TPlaybackQueueEntry *) g_malloc(sizeof(TPlaybackQueueEntry));
	if (NULL == playback_queue_entry)
		return IBMTTS_FALSE;
	playback_queue_entry->type = IBMTTS_QET_AUDIO;
	playback_queue_entry->data.audio.num_samples = (int)num_samples;
	int wlen = sizeof(TEciAudioSamples) * num_samples;
	playback_queue_entry->data.audio.audio_chunk =
	    (TEciAudioSamples *) g_malloc(wlen);
	memcpy(playback_queue_entry->data.audio.audio_chunk, audio_chunk, wlen);
	pthread_mutex_lock(&playback_queue_mutex);
	playback_queue = g_slist_append(playback_queue, playback_queue_entry);
	pthread_mutex_unlock(&playback_queue_mutex);
	return IBMTTS_TRUE;
}

/* Adds an Index Mark to the audio playback queue. */
static TIbmttsBool ibmtts_add_mark_to_playback_queue(long markId)
{
	TPlaybackQueueEntry *playback_queue_entry =
	    (TPlaybackQueueEntry *) g_malloc(sizeof(TPlaybackQueueEntry));
	if (NULL == playback_queue_entry)
		return IBMTTS_FALSE;
	playback_queue_entry->type = IBMTTS_QET_INDEX_MARK;
	playback_queue_entry->data.markId = markId;
	pthread_mutex_lock(&playback_queue_mutex);
	playback_queue = g_slist_append(playback_queue, playback_queue_entry);
	pthread_mutex_unlock(&playback_queue_mutex);
	return IBMTTS_TRUE;
}

/* Adds a begin or end flag to the playback queue. */
static TIbmttsBool
ibmtts_add_flag_to_playback_queue(EPlaybackQueueEntryType type)
{
	TPlaybackQueueEntry *playback_queue_entry =
	    (TPlaybackQueueEntry *) g_malloc(sizeof(TPlaybackQueueEntry));
	if (NULL == playback_queue_entry)
		return IBMTTS_FALSE;
	playback_queue_entry->type = type;
	pthread_mutex_lock(&playback_queue_mutex);
	playback_queue = g_slist_append(playback_queue, playback_queue_entry);
	pthread_mutex_unlock(&playback_queue_mutex);
	return IBMTTS_TRUE;
}

/* Add a sound icon to the playback queue. */
static TIbmttsBool ibmtts_add_sound_icon_to_playback_queue(char *filename)
{
	TPlaybackQueueEntry *playback_queue_entry =
	    (TPlaybackQueueEntry *) g_malloc(sizeof(TPlaybackQueueEntry));
	if (NULL == playback_queue_entry)
		return IBMTTS_FALSE;
	playback_queue_entry->type = IBMTTS_QET_SOUND_ICON;
	playback_queue_entry->data.sound_icon_filename = filename;
	pthread_mutex_lock(&playback_queue_mutex);
	playback_queue = g_slist_append(playback_queue, playback_queue_entry);
	pthread_mutex_unlock(&playback_queue_mutex);
	return IBMTTS_TRUE;
}

/* Deletes an entry from the playback audio queue, freeing memory. */
static void
ibmtts_delete_playback_queue_entry(TPlaybackQueueEntry * playback_queue_entry)
{
	switch (playback_queue_entry->type) {
	case IBMTTS_QET_AUDIO:
		g_free(playback_queue_entry->data.audio.audio_chunk);
		break;
	case IBMTTS_QET_SOUND_ICON:
		g_free(playback_queue_entry->data.sound_icon_filename);
		break;
	default:
		break;
	}
	g_free(playback_queue_entry);
}

/* Erases the entire playback queue, freeing memory. */
static void ibmtts_clear_playback_queue()
{
	pthread_mutex_lock(&playback_queue_mutex);
	while (NULL != playback_queue) {
		TPlaybackQueueEntry *playback_queue_entry =
		    playback_queue->data;
		ibmtts_delete_playback_queue_entry(playback_queue_entry);
		playback_queue =
		    g_slist_remove(playback_queue, playback_queue->data);
	}
	playback_queue = NULL;
	pthread_mutex_unlock(&playback_queue_mutex);
}

/* Sends a chunk of audio to the audio player and waits for completion or error. */
static TIbmttsBool
ibmtts_send_to_audio(TPlaybackQueueEntry * playback_queue_entry)
{
	AudioTrack track;
#if defined(BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)
	AudioFormat format = SPD_AUDIO_BE;
#else
	AudioFormat format = SPD_AUDIO_LE;
#endif
	int ret;

	track.num_samples = playback_queue_entry->data.audio.num_samples;
	track.num_channels = 1;
	track.sample_rate = eci_sample_rate;
	track.bits = 16;
	track.samples = playback_queue_entry->data.audio.audio_chunk;

	if (track.samples == NULL)
		return IBMTTS_TRUE;

	DBG("Ibmtts: Sending %i samples to audio.", track.num_samples);
	ret = module_tts_output(track, format);
	if (ret < 0) {
		DBG("ERROR: Can't play track for unknown reason.");
		return IBMTTS_FALSE;
	}
	DBG("Ibmtts: Sent to audio.");
	return IBMTTS_TRUE;
}

/* Playback thread. */
static void *_ibmtts_play(void *nothing)
{
	int markId;
	char *mark_name;
	TPlaybackQueueEntry *playback_queue_entry = NULL;

	DBG("Ibmtts: Playback thread starting.......\n");

	/* Block all signals to this thread. */
	set_speaking_thread_parameters();

	while (!ibmtts_thread_exit_requested) {
		/* If semaphore not set, set suspended lock and suspend until it is signaled. */
		if (0 != sem_trywait(&ibmtts_play_semaphore)) {
			pthread_mutex_lock(&ibmtts_play_suspended_mutex);
			sem_wait(&ibmtts_play_semaphore);
			pthread_mutex_unlock(&ibmtts_play_suspended_mutex);
		}
		/* DBG("Ibmtts: Playback semaphore on."); */

		while (!ibmtts_stop_play_requested
		       && !ibmtts_thread_exit_requested) {
			pthread_mutex_lock(&playback_queue_mutex);
			if (NULL != playback_queue) {
				playback_queue_entry = playback_queue->data;
				playback_queue =
				    g_slist_remove(playback_queue,
						   playback_queue->data);
			}
			pthread_mutex_unlock(&playback_queue_mutex);
			if (NULL == playback_queue_entry)
				break;

			switch (playback_queue_entry->type) {
			case IBMTTS_QET_AUDIO:
				ibmtts_send_to_audio(playback_queue_entry);
				break;
			case IBMTTS_QET_INDEX_MARK:
				/* Look up the index mark integer id in lookup table to
				   find string name and emit that name. */
				markId = playback_queue_entry->data.markId;
				mark_name =
				    g_hash_table_lookup(ibmtts_index_mark_ht,
							&markId);
				if (NULL == mark_name) {
					DBG("Ibmtts: markId %d returned by IBM TTS not found in lookup table.", markId);
				} else {
					DBG("Ibmtts: reporting index mark |%s|.", mark_name);
					module_report_index_mark(mark_name);
					DBG("Ibmtts: index mark reported.");
					/* If pause requested, wait for an end-of-sentence index mark. */
					if (ibmtts_pause_requested) {
						if (0 ==
						    strncmp(mark_name,
							    SD_MARK_BODY,
							    SD_MARK_BODY_LEN)) {
							DBG("Ibmtts: Pause requested in playback thread.  Stopping.");
							ibmtts_stop_play_requested
							    = IBMTTS_TRUE;
						}
					}
				}
				break;
			case IBMTTS_QET_SOUND_ICON:
				module_play_file(playback_queue_entry->
						 data.sound_icon_filename);
				break;
			case IBMTTS_QET_BEGIN:
				module_report_event_begin();
				break;
			case IBMTTS_QET_END:
				module_report_event_end();
				break;
			}

			ibmtts_delete_playback_queue_entry
			    (playback_queue_entry);
			playback_queue_entry = NULL;
		}
		if (ibmtts_stop_play_requested)
			DBG("Ibmtts: Stop or pause in playback thread.");
	}

	DBG("Ibmtts: Playback thread ended.......\n");

	pthread_exit(NULL);
}

/* Replaces all occurrences of "from" with "to" in msg.
   Returns count of replacements. */
static int ibmtts_replace(char *from, char *to, GString * msg)
{
	int count = 0;
	int pos;
	int from_len = strlen(from);
	int to_len = strlen(to);
	char *p = msg->str;
	while (NULL != (p = strstr(p, from))) {
		pos = p - msg->str;
		g_string_erase(msg, pos, from_len);
		g_string_insert(msg, pos, to);
		p = msg->str + pos + to_len;
		++count;
	}
	return count;
}

static void ibmtts_subst_keys_cb(gpointer data, gpointer user_data)
{
	TIbmttsKeySubstitution *key_subst = data;
	GString *msg = user_data;
	ibmtts_replace(key_subst->key, key_subst->newkey, msg);
}

/* Given a Speech Dispatcher !KEY key sequence, replaces unspeakable
   or incorrectly spoken keys or characters with speakable ones.
   The subsitutions come from the KEY NAME SUBSTITUTIONS section of the
   config file.
   Caller is responsible for freeing returned string. */
static char *ibmtts_subst_keys(char *key)
{
	GString *tmp = g_string_sized_new(30);
	g_string_append(tmp, key);

	GList *keyTable = g_hash_table_lookup(IbmttsKeySubstitution,
					      msg_settings.voice.language);

	if (keyTable)
		g_list_foreach(keyTable, ibmtts_subst_keys_cb, tmp);

	/* Hyphen hangs IBM TTS */
	if (0 == strcmp(tmp->str, "-"))
		g_string_assign(tmp, "hyphen");

	return g_string_free(tmp, FALSE);
}

/* Given a sound icon name, searches for a file to play and if found
   returns the filename.  Returns NULL if none found.  Caller is responsible
   for freeing the returned string. */
/* TODO: These current assumptions should be dealt with:
        Sound icon files are in a single directory (IbmttsSoundIconFolder).
        The name of each icon is symlinked to a .wav file.
   If you have installed the free(b)soft sound-icons package under
   Debian, then these assumptions are true, but what about other distros
   and OSes? */
static char *ibmtts_search_for_sound_icon(const char *icon_name)
{
	char *fn = NULL;
	if (0 == strlen(IbmttsSoundIconFolder))
		return fn;
	GString *filename = g_string_new(IbmttsSoundIconFolder);
	filename = g_string_append(filename, icon_name);
	if (g_file_test(filename->str, G_FILE_TEST_EXISTS))
		fn = filename->str;
	/*
	   else {
	   filename = g_string_assign(filename, g_utf8_strdown(filename->str, -1));
	   if (g_file_test(filename->str, G_FILE_TEST_EXISTS))
	   fn = filename->str;
	   }
	 */

	/*
	 * if the file was found, the pointer *fn  points to the character data
	 * of the string filename. In this situation the string filename must be
	 * freed but its character data must be preserved.
	 * If the file is not found, the pointer *fn contains NULL. In this
	 * situation the string filename must be freed, including its character
	 * data.
	 */
	g_string_free(filename, (fn == NULL));
	return fn;
}

void alloc_voice_list()
{
	enum ECILanguageDialect aLanguage[MAX_NB_OF_LANGUAGES];
	int nLanguages = MAX_NB_OF_LANGUAGES;
	int i = 0;

	if (eciGetAvailableLanguages(aLanguage, &nLanguages))
		return;

	ibmtts_voice_list = g_malloc((nLanguages + 1) * sizeof(SPDVoice *));
	ibmtts_voice_index = g_malloc((nLanguages + 1) * sizeof(SPDVoice *));
	if (!ibmtts_voice_list)
		return;

	DBG("Ibmtts: nLanguages=%d/%lu", nLanguages, (unsigned long)MAX_NB_OF_LANGUAGES);
	for (i = 0; i < nLanguages; i++) {
		/* look for the language name */
		int j;
		ibmtts_voice_list[i] = g_malloc(sizeof(SPDVoice));

		DBG("Ibmtts: aLanguage[%d]=0x%08x", i, aLanguage[i]);
		for (j = 0; j < MAX_NB_OF_LANGUAGES; j++) {
			DBG("Ibmtts: eciLocales[%d].langID=0x%08x", j,
			    eciLocales[j].langID);
			if (eciLocales[j].langID == aLanguage[i]) {
				ibmtts_voice_list[i]->name = eciLocales[j].name;
				ibmtts_voice_list[i]->language =
				    eciLocales[j].lang;
				ibmtts_voice_list[i]->variant =
				    eciLocales[j].variant;
				ibmtts_voice_index[i] = j;
				DBG("Ibmtts: alloc_voice_list %s",
				    ibmtts_voice_list[i]->name);
				break;
			}
		}
		assert(j < MAX_NB_OF_LANGUAGES);
	}
	ibmtts_voice_list[nLanguages] = NULL;
	DBG("Ibmtts: LEAVE %s", __func__);
}

static void free_voice_list()
{
	int i = 0;

	if (ibmtts_voice_index) {
		g_free(ibmtts_voice_index);
		ibmtts_voice_index = NULL;
	}

	if (!ibmtts_voice_list)
		return;

	for (i = 0; ibmtts_voice_list[i]; i++) {
		g_free(ibmtts_voice_list[i]);
	}

	g_free(ibmtts_voice_list);
	ibmtts_voice_list = NULL;
}

static void ibmtts_load_user_dictionary()
{
	GString *dirname = NULL;
	GString *filename = NULL;
	int i = 0;
	int dictionary_is_present = 0;
	static guint old_index = MAX_NB_OF_LANGUAGES;
	guint new_index;
	char *language = NULL, *dash;
	ECIDictHand eciDict = eciGetDict(eciHandle);

	new_index = g_atomic_int_get(&locale_index_atomic);
	if (new_index >= MAX_NB_OF_LANGUAGES) {
		DBG("Ibmtts: %s, unexpected index (0x%x)", __FUNCTION__,
		    new_index);
		return;
	}

	if (old_index == new_index) {
		DBG("Ibmtts: LEAVE %s, no change", __FUNCTION__);
		return;
	}

	language = g_strdup(eciLocales[new_index].lang);
	dash = strchr(language, '-');
	if (dash)
		*dash = '_';

	if (eciDict) {
		DBG("Ibmtts: delete old dictionary");
		eciDeleteDict(eciHandle, eciDict);
	}
	eciDict = eciNewDict(eciHandle);
	if (eciDict) {
		old_index = new_index;
	} else {
		old_index = MAX_NB_OF_LANGUAGES;
		DBG("Ibmtts: can't create new dictionary");
		g_free(language);
		return;
	}

	/* Look for the dictionary directory */
	dirname = g_string_new(NULL);
	g_string_printf(dirname, "%s/%s", IbmttsDictionaryFolder, language);
	if (!g_file_test(dirname->str, G_FILE_TEST_IS_DIR) && dash) {
		*dash = 0;
		g_string_printf(dirname, "%s/%s", IbmttsDictionaryFolder, language);
		if (!g_file_test(dirname->str, G_FILE_TEST_IS_DIR)) {
			g_string_printf(dirname, "%s", IbmttsDictionaryFolder);
			if (!g_file_test(dirname->str, G_FILE_TEST_IS_DIR)) {
				DBG("Ibmtts: %s is not a directory",
				    dirname->str);
				g_free(language);
				return;
			}
		}
	}
	g_free(language);

	DBG("Ibmtts: Looking in dictionary directory %s", dirname->str);
	filename = g_string_new(NULL);

	for (i = 0; i < NB_OF_DICTIONARY_FILENAMES; i++) {
		g_string_printf(filename, "%s/%s", dirname->str,
				dictionary_filenames[i]);
		if (g_file_test(filename->str, G_FILE_TEST_EXISTS)) {
			enum ECIDictError error =
			    eciLoadDict(eciHandle, eciDict, i, filename->str);
			if (!error) {
				dictionary_is_present = 1;
				DBG("Ibmtts: %s dictionary loaded",
				    filename->str);
			} else {
				DBG("Ibmtts: Can't load %s dictionary (%d)",
				    filename->str, error);
			}
		} else {
			DBG("Ibmtts: No %s dictionary", filename->str);
		}
	}

	g_string_free(filename, TRUE);
	g_string_free(dirname, TRUE);

	if (dictionary_is_present) {
		eciSetDict(eciHandle, eciDict);
	}
}
