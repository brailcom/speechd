/*
 * voxin.c - Speech Dispatcher backend for Voxin (version >= 3)
 *
 * Copyright (C) 2006, 2007 Brailcom, o.p.s.
 * Copyright (C) 2020 Gilles Casse <gcasse@oralux.org>
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
 * Based on ibmtts.c:
 * @author  Gary Cramblitt <garycramblitt@comcast.net> (original author)
 *
 * $Id: ibmtts.c,v 1.30 2008-06-30 14:34:02 gcasse Exp $
 */

/* This output module operates with four threads:
   
   The main thread called from Speech Dispatcher (module_*()).
   A synthesis thread that accepts messages, parses them, and forwards
   them to Voxin.
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
#include <ctype.h>

/* Speech Dispatcher includes. */
#include "spd_audio.h"
#include <speechd_types.h>
#include "module_utils.h"

/* Voxin include */
#include "voxin.h"

typedef enum {
	MODULE_FATAL_ERROR = -1,
	MODULE_OK = 0,
	MODULE_ERROR = 1
} module_status;

/* TODO: These defines are in src/server/index_marking.h, but including that
   file here causes a redefinition error on FATAL macro in speechd.h. */
#define SD_MARK_BODY_LEN 6
#define SD_MARK_BODY "__spd_"
#define SD_MARK_HEAD "<mark name=\""SD_MARK_BODY
#define SD_MARK_TAIL "\"/>"

#define SD_MARK_HEAD_ONLY "<mark name=\""
#define SD_MARK_HEAD_ONLY_LEN 12
#define SD_MARK_TAIL_LEN 3

#define MODULE_NAME     "voxin"
#define DBG_MODNAME     "Voxin: "
#define MODULE_VERSION  "0.1"

#define DEBUG_MODULE 1
DECLARE_DEBUG();

/* Define a hash table where each entry is a double-linked list
   loaded from the config file.  Each entry in the config file
   is 3 strings, where the 1st string is used to access a list
   of the 2nd and 3rd strings. */
