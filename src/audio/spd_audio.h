
/*
 * spd_audio.h -- The SPD Audio Library Header
 *
 * Copyright (C) 2004 Brailcom, o.p.s.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: spd_audio.h,v 1.8 2005-05-13 10:34:55 hanke Exp $
 */

#include <pthread.h>

#ifdef WITH_NAS
#include <audio/audiolib.h>
#include <audio/soundlib.h>
#endif

#ifdef WITH_ALSA
#include <alsa/asoundlib.h>
#endif

#define AUDIO_BUF_SIZE 4096

typedef enum{AUDIO_OSS = 0, AUDIO_NAS = 1, AUDIO_ALSA=2} AudioOutputType;

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
snd_pcm_t *pcm;			/* identifier of the ALSA device */
snd_pcm_hw_params_t *hw_params;	/* parameters of sound */
pthread_mutex_t pcm_mutex;	/* mutex to guard the state of the device */
#endif

#ifdef WITH_NAS
    AuServer *aud;
    AuFlowID flow;
    pthread_mutex_t flow_mutex;
    pthread_t nas_event_handler;
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

int spd_audio_play(AudioID *id, AudioTrack track);

int spd_audio_stop(AudioID *id);

int spd_audio_close(AudioID *id);

int spd_audio_set_volume(AudioID *id, int volume);
