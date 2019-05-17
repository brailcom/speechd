/*
 * module_utils_speak_queue.c - Speak queue helper for Speech Dispatcher modules
 *
 * Copyright (C) 2007 Brailcom, o.p.s.
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Based on espeak.c
 *
 * @author Lukas Loehrer
 * Based on ibmtts.c.
 */

#include "module_utils_speak_queue.h"

#define DBG_MODNAME "speak_queue"

#include "module_utils.h"

typedef enum {
	IDLE,
	BEFORE_SYNTH,
	BEFORE_PLAY,
	SPEAKING
} TEspeakState;

typedef enum {
	ESPEAK_PAUSE_OFF,
	ESPEAK_PAUSE_REQUESTED,
	ESPEAK_PAUSE_MARK_REPORTED
} TEspeakPauseState;

/* Thread and process control. */

static TEspeakState espeak_state = IDLE;
/* Note: espeak_state_mutex is always taken before espeak_stop_or_pause_mutex or
 * espeak_play_mutex */
static pthread_mutex_t espeak_state_mutex;

static pthread_t espeak_play_thread;
static pthread_t espeak_stop_or_pause_thread;

static pthread_cond_t espeak_stop_or_pause_cond;
static pthread_cond_t espeak_stop_or_pause_sleeping_cond;
static int espeak_stop_or_pause_sleeping;
static pthread_mutex_t espeak_stop_or_pause_mutex;
static pthread_cond_t espeak_play_cond;
static pthread_cond_t espeak_play_sleeping_cond;
static int espeak_play_sleeping;
static pthread_mutex_t espeak_play_mutex;

static gboolean espeak_close_requested = FALSE;
static TEspeakPauseState espeak_pause_state = ESPEAK_PAUSE_OFF;
static gboolean espeak_stop_requested = FALSE;

static void module_speak_queue_reset(void);

/* The playback queue. */

static int speak_queue_maxsize;
static int speak_queue_sample_rate;

typedef enum {
	ESPEAK_QET_AUDIO,	/* Chunk of audio. */
	ESPEAK_QET_INDEX_MARK,	/* Index mark event. */
	ESPEAK_QET_SOUND_ICON,	/* A Sound Icon */
	ESPEAK_QET_BEGIN,	/* Beginning of speech. */
	ESPEAK_QET_END		/* Speech completed. */
} EPlaybackQueueEntryType;

typedef struct {
	long num_samples;
	short *audio_chunk;
} TPlaybackQueueAudioChunk;

typedef struct {
	EPlaybackQueueEntryType type;
	union {
		char *markId;
		TPlaybackQueueAudioChunk audio;
		char *sound_icon_filename;
	} data;
} TPlaybackQueueEntry;

static GSList *playback_queue = NULL;
static int playback_queue_size = 0;	/* Number of audio frames currently in queue */
static pthread_mutex_t playback_queue_mutex;
pthread_cond_t playback_queue_condition;

/* Internal function prototypes for playback thread. */
static gboolean espeak_add_flag_to_playback_queue(EPlaybackQueueEntryType type);
static void espeak_delete_playback_queue_entry(TPlaybackQueueEntry *
					       playback_queue_entry);
static gboolean espeak_send_to_audio(TPlaybackQueueEntry *
				     playback_queue_entry);

/* Miscellaneous internal function prototypes. */
static void espeak_clear_playback_queue();

/* The playback thread start routine. */
static void *_espeak_play(void *);
/* The stop_or_pause start routine. */
static void *_espeak_stop_or_pause(void *);

int module_speak_queue_init(int sample_rate, int maxsize, char **status_info)
{
	int ret;

	speak_queue_maxsize = maxsize;
	speak_queue_sample_rate = sample_rate;

	/* Reset global state */
	module_speak_queue_reset();

	/* This mutex mediates access to the playback queue between the espeak synthesis thread andthe the playback thread. */
	pthread_mutex_init(&playback_queue_mutex, NULL);
	pthread_cond_init(&playback_queue_condition, NULL);

	/* The following mutex protects access to various flags */
	pthread_mutex_init(&espeak_state_mutex, NULL);

	DBG(DBG_MODNAME " Creating new thread for stop or pause.");
	pthread_cond_init(&espeak_stop_or_pause_cond, NULL);
	pthread_cond_init(&espeak_stop_or_pause_sleeping_cond, NULL);
	pthread_mutex_init(&espeak_stop_or_pause_mutex, NULL);
	espeak_stop_or_pause_sleeping = 0;

	ret =
	    pthread_create(&espeak_stop_or_pause_thread, NULL,
			   _espeak_stop_or_pause, NULL);
	if (0 != ret) {
		DBG("Failed to create stop-or-pause thread.");
		*status_info =
		    g_strdup("Failed to create stop-or-pause thread.");
		return -1;
	}

	pthread_cond_init(&espeak_play_cond, NULL);
	pthread_cond_init(&espeak_play_sleeping_cond, NULL);
	pthread_mutex_init(&espeak_play_mutex, NULL);
	espeak_play_sleeping = 0;

	DBG(DBG_MODNAME " Creating new thread for playback.");
	ret = pthread_create(&espeak_play_thread, NULL, _espeak_play, NULL);
	if (ret != 0) {
		DBG("Failed to create playback thread.");
		*status_info = g_strdup("Failed to create playback thread.");
		return -1;
	}

	return 0;
}