#define MOD_OPTION_3_STR_HT_DLL(name, arg1, arg2, arg3)			\
	typedef struct{							\
		char* arg2;						\
		char* arg3;						\
	}T ## name;							\
	GHashTable *name;						\
									\
	DOTCONF_CB(name ## _cb)						\
	{								\
		T ## name *new_item;					\
		char *new_key;						\
		GList *dll = NULL;					\
		new_item = (T ## name *) g_malloc(sizeof(T ## name));	\
		new_key = g_strdup(cmd->data.list[0]);			\
		if (NULL != cmd->data.list[1])				\
			new_item->arg2 = g_strdup(cmd->data.list[1]);	\
		else							\
			new_item->arg2 = NULL;				\
		if (NULL != cmd->data.list[2])				\
			new_item->arg3 = g_strdup(cmd->data.list[2]);	\
		else							\
			new_item->arg3 = NULL;				\
		dll = g_hash_table_lookup(name, new_key);		\
		dll = g_list_append(dll, new_item);			\
		g_hash_table_insert(name, new_key, dll);		\
		return NULL;						\
	}

/* Load a double-linked list from config file. */
#define MOD_OPTION_HT_DLL_REG(name)					\
	name = g_hash_table_new(g_str_hash, g_str_equal);		\
	module_dc_options = module_add_config_option(module_dc_options, \
						     &module_num_dc_options, #name, \
						     ARG_LIST, name ## _cb, NULL, 0);

/* Define a hash table mapping a string to 7 integer values. */
#define MOD_OPTION_6_INT_HT(name, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
	typedef struct{							\
		int arg1;						\
		int arg2;						\
		int arg3;						\
		int arg4;						\
		int arg5;						\
		int arg6;						\
		int arg7;						\
	}T ## name;							\
	GHashTable *name;						\
									\
	DOTCONF_CB(name ## _cb)						\
	{								\
		T ## name *new_item;					\
		char* new_key;						\
		new_item = (T ## name *) g_malloc(sizeof(T ## name));	\
		if (cmd->data.list[0] == NULL) return NULL;		\
		new_key = g_strdup(cmd->data.list[0]);			\
		new_item->arg1 = (int) strtol(cmd->data.list[1], NULL, 10); \
		new_item->arg2 = (int) strtol(cmd->data.list[2], NULL, 10); \
		new_item->arg3 = (int) strtol(cmd->data.list[3], NULL, 10); \
		new_item->arg4 = (int) strtol(cmd->data.list[4], NULL, 10); \
		new_item->arg5 = (int) strtol(cmd->data.list[5], NULL, 10); \
		new_item->arg6 = (int) strtol(cmd->data.list[6], NULL, 10); \
		new_item->arg7 = (int) strtol(cmd->data.list[7], NULL, 10); \
		g_hash_table_insert(name, new_key, new_item);		\
		return NULL;						\
	}

/* Thread and process control. */
static pthread_t synth_thread;
static pthread_t play_thread;
static pthread_t stop_or_pause_thread;

static sem_t synth_semaphore;
static sem_t play_semaphore;
static sem_t stop_or_pause_semaphore;

static pthread_mutex_t synth_suspended_mutex;
static pthread_mutex_t play_suspended_mutex;
static pthread_mutex_t stop_or_pause_suspended_mutex;

static gboolean thread_exit_requested = FALSE;
static gboolean stop_synth_requested = FALSE;
static gboolean stop_play_requested = FALSE;
static gboolean pause_requested = FALSE;

/* Current message from Speech Dispatcher. */
static char **message;
static SPDMessageType message_type;

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
	QET_AUDIO,	/* Chunk of audio. */
	QET_INDEX_MARK,	/* Index mark event. */
	QET_SOUND_ICON,	/* A Sound Icon */
	QET_BEGIN,	/* Beginning of speech. */
	QET_END		/* Speech completed. */
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
static GHashTable *index_mark_ht = NULL;
#define MSG_END_MARK 0

static pthread_mutex_t sound_stop_mutex;

/* When a voice is set, this is the baseline pitch of the voice.
   SSIP PITCH commands then adjust relative to this. */
static int voice_pitch_baseline;
/* When a voice is set, this the default speed of the voice.
   SSIP RATE commands then adjust relative to this. */
static int voice_speed;

/* Expected input encoding for current language */
static char *input_encoding = "utf-8";

/* list of speechd voices */
static SPDVoice **speechd_voice = NULL;

/* Internal function prototypes for main thread. */
static void update_sample_rate();
static void set_language(char *lang);
static void set_voice_type(SPDVoiceType voice_type);
static char *voice_enum_to_str(SPDVoiceType voice);
static void set_language_and_voice(char *lang, SPDVoiceType voice_type, char *name);
static void set_synthesis_voice(char *);
static void set_rate(signed int rate);
static void set_pitch(signed int pitch);
static void set_punctuation_mode(SPDPunctuation punct_mode);
static void set_volume(signed int pitch);

/* locale_index_atomic stores the current index of the voices array.
   The main thread writes this information, the synthesis thread reads it.
*/
static gint locale_index_atomic;

/* Internal function prototypes for synthesis thread. */
static char *extract_mark_name(char *mark);
static char *next_part(char *msg, char **mark_name);
static int replace(char *from, char *to, GString * msg);
static void subst_keys_cb(gpointer data, gpointer user_data);
static char *subst_keys(char *key);
static char *search_for_sound_icon(const char *icon_name);
static gboolean add_sound_icon_to_playback_queue(char *filename);
static void load_user_dictionary();

static enum ECICallbackReturn eciCallback(ECIHand hEngine,
					  enum ECIMessage msg,
					  long lparam, void *data);

/* Internal function prototypes for playback thread. */
static gboolean add_audio_to_playback_queue(TEciAudioSamples *
						   audio_chunk,
						   long num_samples);
static gboolean add_mark_to_playback_queue(long markId);
static gboolean add_flag_to_playback_queue(EPlaybackQueueEntryType
						  type);
static void delete_playback_queue_entry(TPlaybackQueueEntry *
					       playback_queue_entry);
static gboolean send_to_audio(TPlaybackQueueEntry *
				     playback_queue_entry);

/* Miscellaneous internal function prototypes. */
static gboolean is_thread_busy(pthread_mutex_t * suspended_mutex);
static void log_eci_error();
static void clear_playback_queue();
static gboolean alloc_voice_list();
static void free_voice_list();

/* The synthesis thread start routine. */
static void *_synth(void *);
/* The playback thread start routine. */
static void *_play(void *);
/* The stop_or_pause start routine. */
static void *_stop_or_pause(void *);

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

/* Array of installed voices returned by voxGetVoices() */
static vox_t *voices;
static unsigned int number_of_voices;

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

	return MODULE_OK;
}

int module_init(char **status_info)
{
	int ret;
	char version[20];

	DBG(DBG_MODNAME "Module init().");
	INIT_INDEX_MARKING();

	*status_info = NULL;
	thread_exit_requested = FALSE;
	
	/* Report versions. */
	eciVersion(version);
	DBG(DBG_MODNAME "output module version %s, engine version %s", MODULE_VERSION, version);

	/* Setup TTS engine. */
	DBG(DBG_MODNAME "Creating an engine instance.");
	eciHandle = eciNew();
	if (NULL_ECI_HAND == eciHandle) {
		DBG(DBG_MODNAME "Could not create an engine instance.\n");
		*status_info = g_strdup("Could not create an engine instance. "
					"Is the TTS engine installed?");
		return MODULE_FATAL_ERROR;
	}

	update_sample_rate();

	/* Allocate a chunk for ECI to return audio. */
	audio_chunk =
		(TEciAudioSamples *) g_malloc((IbmttsAudioChunkSize) *
					      sizeof(TEciAudioSamples));

	DBG(DBG_MODNAME "Registering ECI callback.");
	eciRegisterCallback(eciHandle, eciCallback, NULL);

	DBG(DBG_MODNAME "Registering an ECI audio buffer.");
	if (!eciSetOutputBuffer(eciHandle, IbmttsAudioChunkSize, audio_chunk)) {
		DBG(DBG_MODNAME "Error registering ECI audio buffer.");
		log_eci_error();
	}

	eciSetParam(eciHandle, eciDictionary, !IbmttsUseAbbreviation);

	/* enable annotations */
	eciSetParam(eciHandle, eciInputType, 1);

	/* load possibly the ssml filter */
	eciAddText(eciHandle, " `gfa1 ");

	/* load possibly the punctuation filter */
	eciAddText(eciHandle, " `gfa2 ");

	set_punctuation_mode(msg_settings.punctuation_mode);

	if (!alloc_voice_list()) {
		DBG(DBG_MODNAME "voice list allocation failed.");
		*status_info =
			g_strdup
			("The module can't build the list of installed voices.");
		return MODULE_FATAL_ERROR;
	}

	/* These mutexes are locked when the corresponding threads are suspended. */
	pthread_mutex_init(&synth_suspended_mutex, NULL);
	pthread_mutex_init(&play_suspended_mutex, NULL);

	/* This mutex is used to hold a stop request until audio actually stops. */
	pthread_mutex_init(&sound_stop_mutex, NULL);

	/* This mutex mediates access to the playback queue between the synthesis and
	   playback threads. */
	pthread_mutex_init(&playback_queue_mutex, NULL);

	DBG(DBG_MODNAME "IbmttsAudioChunkSize = %d", IbmttsAudioChunkSize);

	message = g_malloc(sizeof(char *));
	*message = NULL;

	DBG(DBG_MODNAME "Creating new thread for stop or pause.");
	sem_init(&stop_or_pause_semaphore, 0, 0);

	ret = pthread_create(&stop_or_pause_thread, NULL,
			     _stop_or_pause, NULL);
	if (0 != ret) {
		DBG(DBG_MODNAME "stop or pause thread creation failed.");
		*status_info =
			g_strdup
			("The module couldn't initialize stop or pause thread. "
			 "This could be either an internal problem or an "
			 "architecture problem. If you are sure your architecture "
			 "supports threads, please report a bug.");
		return MODULE_FATAL_ERROR;
	}

	DBG(DBG_MODNAME "Creating new thread for playback.");
	sem_init(&play_semaphore, 0, 0);

	ret = pthread_create(&play_thread, NULL, _play, NULL);
	if (0 != ret) {
		DBG(DBG_MODNAME "play thread creation failed.");
		*status_info =
			g_strdup("The module couldn't initialize play thread. "
				 "This could be either an internal problem or an "
				 "architecture problem. If you are sure your architecture "
				 "supports threads, please report a bug.");
		return MODULE_FATAL_ERROR;
	}

	DBG(DBG_MODNAME "Creating new thread for TTS synthesis.");
	sem_init(&synth_semaphore, 0, 0);

	ret = pthread_create(&synth_thread, NULL, _synth, NULL);
	if (0 != ret) {
		DBG(DBG_MODNAME "synthesis thread creation failed.");
		*status_info =
			g_strdup("The module couldn't initialize synthesis thread. "
				 "This could be either an internal problem or an "
				 "architecture problem. If you are sure your architecture "
				 "supports threads, please report a bug.");
		return MODULE_FATAL_ERROR;
	}

	*status_info = g_strdup(DBG_MODNAME "Initialized successfully.");

	return MODULE_OK;
}

SPDVoice **module_list_voices(void)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	return speechd_voice;
}

int module_speak(gchar * data, size_t bytes, SPDMessageType msgtype)
{
	DBG(DBG_MODNAME "module_speak().");

	if (is_thread_busy(&synth_suspended_mutex) ||
	    is_thread_busy(&play_suspended_mutex) ||
	    is_thread_busy(&stop_or_pause_suspended_mutex)) {
		DBG(DBG_MODNAME "Already synthesizing when requested to synthesize (module_speak).");
		return FALSE;
	}

	DBG(DBG_MODNAME "Type: %d, bytes: %lu, requested data: |%s|\n", msgtype,
	    (unsigned long)bytes, data);

	g_free(*message);
	*message = NULL;

	if (!g_utf8_validate(data, bytes, NULL)) {
		DBG(DBG_MODNAME "Input is not valid utf-8.");
		/* Actually, we should just fail here, but let's assume input is latin-1 */
		*message =
			g_convert(data, bytes, "utf-8", "iso-8859-1", NULL, NULL,
				  NULL);
		if (*message == NULL) {
			DBG(DBG_MODNAME "Fallback conversion to utf-8 failed.");
			return FALSE;
		}
	} else {
		*message = g_strndup(data, bytes);
	}

	message_type = msgtype;
	if ((msgtype == SPD_MSGTYPE_TEXT)
	    && (msg_settings.spelling_mode == SPD_SPELL_ON))
		message_type = SPD_MSGTYPE_SPELL;

	/* Setting speech parameters. */
	UPDATE_STRING_PARAMETER(voice.language, set_language);
	UPDATE_PARAMETER(voice_type, set_voice_type);
	UPDATE_STRING_PARAMETER(voice.name, set_synthesis_voice);
	UPDATE_PARAMETER(rate, set_rate);
	UPDATE_PARAMETER(volume, set_volume);
	UPDATE_PARAMETER(pitch, set_pitch);
	UPDATE_PARAMETER(punctuation_mode, set_punctuation_mode);

	/* TODO: Handle these in _synth() ?
	   UPDATE_PARAMETER(cap_let_recogn, festival_set_cap_let_recogn);
	*/

	if (!IbmttsUseSSML) {
		/* Strip all SSML */
		char *tmp = *message;
		*message = module_strip_ssml(*message);
		g_free(tmp);
		/* Convert input to suitable encoding for current language variant */
		tmp =
			g_convert_with_fallback(*message, -1,
						input_encoding, "utf-8", "?",
						NULL, &bytes, NULL);
		if (tmp != NULL) {
			g_free(*message);
			*message = tmp;
		}
	}

	/* Send semaphore signal to the synthesis thread */
	sem_post(&synth_semaphore);

	DBG(DBG_MODNAME "Leaving module_speak() normally.");
	return TRUE;
}

int module_stop(void)
{
	DBG(DBG_MODNAME "module_stop().");

	if ((is_thread_busy(&synth_suspended_mutex) ||
	     is_thread_busy(&play_suspended_mutex)) &&
	    !is_thread_busy(&stop_or_pause_suspended_mutex)) {

		/* Request both synth and playback threads to stop what they are doing
		   (if anything). */
		stop_synth_requested = TRUE;
		stop_play_requested = TRUE;

		/* Wake the stop_or_pause thread. */
		sem_post(&stop_or_pause_semaphore);
	}

	return MODULE_OK;
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
	DBG(DBG_MODNAME "module_pause().");

	/* Request playback thread to pause.  Note we cannot stop synthesis or
	   playback until end of sentence or end of message is played. */
	pause_requested = TRUE;

	/* Wake the stop_or_pause thread. */
	sem_post(&stop_or_pause_semaphore);

	return MODULE_OK;
}

int module_close(void)
{

	DBG(DBG_MODNAME "close().");

	if (is_thread_busy(&synth_suspended_mutex) ||
	    is_thread_busy(&play_suspended_mutex)) {
		DBG(DBG_MODNAME "Stopping speech");
		module_stop();
	}

	DBG(DBG_MODNAME "De-registering ECI callback.");
	eciRegisterCallback(eciHandle, NULL, NULL);

	DBG(DBG_MODNAME "Destroying ECI instance.");
	eciDelete(eciHandle);
	eciHandle = NULL_ECI_HAND;

	/* Free buffer for ECI audio. */
	g_free(audio_chunk);

	/* Request each thread exit and wait until it exits. */
	DBG(DBG_MODNAME "Terminating threads");
	thread_exit_requested = TRUE;
	sem_post(&synth_semaphore);
	sem_post(&play_semaphore);
	sem_post(&stop_or_pause_semaphore);
	if (0 != pthread_join(synth_thread, NULL))
		return -1;
	if (0 != pthread_join(play_thread, NULL))
		return -1;
	if (0 != pthread_join(stop_or_pause_thread, NULL))
		return -1;

	clear_playback_queue();

	/* Free index mark lookup table. */
	if (index_mark_ht) {
		g_hash_table_destroy(index_mark_ht);
		index_mark_ht = NULL;
	}

	free_voice_list();
	sem_destroy(&synth_semaphore);
	sem_destroy(&play_semaphore);
	sem_destroy(&stop_or_pause_semaphore);

	return 0;
}

/* Internal functions */

static void update_sample_rate()
{
	//	DBG(DBG_MODNAME "ENTER %s", __func__);
	int sample_rate;
	/* Get ECI audio sample rate. */
	sample_rate = eciGetParam(eciHandle, eciSampleRate);
	switch (sample_rate) {
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
		DBG(DBG_MODNAME "Invalid audio sample rate returned by ECI = %i",
		    sample_rate);
	}
	DBG(DBG_MODNAME "LEAVE %s, eci_sample_rate=%d",  __FUNCTION__, eci_sample_rate);  
}

/* Return TRUE if the thread is busy, i.e., suspended mutex is not locked. */
static gboolean is_thread_busy(pthread_mutex_t * suspended_mutex)
{
	if (EBUSY == pthread_mutex_trylock(suspended_mutex))
		return FALSE;
	else {
		pthread_mutex_unlock(suspended_mutex);
		return TRUE;
	}
}

/* Given a string containing an index mark in the form
   <mark name="some_name"/>, returns some_name.  Calling routine is
   responsible for freeing returned string. If an error occurs,
   returns NULL. */
static char *extract_mark_name(char *mark)
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
static char *next_part(char *msg, char **mark_name)
{
	char *mark_head = strstr(msg, SD_MARK_HEAD_ONLY);
	if (NULL == mark_head)
		return (char *)g_strndup(msg, strlen(msg));
	else if (mark_head == msg) {
		*mark_name = extract_mark_name(mark_head);
		if (NULL == *mark_name)
			return strcat((char *)
				      g_strndup(msg, SD_MARK_HEAD_ONLY_LEN),
				      next_part(msg +
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
static void *_stop_or_pause(void *nothing)
{
	DBG(DBG_MODNAME "Stop or pause thread starting.......\n");

	/* Block all signals to this thread. */
	set_speaking_thread_parameters();

	while (!thread_exit_requested) {
		/* If semaphore not set, set suspended lock and suspend until it is signaled. */
		if (0 != sem_trywait(&stop_or_pause_semaphore)) {
			pthread_mutex_lock
				(&stop_or_pause_suspended_mutex);
			sem_wait(&stop_or_pause_semaphore);
			pthread_mutex_unlock
				(&stop_or_pause_suspended_mutex);
			if (thread_exit_requested)
				break;
		}
		DBG(DBG_MODNAME "Stop or pause semaphore on.");
		/* The following is a hack. The condition should never
		   be true, but sometimes it is true for unclear reasons. */
		if (!(stop_synth_requested || pause_requested))
			continue;

		if (stop_synth_requested) {
			/* Stop synthesis (if in progress). */
			if (eciHandle) {
				DBG(DBG_MODNAME "Stopping synthesis.");
				eciStop(eciHandle);
			}

			/* Stop any audio playback (if in progress). */
			if (module_audio_id) {
				pthread_mutex_lock(&sound_stop_mutex);
				DBG(DBG_MODNAME "Stopping audio.");
				int ret = spd_audio_stop(module_audio_id);
				if (0 != ret)
					DBG(DBG_MODNAME "WARNING: Non 0 value from spd_audio_stop: %d", ret);
				pthread_mutex_unlock(&sound_stop_mutex);
			}
		}

		DBG(DBG_MODNAME "Waiting for synthesis thread to suspend.");
		while (is_thread_busy(&synth_suspended_mutex))
			g_usleep(100);
		DBG(DBG_MODNAME "Waiting for playback thread to suspend.");
		while (is_thread_busy(&play_suspended_mutex))
			g_usleep(100);

		DBG(DBG_MODNAME "Clearing playback queue.");
		clear_playback_queue();

		DBG(DBG_MODNAME "Clearing index mark lookup table.");
		if (index_mark_ht) {
			g_hash_table_destroy(index_mark_ht);
			index_mark_ht = NULL;
		}

		if (stop_synth_requested)
			module_report_event_stop();
		else
			module_report_event_pause();

		stop_synth_requested = FALSE;
		stop_play_requested = FALSE;
		pause_requested = FALSE;

		DBG(DBG_MODNAME "Stop or pause completed.");
	}
	DBG(DBG_MODNAME "Stop or pause thread ended.......\n");

	pthread_exit(NULL);
}

static int process_text_mark(char *part, int part_len, char *mark_name)
{
	/* Handle index marks. */
	if (NULL != mark_name) {
		/* Assign the mark name an integer number and store in lookup table. */
		int *markId = (int *)g_malloc(sizeof(int));
		*markId = 1 + g_hash_table_size(index_mark_ht);
		g_hash_table_insert(index_mark_ht, markId, mark_name);
		if (!eciInsertIndex(eciHandle, *markId)) {
			DBG(DBG_MODNAME "Error sending index mark to synthesizer.");
			log_eci_error();
			/* Try to keep going. */
		} else
			DBG(DBG_MODNAME "Index mark |%s| (id %i) sent to synthesizer.", mark_name, *markId);
		/* If pause is requested, skip over rest of message,
		   but synthesize what we have so far. */
		if (pause_requested) {
			DBG(DBG_MODNAME "Pause requested in synthesis thread.");
			return 1;
		}
		return 0;
	}

	/* Handle normal text. */
	if (part_len > 0) {
		DBG(DBG_MODNAME "Returned %d bytes from get_part.", part_len);
		DBG(DBG_MODNAME "Text to synthesize is |%s|\n", part);
		DBG(DBG_MODNAME "Sending text to synthesizer.");
		if (!eciAddText(eciHandle, part)) {
			DBG(DBG_MODNAME "Error sending text.");
			log_eci_error();
			return 2;
		}
		return 0;
	}

	/* Handle end of text. */
	DBG(DBG_MODNAME "End of data in synthesis thread.");
	/*
	  Add index mark for end of message.
	  This also makes sure the callback gets called at least once
	*/
	eciInsertIndex(eciHandle, MSG_END_MARK);
	DBG(DBG_MODNAME "Trying to synthesize text.");
	if (!eciSynthesize(eciHandle)) {
		DBG(DBG_MODNAME "Error synthesizing.");
		log_eci_error();
		return 2;;
	}

	/* Audio and index marks are returned in eciCallback(). */
	DBG(DBG_MODNAME "Waiting for synthesis to complete.");
	if (!eciSynchronize(eciHandle)) {
		DBG(DBG_MODNAME "Error waiting for synthesis to complete.");
		log_eci_error();
		return 2;
	}
	DBG(DBG_MODNAME "Synthesis complete.");
	return 3;
}

/* Synthesis thread. */
static void *_synth(void *nothing)
{
	char *pos = NULL;
	char *part = NULL;
	int part_len = 0;
	int ret;

	DBG(DBG_MODNAME "Synthesis thread starting.......\n");

	/* Block all signals to this thread. */
	set_speaking_thread_parameters();

	/* Allocate a place for index mark names to be placed. */
	char *mark_name = NULL;

	while (!thread_exit_requested) {
		/* If semaphore not set, set suspended lock and suspend until it is signaled. */
		if (0 != sem_trywait(&synth_semaphore)) {
			pthread_mutex_lock(&synth_suspended_mutex);
			sem_wait(&synth_semaphore);
			pthread_mutex_unlock(&synth_suspended_mutex);
			if (thread_exit_requested)
				break;
		}
		DBG(DBG_MODNAME "Synthesis semaphore on.");

		/* This table assigns each index mark name an integer id for fast lookup when
		   ECI returns the integer index mark event. */
		if (index_mark_ht)
			g_hash_table_destroy(index_mark_ht);
		index_mark_ht =
			g_hash_table_new_full(g_int_hash, g_int_equal, g_free,
					      g_free);

		pos = *message;
		load_user_dictionary();

		switch (message_type) {
		case SPD_MSGTYPE_TEXT:
			eciSetParam(eciHandle, eciTextMode, eciTextModeDefault);
			break;
		case SPD_MSGTYPE_SOUND_ICON:
			/* IBM TTS does not support sound icons.
			   If we can find a sound icon file, play that,
			   otherwise speak the name of the sound icon. */
			part = search_for_sound_icon(*message);
			if (NULL != part) {
				add_flag_to_playback_queue
					(QET_BEGIN);
				add_sound_icon_to_playback_queue(part);
				part = NULL;
				add_flag_to_playback_queue
					(QET_END);
				/* Wake up the audio playback thread, if not already awake. */
				if (!is_thread_busy
				    (&play_suspended_mutex))
					sem_post(&play_semaphore);
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
			/* TODO: make sure all SSIP cases are supported */
			/* Map unspeakable keys to speakable words. */
			DBG(DBG_MODNAME "Key from Speech Dispatcher: |%s|", pos);
			pos = subst_keys(pos);
			DBG(DBG_MODNAME "Key to speak: |%s|", pos);
			g_free(*message);
			*message = pos;
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

		add_flag_to_playback_queue(QET_BEGIN);
		while (TRUE) {
			if (stop_synth_requested) {
				DBG(DBG_MODNAME "Stop in synthesis thread, terminating.");
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

			part = next_part(pos, &mark_name);
			if (NULL == part) {
				DBG(DBG_MODNAME "Error getting next part of message.");
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

	DBG(DBG_MODNAME "Synthesis thread ended.......\n");

	pthread_exit(NULL);
}

static void set_rate(signed int rate)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	/* Setting rate to midpoint is too fast.  An eci value of 50 is "normal".
	   See chart on pg 38 of the ECI manual. */
	assert(rate >= -100 && rate <= +100);
	int speed;
	/* Possible ECI range is 0 to 250. */
	/* Map rate -100 to 100 onto speed 0 to 140. */
	if (rate < 0)
		/* Map -100 to 0 onto 0 to voice_speed */
		speed = ((float)(rate + 100) * voice_speed) / (float)100;
	else
		/* Map 0 to 100 onto voice_speed to 140 */
		speed =
			(((float)rate * (140 - voice_speed)) / (float)100)
			+ voice_speed;
	assert(speed >= 0 && speed <= 140);
	int ret = eciSetVoiceParam(eciHandle, 0, eciSpeed, speed);
	if (-1 == ret) {
		DBG(DBG_MODNAME "Error setting rate %i.", speed);
		log_eci_error();
	} else
		DBG(DBG_MODNAME "Rate set to %i.", speed);
}

static void set_volume(signed int volume)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
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
		DBG(DBG_MODNAME "Error setting volume %i.", vol);
		log_eci_error();
	} else
		DBG(DBG_MODNAME "Volume set to %i.", vol);
}

static void set_pitch(signed int pitch)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	/* Setting pitch to midpoint is to low.  eci values between 65 and 89
	   are "normal".
	   See chart on pg 38 of the ECI manual. */
	assert(pitch >= -100 && pitch <= +100);
	int pitchBaseline;
	/* Possible range 0 to 100. */
	if (pitch < 0)
		/* Map -100 to 0 onto 0 to voice_pitch_baseline */
		pitchBaseline =
			((float)(pitch + 100) * voice_pitch_baseline) /
			(float)100;
	else
		/* Map 0 to 100 onto voice_pitch_baseline to 100 */
		pitchBaseline =
			(((float)pitch * (100 - voice_pitch_baseline)) /
			 (float)100)
			+ voice_pitch_baseline;
	assert(pitchBaseline >= 0 && pitchBaseline <= 100);
	int ret =
		eciSetVoiceParam(eciHandle, 0, eciPitchBaseline, pitchBaseline);
	if (-1 == ret) {
		DBG(DBG_MODNAME "Error setting pitch %i.", pitchBaseline);
		log_eci_error();
	} else
		DBG(DBG_MODNAME "Pitch set to %i.", pitchBaseline);
}

static void set_punctuation_mode(SPDPunctuation punct_mode)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	const char *fmt = " `Pf%d%s ";
	char *msg = NULL;
	int real_punct_mode = 0;

	switch (punct_mode) {
	case SPD_PUNCT_NONE:
		real_punct_mode = 0;
		break;
	case SPD_PUNCT_SOME:
	case SPD_PUNCT_MOST:
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

static char *voice_enum_to_str(SPDVoiceType voice_type)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	/* TODO: Would be better to move this to module_utils.c. */
	char *voicename;
	switch (voice_type) {
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

/** Set voice parameters (if any are defined for this voice) */
static void set_voice_parameters(SPDVoiceType voice_type)
{
	char *voicename = voice_enum_to_str(voice_type);
	int eciVoice;
	int ret = -1;

	TIbmttsVoiceParameters *params = g_hash_table_lookup(IbmttsVoiceParameters, voicename);
	if (NULL == params) {
		DBG(DBG_MODNAME "Setting default VoiceParameters for voice %s", voicename);

		switch (voice_type) {
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
		case SPD_CHILD_FEMALE:
			eciVoice = 3;
			break;	/* Child */
		default:
			eciVoice = 1;
			break;	/* Adult Male 1 */
		}
		ret = eciCopyVoice(eciHandle, eciVoice, 0);
		if (-1 == ret)
			DBG(DBG_MODNAME "ERROR: Setting default voice parameters (voice %i).", eciVoice);
	} else {
		DBG(DBG_MODNAME "Setting custom VoiceParameters for voice %s", voicename);

		ret = eciSetVoiceParam(eciHandle, 0, eciGender, params->gender);
		if (-1 == ret)
			DBG(DBG_MODNAME "ERROR: Setting gender %i", params->gender);

		ret = eciSetVoiceParam(eciHandle, 0, eciBreathiness, params->breathiness);
		if (-1 == ret)
			DBG(DBG_MODNAME "ERROR: Setting breathiness %i", params->breathiness);

		ret = eciSetVoiceParam(eciHandle, 0, eciHeadSize, params->head_size);
		if (-1 == ret)
			DBG(DBG_MODNAME "ERROR: Setting head size %i", params->head_size);

		ret = eciSetVoiceParam(eciHandle, 0, eciPitchBaseline, params->pitch_baseline);
		if (-1 == ret)
			DBG(DBG_MODNAME "ERROR: Setting pitch baseline %i", params->pitch_baseline);

		ret = eciSetVoiceParam(eciHandle, 0, eciPitchFluctuation, params->pitch_fluctuation);
		if (-1 == ret)
			DBG(DBG_MODNAME "ERROR: Setting pitch fluctuation %i", params->pitch_fluctuation);

		ret = eciSetVoiceParam(eciHandle, 0, eciRoughness, params->roughness);
		if (-1 == ret)
			DBG(DBG_MODNAME "ERROR: Setting roughness %i", params->roughness);

		ret = eciSetVoiceParam(eciHandle, 0, eciSpeed, params->speed);
		if (-1 == ret)
			DBG(DBG_MODNAME "ERROR: Setting speed %i", params->speed);
	}

	g_free(voicename);
}

/* 
   Convert the supplied arguments to the eciLanguageDialect value and
   sets the eciLanguageDialect parameter.

   The arguments are used in this order:
   - find a matching voice name, 
   - otherwise find the first matching language

   EXAMPLES
   1. Using Orca 3.30.1:
   - lang="en", voice=1, name="zuzana"
   name ("zuzana") matches the installed voice Zuzana embedded-compact

   - lang="en", voice=1, name="voxin default voice"
   name does not match any installed voice.
   The first English voice present is returned.


   2. Using spd-say (LC_ALL=C)
   - lang="c", voice=1, name="nathan-embedded-compact"
   name matches the installed voice Nathan embedded-compact

   spd-say command: 
   spd-say -o voxin -y nathan-embedded-compact hello

   - lang="en-us", voice=1, name=
   The first American English voice present is returned.

   spd-say command:
   spd-say -o voxin -l en-US hello

*/
static void set_language_and_voice(char *lang, SPDVoiceType voice_type, char *name)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	int ret = -1;
	int i = 0, index = -1;
	
	DBG(DBG_MODNAME "%s, lang=%s, voice=%d, name=%s",
	    __FUNCTION__, lang, (int)voice_type, name ? name : "");

	assert(speechd_voice);

	if (name && *name) {
		for (i = 0; speechd_voice[i]; i++) {
			DBG("%d. name=%s", i, speechd_voice[i]->name);
			if (!strcasecmp(speechd_voice[i]->name, name)) {
				index = i;
				break;
			}
		}
	}

	if ((index == -1) && lang) {
		/* lang may include an optional sub tag after its two */
		/* letters code (e.g. "pt-br") */
		const char *optional_sub_tag = (lang[2] == '-') ? lang + 3 : NULL;

		for (i = 0; speechd_voice[i]; i++) {
			DBG("Checking speechd_voice[%d] (language=%s, name=%s)",
			    i,
			    speechd_voice[i]->language ? speechd_voice[i]->language : "",
			    speechd_voice[i]->name ? speechd_voice[i]->name : "");
			if (!strncmp(lang, speechd_voice[i]->language, 2)) {
				if (optional_sub_tag
				    && (speechd_voice[i]->language[2] == '-')
				    && !strcasecmp(optional_sub_tag, speechd_voice[i]->language+3)) {
					DBG("match!");
					index = i;
					break;
				} else if (index == -1) {
					DBG("match!");
					index = i;					
				}
			}
		}
	}

	if (index == -1) { // no matching voice: choose the first available voice 
		if (!speechd_voice[0])
			return;
		index = 0;
	}

	ret = eciSetParam(eciHandle, eciLanguageDialect, voices[index].id);
	if (ret == -1) {
		log_eci_error();
		return;
	}

	DBG(DBG_MODNAME "select speechd_voice[%d]: id=0x%x, name=%s (ret=%d)",
	    index, voices[index].id, voices[index].name, ret);

	input_encoding = voices[index].charset;
	update_sample_rate();		  	
	g_atomic_int_set(&locale_index_atomic, index);

	set_voice_parameters(voice_type);

	/* Retrieve the baseline pitch and speed of the voice. */
	voice_pitch_baseline = eciGetVoiceParam(eciHandle, 0, eciPitchBaseline);
	if (-1 == voice_pitch_baseline)
		DBG(DBG_MODNAME "Cannot get pitch baseline of voice.");
	
	voice_speed = eciGetVoiceParam(eciHandle, 0, eciSpeed);
	if (-1 == voice_speed)
		DBG(DBG_MODNAME "Cannot get speed of voice.");
}

static void set_voice_type(SPDVoiceType voice_type)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	if (msg_settings.voice.language) {
		set_language_and_voice(msg_settings.voice.language, voice_type, msg_settings.voice.name);
	}
}

static void set_language(char *lang)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	set_language_and_voice(lang, msg_settings.voice_type, msg_settings.voice.name);
}

/* sets the voice according to its name. 

   If the voice name is not found, try to select the first available
   voice for the current language (e.g. synthesis_voice == "voxin
   default voice" will select the first voice of the current language)
*/
static void set_synthesis_voice(char *synthesis_voice)
{
	int i = 0;

	if (synthesis_voice == NULL) {
		return;
	}

	DBG(DBG_MODNAME "ENTER %s(%s)", __FUNCTION__, synthesis_voice);

	for (i = 0; i < number_of_voices; i++) {
		if (!strcasecmp(speechd_voice[i]->name, synthesis_voice)) {
			set_language_and_voice(speechd_voice[i]->language, msg_settings.voice_type, speechd_voice[i]->name);
			break;
		}
	}
	
	if (i == number_of_voices) {
		set_language_and_voice(msg_settings.voice.language, msg_settings.voice_type, NULL);
	}

}

static void log_eci_error()
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	/* TODO: This routine is not working.  Not sure why. */
	char buf[100];
	eciErrorMessage(eciHandle, buf);
	DBG(DBG_MODNAME "ECI Error Message: %s", buf);
}

/* The text-to-speech calls back here when a chunk of audio is ready
   or an index mark has been reached.  The good news is that it
   returns the audio up to each index mark or when the audio buffer is
   full. */
static enum ECICallbackReturn eciCallback(ECIHand hEngine,
					  enum ECIMessage msg,
					  long lparam, void *data)
{
	/* This callback is running in the same thread as called eciSynchronize(),
	   i.e., the _synth() thread. */

	/* If module_stop was called, discard any further callbacks until module_speak is called. */
	if (stop_synth_requested || stop_play_requested)
		return eciDataProcessed;

	switch (msg) {
	case eciWaveformBuffer:
		DBG(DBG_MODNAME "%ld audio samples returned from TTS.", lparam);
		/* Add audio to output queue. */
		add_audio_to_playback_queue(audio_chunk, lparam);
		/* Wake up the audio playback thread, if not already awake. */
		if (!is_thread_busy(&play_suspended_mutex))
			sem_post(&play_semaphore);
		return eciDataProcessed;
		break;
	case eciIndexReply:
		DBG(DBG_MODNAME "Index mark id %ld returned from TTS.", lparam);
		if (lparam == MSG_END_MARK) {
			add_flag_to_playback_queue(QET_END);
		} else {
			/* Add index mark to output queue. */
			add_mark_to_playback_queue(lparam);
		}
		/* Wake up the audio playback thread, if not already awake. */
		if (!is_thread_busy(&play_suspended_mutex))
			sem_post(&play_semaphore);
		return eciDataProcessed;
		break;
	default:
		return eciDataProcessed;
	}
}

/* Adds a chunk of pcm audio to the audio playback queue. */
static gboolean add_audio_to_playback_queue(TEciAudioSamples * audio_chunk, long num_samples)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	TPlaybackQueueEntry *playback_queue_entry =
		(TPlaybackQueueEntry *) g_malloc(sizeof(TPlaybackQueueEntry));
	if (NULL == playback_queue_entry)
		return FALSE;
	playback_queue_entry->type = QET_AUDIO;
	playback_queue_entry->data.audio.num_samples = (int)num_samples;
	int wlen = sizeof(TEciAudioSamples) * num_samples;
	playback_queue_entry->data.audio.audio_chunk =
		(TEciAudioSamples *) g_malloc(wlen);
	memcpy(playback_queue_entry->data.audio.audio_chunk, audio_chunk, wlen);
	pthread_mutex_lock(&playback_queue_mutex);
	playback_queue = g_slist_append(playback_queue, playback_queue_entry);
	pthread_mutex_unlock(&playback_queue_mutex);
	return TRUE;
}

/* Adds an Index Mark to the audio playback queue. */
static gboolean add_mark_to_playback_queue(long markId)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	TPlaybackQueueEntry *playback_queue_entry =
		(TPlaybackQueueEntry *) g_malloc(sizeof(TPlaybackQueueEntry));
	if (NULL == playback_queue_entry)
		return FALSE;
	playback_queue_entry->type = QET_INDEX_MARK;
	playback_queue_entry->data.markId = markId;
	pthread_mutex_lock(&playback_queue_mutex);
	playback_queue = g_slist_append(playback_queue, playback_queue_entry);
	pthread_mutex_unlock(&playback_queue_mutex);
	return TRUE;
}

/* Adds a begin or end flag to the playback queue. */
static gboolean add_flag_to_playback_queue(EPlaybackQueueEntryType type)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	TPlaybackQueueEntry *playback_queue_entry =
		(TPlaybackQueueEntry *) g_malloc(sizeof(TPlaybackQueueEntry));
	if (NULL == playback_queue_entry)
		return FALSE;
	playback_queue_entry->type = type;
	pthread_mutex_lock(&playback_queue_mutex);
	playback_queue = g_slist_append(playback_queue, playback_queue_entry);
	pthread_mutex_unlock(&playback_queue_mutex);
	return TRUE;
}

/* Add a sound icon to the playback queue. */
static gboolean add_sound_icon_to_playback_queue(char *filename)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	TPlaybackQueueEntry *playback_queue_entry =
		(TPlaybackQueueEntry *) g_malloc(sizeof(TPlaybackQueueEntry));
	if (NULL == playback_queue_entry)
		return FALSE;
	playback_queue_entry->type = QET_SOUND_ICON;
	playback_queue_entry->data.sound_icon_filename = filename;
	pthread_mutex_lock(&playback_queue_mutex);
	playback_queue = g_slist_append(playback_queue, playback_queue_entry);
	pthread_mutex_unlock(&playback_queue_mutex);
	return TRUE;
}

/* Deletes an entry from the playback audio queue, freeing memory. */
static void delete_playback_queue_entry(TPlaybackQueueEntry * playback_queue_entry)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	switch (playback_queue_entry->type) {
	case QET_AUDIO:
		g_free(playback_queue_entry->data.audio.audio_chunk);
		break;
	case QET_SOUND_ICON:
		g_free(playback_queue_entry->data.sound_icon_filename);
		break;
	default:
		break;
	}
	g_free(playback_queue_entry);
}

/* Erases the entire playback queue, freeing memory. */
static void clear_playback_queue()
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	pthread_mutex_lock(&playback_queue_mutex);
	while (NULL != playback_queue) {
		TPlaybackQueueEntry *playback_queue_entry =
			playback_queue->data;
		delete_playback_queue_entry(playback_queue_entry);
		playback_queue =
			g_slist_remove(playback_queue, playback_queue->data);
	}
	playback_queue = NULL;
	pthread_mutex_unlock(&playback_queue_mutex);
}

/* Sends a chunk of audio to the audio player and waits for completion or error. */
static gboolean send_to_audio(TPlaybackQueueEntry * playback_queue_entry)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
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
		return TRUE;

	DBG(DBG_MODNAME "Sending %i samples to audio.", track.num_samples);
	ret = module_tts_output(track, format);
	if (ret < 0) {
		DBG("ERROR: Can't play track for unknown reason.");
		return FALSE;
	}
	DBG(DBG_MODNAME "Sent to audio.");
	return TRUE;
}

