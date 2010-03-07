
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
 * $Id: spd_audio.h,v 1.21 2008-10-15 17:28:17 hanke Exp $
 */

#ifndef __SPD_AUDIO_H
#define __SPD_AUDIO_H

#include <pthread.h>
#include <sys/types.h>

#define AUDIO_BUF_SIZE 4096

typedef enum{AUDIO_OSS = 0, AUDIO_NAS = 1, AUDIO_ALSA=2, AUDIO_PULSE=3, AUDIO_LIBAO=4} AudioOutputType;
typedef enum{SPD_AUDIO_LE, SPD_AUDIO_BE} AudioFormat;

typedef struct{
    int bits;
    int num_channels;
    int sample_rate;

    int num_samples;
    signed short *samples;
}AudioTrack;

struct spd_audio_plugin;

typedef struct{
    AudioOutputType type;

    int volume;
    AudioFormat format;

    struct spd_audio_plugin *function;

    int working;
}AudioID;

typedef struct spd_audio_plugin {
    AudioID * (* open)  (void** pars);
    int   (* play)  (AudioID *id, AudioTrack track);
    int   (* stop)  (AudioID *id);
    int   (* close) (AudioID *id);
    int   (* set_volume) (AudioID *id, int);
    void  (* set_loglevel) (int level);
} spd_audio_plugin_t;

AudioID* spd_audio_open(AudioOutputType type, void **pars, char **error);

int spd_audio_play(AudioID *id, AudioTrack track, AudioFormat format);

int spd_audio_stop(AudioID *id);

int spd_audio_close(AudioID *id);

int spd_audio_set_volume(AudioID *id, int volume);

void spd_audio_set_loglevel(AudioID *id, int level);

#endif /* ifndef #__SPD_AUDIO_H */