void module_speak_queue_reset(void)
{
	espeak_state = IDLE;
	espeak_pause_state = ESPEAK_PAUSE_OFF;
	espeak_stop_requested = FALSE;
}

int module_speak_queue_before_synth(void)
{
	pthread_mutex_lock(&espeak_state_mutex);
	if (espeak_state != IDLE) {
		DBG(DBG_MODNAME " Warning, module_speak called when not ready.");
		pthread_mutex_unlock(&espeak_state_mutex);
		return FALSE;
	}

	module_speak_queue_reset();
	espeak_state = BEFORE_SYNTH;
	pthread_mutex_unlock(&espeak_state_mutex);
	return TRUE;
}

int module_speak_queue_before_play(void)
{
	int ret = 0;
	pthread_mutex_lock(&espeak_state_mutex);
	if (espeak_state == BEFORE_SYNTH) {
		ret = 1;
		espeak_state = BEFORE_PLAY;
		espeak_add_flag_to_playback_queue(ESPEAK_QET_BEGIN);
		/* Wake up playback thread */
		pthread_cond_signal(&espeak_play_cond);
	}
	pthread_mutex_unlock(&espeak_state_mutex);
	return ret;
}

gboolean module_speak_queue_add_end(void)
{
	return espeak_add_flag_to_playback_queue(ESPEAK_QET_END);
}

static TPlaybackQueueEntry *playback_queue_pop()
{
	TPlaybackQueueEntry *result = NULL;
	pthread_mutex_lock(&playback_queue_mutex);
	while (!espeak_stop_requested && playback_queue == NULL) {
		pthread_cond_wait(&playback_queue_condition,
				  &playback_queue_mutex);
	}
	if (!espeak_stop_requested) {
		result = (TPlaybackQueueEntry *) playback_queue->data;
		playback_queue =
		    g_slist_remove(playback_queue, playback_queue->data);
		if (result->type == ESPEAK_QET_AUDIO) {
			playback_queue_size -= result->data.audio.num_samples;
			pthread_cond_signal(&playback_queue_condition);
		}
	}
	pthread_mutex_unlock(&playback_queue_mutex);
	return result;
}

static gboolean playback_queue_push(TPlaybackQueueEntry * entry)
{
	pthread_mutex_lock(&playback_queue_mutex);
	playback_queue = g_slist_append(playback_queue, entry);
	if (entry->type == ESPEAK_QET_AUDIO) {
		playback_queue_size += entry->data.audio.num_samples;
	}
	pthread_cond_signal(&playback_queue_condition);
	pthread_mutex_unlock(&playback_queue_mutex);
	return TRUE;
}

/* Adds a chunk of pcm audio to the audio playback queue.
   Waits until there is enough space in the queue. */
gboolean
module_speak_queue_add_audio(short *audio_chunk, int num_samples)
{
	pthread_mutex_lock(&playback_queue_mutex);
	while (!espeak_stop_requested
	       && playback_queue_size > speak_queue_maxsize) {
		pthread_cond_wait(&playback_queue_condition,
				  &playback_queue_mutex);
	}
	pthread_mutex_unlock(&playback_queue_mutex);
	if (espeak_stop_requested) {
		return FALSE;
	}

	TPlaybackQueueEntry *playback_queue_entry =
	    g_new(TPlaybackQueueEntry, 1);

	playback_queue_entry->type = ESPEAK_QET_AUDIO;
	playback_queue_entry->data.audio.num_samples = num_samples;
	gint nbytes = sizeof(short) * num_samples;
	playback_queue_entry->data.audio.audio_chunk =
	    (short *)g_memdup((gconstpointer) audio_chunk, nbytes);

	playback_queue_push(playback_queue_entry);
	return TRUE;
}