/* Playback thread. */
static void *_play(void *nothing)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	int markId;
	char *mark_name;
	TPlaybackQueueEntry *playback_queue_entry = NULL;

	DBG(DBG_MODNAME "Playback thread starting.......\n");

	/* Block all signals to this thread. */
	set_speaking_thread_parameters();

	while (!thread_exit_requested) {
		/* If semaphore not set, set suspended lock and suspend until it is signaled. */
		if (0 != sem_trywait(&play_semaphore)) {
			pthread_mutex_lock(&play_suspended_mutex);
			sem_wait(&play_semaphore);
			pthread_mutex_unlock(&play_suspended_mutex);
		}
		/* DBG(DBG_MODNAME "Playback semaphore on."); */

		while (!stop_play_requested
		       && !thread_exit_requested) {
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
			case QET_AUDIO:
				send_to_audio(playback_queue_entry);
				break;
			case QET_INDEX_MARK:
				/* Look up the index mark integer id in lookup table to
				   find string name and emit that name. */
				markId = playback_queue_entry->data.markId;
				mark_name =
					g_hash_table_lookup(index_mark_ht,
							    &markId);
				if (NULL == mark_name) {
					DBG(DBG_MODNAME "markId %d returned by TTS not found in lookup table.", markId);
				} else {
					DBG(DBG_MODNAME "reporting index mark |%s|.", mark_name);
					module_report_index_mark(mark_name);
					DBG(DBG_MODNAME "index mark reported.");
					/* If pause requested, wait for an end-of-sentence index mark. */
					if (pause_requested) {
						if (0 ==
						    strncmp(mark_name,
							    SD_MARK_BODY,
							    SD_MARK_BODY_LEN)) {
							DBG(DBG_MODNAME "Pause requested in playback thread.  Stopping.");
							stop_play_requested
								= TRUE;
						}
					}
				}
				break;
			case QET_SOUND_ICON:
				module_play_file(playback_queue_entry->
						 data.sound_icon_filename);
				break;
			case QET_BEGIN:
				module_report_event_begin();
				break;
			case QET_END:
				module_report_event_end();
				break;
			}

			delete_playback_queue_entry
				(playback_queue_entry);
			playback_queue_entry = NULL;
		}
		if (stop_play_requested)
			DBG(DBG_MODNAME "Stop or pause in playback thread.");
	}

	DBG(DBG_MODNAME "Playback thread ended.......\n");

	pthread_exit(NULL);
}

