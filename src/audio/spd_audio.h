
/*
 * spd_audio.h -- The SPD Audio Library Header
 *
 * Copyright (C) 2004 Brailcom, o.p.s.
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
 * along with this package; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * $Id: spd_audio.h,v 1.17 2007-11-18 17:24:13 gcasse Exp $
 */

#include <pthread.h>
#include <sys/types.h>

#ifdef WITH_NAS
#include <audio/audiolib.h>
#include <audio/soundlib.h>
#endif

#ifdef WITH_ALSA
#include <alsa/asoundlib.h>
#endif

#ifdef WITH_PULSE
#include <pulse/pulseaudio.h>
#include <stdio.h> // TBD
#endif

#define AUDIO_BUF_SIZE 4096

typedef enum{AUDIO_OSS = 0, AUDIO_NAS = 1, AUDIO_ALSA=2, AUDIO_PULSE=3} AudioOutputType;
typedef enum{SPD_AUDIO_LE, SPD_AUDIO_BE} AudioFormat;

#if defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN
AudioFormat audio_endian = SPD_AUDIO_BE;
#else
AudioFormat audio_endian = SPD_AUDIO_LE;
#endif

typedef struct{
    int bits;
    int num_channels;
    int sample_rate;

    int num_samples;
    signed short *samples;
}AudioTrack;

typedef struct{
    int   (* open)  (void *id, void** pars);
    int   (* play)  (void *id, AudioTrack track);
    int   (* stop)  (void *id);
    int   (* close) (void *id);
    int   (* set_volume) (void *id, int);
}Funct;

typedef struct{
    AudioOutputType type;

    int volume;

#ifdef WITH_OSS
    /* OSS specific */
    int fd;
    char* device_name;
    pthread_mutex_t fd_mutex;
    pthread_cond_t pt_cond;
    pthread_mutex_t pt_mutex;
#endif

#ifdef WITH_ALSA
    snd_pcm_t *alsa_pcm;		/* identifier of the ALSA device */
    snd_pcm_hw_params_t *alsa_hw_params;	/* parameters of sound */
    snd_pcm_sw_params_t *alsa_sw_params;	/* parameters of playback */
    snd_pcm_uframes_t alsa_buffer_size;
    pthread_mutex_t alsa_pcm_mutex;	/* mutex to guard the state of the device */
    int alsa_stop_pipe[2];		/* Pipe for communication about stop requests*/
    int alsa_fd_count;		/* Counter of descriptors to poll */
    struct pollfd *alsa_poll_fds; /* Descriptors to poll */
    int alsa_opened; 		/* 1 between snd_pcm_open and _close, 0 otherwise */
    char *alsa_device_name; 	/* the name of the device to open */
#endif

#ifdef WITH_NAS
    AuServer *aud;
    AuFlowID flow;
    pthread_mutex_t flow_mutex;
    pthread_t nas_event_handler;
#endif

#ifdef WITH_PULSE
    pa_context *pulse_context;
    pa_stream *pulse_stream;
    pa_threaded_mainloop *pulse_mainloop;
    pa_cvolume pulse_volume;
    int pulse_volume_valid;
    int pulse_do_trigger;
    int pulse_time_offset_msec;
    int pulse_just_flushed;
    int pulse_connected;
    int pulse_success; // status for synchronous operation */
    int pulse_stop_required;
    pthread_mutex_t pulse_mutex;
    pa_time_event *pulse_volume_time_event;
    int pulse_max_length;
    int pulse_target_length;
    int pulse_pre_buffering;
    int pulse_min_request;
    char* pulse_server;
#endif

    Funct *function;

    int working;
}AudioID;

typedef int (* t_audio_open)(AudioID *id, void** pars);
typedef int (* t_audio_play)(AudioID *id, AudioTrack track);
typedef int (* t_audio_stop)(AudioID *id);
typedef int (* t_audio_close)(AudioID *id);
typedef int (* t_audio_set_volume)(AudioID *id, int volume);

typedef struct{
    t_audio_open open;
    t_audio_play play;
    t_audio_stop stop;
    t_audio_close close;
    t_audio_set_volume set_volume;
}AudioFunctions;    

AudioID* spd_audio_open(AudioOutputType type, void **pars, char **error);

int spd_audio_play(AudioID *id, AudioTrack track, AudioFormat format);

int spd_audio_stop(AudioID *id);

int spd_audio_close(AudioID *id);

int spd_audio_set_volume(AudioID *id, int volume);