/* Adds an Index Mark to the audio playback queue. */
gboolean module_speak_queue_add_mark(const char *markId)
{
	TPlaybackQueueEntry *playback_queue_entry =
	    (TPlaybackQueueEntry *) g_malloc(sizeof(TPlaybackQueueEntry));

	playback_queue_entry->type = ESPEAK_QET_INDEX_MARK;
	playback_queue_entry->data.markId = g_strdup(markId);
	return playback_queue_push(playback_queue_entry);
}

/* Adds a begin or end flag to the playback queue. */
static gboolean espeak_add_flag_to_playback_queue(EPlaybackQueueEntryType type)
{
	TPlaybackQueueEntry *playback_queue_entry =
	    (TPlaybackQueueEntry *) g_malloc(sizeof(TPlaybackQueueEntry));

	playback_queue_entry->type = type;
	return playback_queue_push(playback_queue_entry);
}

/* Add a sound icon to the playback queue. */
gboolean module_speak_queue_add_sound_icon(const char *filename)
{
	TPlaybackQueueEntry *playback_queue_entry =
	    (TPlaybackQueueEntry *) g_malloc(sizeof(TPlaybackQueueEntry));

	playback_queue_entry->type = ESPEAK_QET_SOUND_ICON;
	playback_queue_entry->data.sound_icon_filename = g_strdup(filename);
	return playback_queue_push(playback_queue_entry);
}

/* Deletes an entry from the playback audio queue, freeing memory. */
static void
espeak_delete_playback_queue_entry(TPlaybackQueueEntry * playback_queue_entry)
{
	switch (playback_queue_entry->type) {
	case ESPEAK_QET_AUDIO:
		g_free(playback_queue_entry->data.audio.audio_chunk);
		break;
	case ESPEAK_QET_INDEX_MARK:
		g_free(playback_queue_entry->data.markId);
		break;
	case ESPEAK_QET_SOUND_ICON:
		g_free(playback_queue_entry->data.sound_icon_filename);
		break;
	default:
		break;
	}
	g_free(playback_queue_entry);
}

/* Erases the entire playback queue, freeing memory. */
static void espeak_clear_playback_queue()
{
	pthread_mutex_lock(&playback_queue_mutex);

	while (NULL != playback_queue) {
		TPlaybackQueueEntry *playback_queue_entry =
		    playback_queue->data;
		espeak_delete_playback_queue_entry(playback_queue_entry);
		playback_queue =
		    g_slist_remove(playback_queue, playback_queue->data);
	}
	playback_queue = NULL;
	playback_queue_size = 0;
	pthread_mutex_unlock(&playback_queue_mutex);
}

/* Sends a chunk of audio to the audio player and waits for completion or error. */
static gboolean espeak_send_to_audio(TPlaybackQueueEntry * playback_queue_entry)
{
	int ret = 0;
	AudioTrack track;
	track.num_samples = playback_queue_entry->data.audio.num_samples;
	track.num_channels = 1;
	track.sample_rate = speak_queue_sample_rate;
	track.bits = 16;
	track.samples = playback_queue_entry->data.audio.audio_chunk;

	DBG(DBG_MODNAME " Sending %i samples to audio.", track.num_samples);
	ret = module_tts_output(track, SPD_AUDIO_LE);
	if (ret < 0) {
		DBG("ERROR: Can't play track for unknown reason.");
		return FALSE;
	}
	DBG(DBG_MODNAME " Sent to audio.");
	return TRUE;
}