/* Replaces all occurrences of "from" with "to" in msg.
   Returns count of replacements. */
static int replace(char *from, char *to, GString * msg)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
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

static void subst_keys_cb(gpointer data, gpointer user_data)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	TIbmttsKeySubstitution *key_subst = data;
	GString *msg = user_data;
	replace(key_subst->key, key_subst->newkey, msg);
}

/* Given a Speech Dispatcher !KEY key sequence, replaces unspeakable
   or incorrectly spoken keys or characters with speakable ones.
   The subsitutions come from the KEY NAME SUBSTITUTIONS section of the
   config file.
   Caller is responsible for freeing returned string. */
static char *subst_keys(char *key)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	GString *tmp = g_string_sized_new(30);
	g_string_append(tmp, key);

	GList *keyTable = g_hash_table_lookup(IbmttsKeySubstitution,
					      msg_settings.voice.language);

	if (keyTable)
		g_list_foreach(keyTable, subst_keys_cb, tmp);

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
static char *search_for_sound_icon(const char *icon_name)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
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

static void free_voice_list()
{	
	DBG(DBG_MODNAME "ENTER %s", __func__);
	int i = 0;


	if (!speechd_voice)
		return;

	for (i = 0; speechd_voice[i]; i++) {
		if (speechd_voice[i]->name) {
			g_free(speechd_voice[i]->name);
			speechd_voice[i]->name = NULL;
		}
		if (speechd_voice[i]->language) {
			g_free(speechd_voice[i]->language);
			speechd_voice[i]->language = NULL;
		}
		if (speechd_voice[i]->variant) {
			g_free(speechd_voice[i]->variant);
			speechd_voice[i]->variant = NULL;
		}
		g_free(speechd_voice[i]);
		speechd_voice[i] = NULL;
	}

	g_free(speechd_voice);
	speechd_voice = NULL;
}

static gboolean vox_to_spd_voice(vox_t *from, SPDVoice *to)
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	if (!from
	    || !to
	    || to->name || to->language || to->variant
	    || from->name[sizeof(from->name)-1]
	    || from->lang[sizeof(from->lang)-1]
	    || from->variant[sizeof(from->variant)-1]
	    ) {
		DBG(DBG_MODNAME "args error");		
		return FALSE;
	}	

	{ /* set name */
		int i;
		to->name = *from->quality ?
			g_strdup_printf("%s-%s", from->name, from->quality) :
			g_strdup(from->name);
		for (i=0; to->name[i]; i++) {
			to->name[i] = tolower(to->name[i]);
		}
	}
	{ /* set language: language identifier (lower case) + variant/dialect (all caps) */
		if (*from->variant) {
			size_t len = strlen(from->lang);
			int i;
			to->language = g_strdup_printf("%s-%s", from->lang, from->variant);
			for (i=len; to->language[i]; i++) {
				to->language[i] = toupper(to->language[i]);
			}
		} else {
			to->language = g_strdup(from->lang);
		}
	}
	to->variant = g_strdup("none");
	
	{ /* log the 'from' argument */
		size_t size = 0;
		if (!voxToString(from, NULL, &size)) {
			gchar *str = g_malloc0(size);
			if (!voxToString(from, str, &size)) {
				DBG(DBG_MODNAME "from: %s", str);
			}
			g_free(str);
		}
	}
	DBG(DBG_MODNAME "to: name=%s, variant=%s, language=%s", to->name, to->variant, to->language);
	return TRUE;
}