/* Playback thread. */
static void *_espeak_play(void *nothing)
{
	char *markId;
	TPlaybackQueueEntry *playback_queue_entry = NULL;

	DBG(DBG_MODNAME " Playback thread starting.......");

	/* Block all signals to this thread. */
	set_speaking_thread_parameters();

	pthread_mutex_lock(&espeak_play_mutex);
	while (!espeak_close_requested) {
		espeak_play_sleeping = 1;
		pthread_cond_signal(&espeak_play_sleeping_cond);
		while (espeak_state < BEFORE_PLAY) {
			pthread_cond_wait(&espeak_play_cond, &espeak_play_mutex);
		}
		espeak_play_sleeping = 0;
		pthread_cond_signal(&espeak_play_sleeping_cond);
		DBG(DBG_MODNAME " Playback.");
		if (espeak_close_requested)
			break;
		pthread_mutex_unlock(&espeak_play_mutex);

		while (1) {
			gboolean finished = FALSE;
			playback_queue_entry = playback_queue_pop();
			if (playback_queue_entry == NULL) {
				DBG(DBG_MODNAME " playback thread detected stop.");
				break;
			}

			switch (playback_queue_entry->type) {
			case ESPEAK_QET_AUDIO:
				espeak_send_to_audio(playback_queue_entry);
				break;
			case ESPEAK_QET_INDEX_MARK:
				markId = playback_queue_entry->data.markId;
				DBG(DBG_MODNAME " reporting index mark |%s|.",
				    markId);
				module_report_index_mark(markId);
				DBG(DBG_MODNAME " index mark reported.");
				pthread_mutex_lock(&espeak_state_mutex);
				if (espeak_state == SPEAKING
				    && espeak_pause_state ==
				    ESPEAK_PAUSE_REQUESTED
				    && espeak_stop_or_pause_sleeping
				    && g_str_has_prefix(markId, "__spd_")) {
					DBG(DBG_MODNAME " Pause requested in playback thread.  Stopping.");
					espeak_stop_requested = TRUE;
					espeak_pause_state =
					    ESPEAK_PAUSE_MARK_REPORTED;
					pthread_cond_signal
					    (&espeak_stop_or_pause_cond);
					finished = TRUE;
				}
				pthread_mutex_unlock(&espeak_state_mutex);
				break;
			case ESPEAK_QET_SOUND_ICON:
				module_play_file(playback_queue_entry->
						 data.sound_icon_filename);
				break;
			case ESPEAK_QET_BEGIN:{
					gboolean report_begin = FALSE;
					pthread_mutex_lock(&espeak_state_mutex);
					if (espeak_state == BEFORE_PLAY) {
						espeak_state = SPEAKING;
						report_begin = TRUE;
					}
					pthread_mutex_unlock
					    (&espeak_state_mutex);
					if (report_begin)
						module_report_event_begin();
					break;
				}
			case ESPEAK_QET_END:
				pthread_mutex_lock(&espeak_state_mutex);
				DBG(DBG_MODNAME " playback thread got END from queue.");
				if (espeak_state == SPEAKING) {
					if (!espeak_stop_requested) {
						DBG(DBG_MODNAME " playback thread reporting end.");
						espeak_state = IDLE;
						espeak_pause_state =
						    ESPEAK_PAUSE_OFF;
					}
					finished = TRUE;
				}
				pthread_mutex_unlock(&espeak_state_mutex);
				if (finished)
					module_report_event_end();
				break;
			}

			espeak_delete_playback_queue_entry
			    (playback_queue_entry);
			if (finished)
				break;
		}
	}
	espeak_play_sleeping = 1;
	pthread_mutex_unlock(&espeak_play_mutex);
	DBG(DBG_MODNAME " Playback thread ended.......");
	return 0;
}

int module_speak_queue_stop_requested(void)
{
	return espeak_stop_requested;
}

void module_speak_queue_stop(void)
{
	pthread_mutex_lock(&espeak_state_mutex);
	pthread_mutex_lock(&espeak_stop_or_pause_mutex);
	if (espeak_state != IDLE &&
	    !espeak_stop_requested &&
	    espeak_stop_or_pause_sleeping) {
		DBG(DBG_MODNAME " stopping...");
		espeak_stop_requested = TRUE;
		/* Wake the stop_or_pause thread. */
		pthread_cond_signal(&espeak_stop_or_pause_cond);
	} else {
		DBG(DBG_MODNAME " Cannot stop now.");
	}
	pthread_mutex_unlock(&espeak_stop_or_pause_mutex);
	pthread_mutex_unlock(&espeak_state_mutex);
}

void module_speak_queue_pause(void)
{
	pthread_mutex_lock(&espeak_state_mutex);
	if (espeak_pause_state == ESPEAK_PAUSE_OFF && !espeak_stop_requested) {
		espeak_pause_state = ESPEAK_PAUSE_REQUESTED;
	}
	pthread_mutex_unlock(&espeak_state_mutex);
}