static gboolean alloc_voice_list()
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	int i = 0;	

	/* obtain the list of installed voices */
	number_of_voices = 0;
	if (voxGetVoices(NULL, &number_of_voices) || !number_of_voices) {
		return FALSE;
	}

	voices = g_new0(vox_t, number_of_voices);
	if (voxGetVoices(voices, &number_of_voices) || !number_of_voices)
		goto exit0;

	DBG(DBG_MODNAME "number_of_voices=%u", number_of_voices);

	/* build speechd_voice */
	speechd_voice = g_new0(SPDVoice*, number_of_voices + 1);	
	for (i = 0; i < number_of_voices; i++) {
		speechd_voice[i] = g_malloc0(sizeof(SPDVoice));
		if (!vox_to_spd_voice(voices+i, speechd_voice[i]))
			goto exit0;			
	}
	speechd_voice[number_of_voices] = NULL;

	for (i = 0; speechd_voice[i]; i++) {
		DBG(DBG_MODNAME "speechd_voice[%d]:name=%s, language=%s, variant=%s",
		    i,
		    speechd_voice[i]->name ? speechd_voice[i]->name : "null",
		    speechd_voice[i]->language ? speechd_voice[i]->language : "null",
		    speechd_voice[i]->variant ? speechd_voice[i]->variant : "null");
	}
	
	DBG(DBG_MODNAME "LEAVE %s", __func__);
	return TRUE;

 exit0:
	if (voices) {
		g_free(voices);
		voices = NULL;
	}
	free_voice_list();
	return FALSE;
}

static void load_user_dictionary()
{
	DBG(DBG_MODNAME "ENTER %s", __func__);
	GString *dirname = NULL;
	GString *filename = NULL;
	int i = 0;
	int dictionary_is_present = 0;
	static guint old_index = G_MAXUINT;
	guint new_index;
	const char *language = NULL;
	const char *region = NULL;
	ECIDictHand eciDict = eciGetDict(eciHandle);

	new_index = g_atomic_int_get(&locale_index_atomic);
	if (new_index >= number_of_voices) {
		DBG(DBG_MODNAME "%s, unexpected index (0x%x)", __FUNCTION__, new_index);
		return;
	}

	if (old_index == new_index) {
		DBG(DBG_MODNAME "LEAVE %s, no change", __FUNCTION__);
		return;
	}

	language = voices[new_index].lang;
	region = voices[new_index].variant;

	/* Fix locale name for French Canadian */
	if (!strcmp(language, "ca") && !strcmp(region, "FR")) {
		language = "fr";
		region = "CA";
	}

	if (eciDict) {
		DBG(DBG_MODNAME "delete old dictionary");
		eciDeleteDict(eciHandle, eciDict);
	}
	eciDict = eciNewDict(eciHandle);
	if (eciDict) {
		old_index = new_index;
	} else {
		old_index = number_of_voices;
		DBG(DBG_MODNAME "can't create new dictionary");
		return;
	}
	
	/* Look for the dictionary directory */
	dirname = g_string_new(NULL);
	g_string_printf(dirname, "%s/%s_%s", IbmttsDictionaryFolder, language,
			region);
	if (!g_file_test(dirname->str, G_FILE_TEST_IS_DIR)) {
		DBG(DBG_MODNAME "%s is not a directory",
		    dirname->str);		
		g_string_printf(dirname, "%s/%s", IbmttsDictionaryFolder,
				language);
		if (!g_file_test(dirname->str, G_FILE_TEST_IS_DIR)) {
			g_string_printf(dirname, "%s", IbmttsDictionaryFolder);
			if (!g_file_test(dirname->str, G_FILE_TEST_IS_DIR)) {
				DBG(DBG_MODNAME "%s is not a directory",
				    dirname->str);
				return;
			}
		}
	}

	DBG(DBG_MODNAME "Looking in dictionary directory %s", dirname->str);
	filename = g_string_new(NULL);

	for (i = 0; i < NB_OF_DICTIONARY_FILENAMES; i++) {
		g_string_printf(filename, "%s/%s", dirname->str,
				dictionary_filenames[i]);
		if (g_file_test(filename->str, G_FILE_TEST_EXISTS)) {
			enum ECIDictError error =
				eciLoadDict(eciHandle, eciDict, i, filename->str);
			if (!error) {
				dictionary_is_present = 1;
				DBG(DBG_MODNAME "%s dictionary loaded",
				    filename->str);
			} else {
				DBG(DBG_MODNAME "Can't load %s dictionary (%d)",
				    filename->str, error);
			}
		} else {
			DBG(DBG_MODNAME "No %s dictionary", filename->str);
		}
	}

	g_string_free(filename, TRUE);
	g_string_free(dirname, TRUE);

	if (dictionary_is_present) {
		eciSetDict(eciHandle, eciDict);
	}
}
/* local variables: */
/* c-basic-offset: 8 */
/* end: */