void module_speak_queue_terminate(void)
{
	espeak_stop_requested = TRUE;
	espeak_close_requested = TRUE;

	pthread_mutex_lock(&playback_queue_mutex);
	pthread_cond_broadcast(&playback_queue_condition);
	pthread_mutex_unlock(&playback_queue_mutex);

	pthread_cond_signal(&espeak_play_cond);
	pthread_cond_signal(&espeak_stop_or_pause_cond);
	/* Give threads a chance to quit on their own terms. */
	g_usleep(25000);

	/* Make sure threads have really exited */
	pthread_cancel(espeak_play_thread);
	pthread_cancel(espeak_stop_or_pause_thread);

	DBG(DBG_MODNAME " Joining play thread.");
	pthread_join(espeak_play_thread, NULL);
	DBG(DBG_MODNAME " Joining stop thread.");
	pthread_join(espeak_stop_or_pause_thread, NULL);
}

void module_speak_queue_free(void)
{
	DBG(DBG_MODNAME " Freeing resources.");
	espeak_clear_playback_queue();

	pthread_mutex_destroy(&espeak_state_mutex);
	pthread_mutex_destroy(&espeak_play_mutex);
	pthread_mutex_destroy(&espeak_stop_or_pause_mutex);
	pthread_mutex_destroy(&playback_queue_mutex);
	pthread_cond_destroy(&playback_queue_condition);
	pthread_cond_destroy(&espeak_play_cond);
	pthread_cond_destroy(&espeak_play_sleeping_cond);
	pthread_cond_destroy(&espeak_stop_or_pause_cond);
	pthread_cond_destroy(&espeak_stop_or_pause_sleeping_cond);
}

/* Stop or Pause thread. */
static void *_espeak_stop_or_pause(void *nothing)
{
	int ret;

	DBG(DBG_MODNAME " Stop or pause thread starting.......");

	/* Block all signals to this thread. */
	set_speaking_thread_parameters();

	pthread_mutex_lock(&espeak_stop_or_pause_mutex);
	while (!espeak_close_requested) {
		espeak_stop_or_pause_sleeping = 1;
		pthread_cond_signal(&espeak_stop_or_pause_sleeping_cond);
		while (!espeak_stop_requested)
			pthread_cond_wait(&espeak_stop_or_pause_cond, &espeak_stop_or_pause_mutex);
		espeak_stop_or_pause_sleeping = 0;
		pthread_cond_signal(&espeak_stop_or_pause_sleeping_cond);

		DBG(DBG_MODNAME " Stop or pause.");
		if (espeak_close_requested)
			break;
		pthread_mutex_unlock(&espeak_stop_or_pause_mutex);

		pthread_mutex_lock(&playback_queue_mutex);
		pthread_cond_broadcast(&playback_queue_condition);
		pthread_mutex_unlock(&playback_queue_mutex);

		if (module_audio_id) {
			pthread_mutex_lock(&espeak_state_mutex);
			espeak_state = IDLE;
			pthread_mutex_unlock(&espeak_state_mutex);
			DBG(DBG_MODNAME " Stopping audio.");
			ret = spd_audio_stop(module_audio_id);
			if (ret != 0)
				DBG("spd_audio_stop returned non-zero value.");
			pthread_mutex_lock(&espeak_play_mutex);
			while (!espeak_play_sleeping) {
				ret = spd_audio_stop(module_audio_id);
				if (ret != 0)
					DBG("spd_audio_stop returned non-zero value.");
				pthread_mutex_unlock(&espeak_play_mutex);
				g_usleep(5000);
				pthread_mutex_lock(&espeak_play_mutex);
			}
			pthread_mutex_unlock(&espeak_play_mutex);
		} else {
			pthread_mutex_lock(&espeak_play_mutex);
			while (!espeak_play_sleeping)
				pthread_cond_wait(&espeak_play_sleeping_cond, &espeak_play_mutex);
			pthread_mutex_unlock(&espeak_play_mutex);
		}

		DBG(DBG_MODNAME " Waiting for synthesis to stop.");

		module_speak_queue_cancel();

		DBG(DBG_MODNAME " Clearing playback queue.");
		espeak_clear_playback_queue();

		int save_pause_state = espeak_pause_state;
		pthread_mutex_lock(&espeak_state_mutex);
		module_speak_queue_reset();
		pthread_mutex_unlock(&espeak_state_mutex);

		if (save_pause_state == ESPEAK_PAUSE_MARK_REPORTED) {
			module_report_event_pause();
		} else {
			module_report_event_stop();
		}

		DBG(DBG_MODNAME " Stop or pause thread ended.......\n");
		pthread_mutex_lock(&espeak_stop_or_pause_mutex);
	}
	pthread_mutex_unlock(&espeak_stop_or_pause_mutex);
	pthread_exit(NULL);
}
