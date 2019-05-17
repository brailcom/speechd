/*
 * module_utils_speak_queue.h - Speak queue helper for Speech Dispatcher modules
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

#ifndef __MODULE_UTILS_SPEAK_QUEUE_H
#define __MODULE_UTILS_SPEAK_QUEUE_H

#include <pthread.h>
#include <glib.h>

/* To be called in module_init after synth initialization, to start playback
 * threads.  */
int module_speak_queue_init(int sample_rate, int maxsize, char **status_info);


/* To be called from module_speak before synthesizing the voice.  */
int module_speak_queue_before_synth(void);


/* To be called from the synth callback before looking through its events.  */
int module_speak_queue_before_play(void);

/* To be called from the synth callback to push different types of events.  */
gboolean module_speak_queue_add_audio(short *audio_chunk, int num_samples);
gboolean module_speak_queue_add_mark(const char *markId);
gboolean module_speak_queue_add_sound_icon(const char *filename);
/* To be called on the last synth callback call.  */
gboolean module_speak_queue_add_end(void);

/* To be called in the synth callback to look for early stopping.  */
int module_speak_queue_stop_requested(void);


/* To be called from module_stop.  */
void module_speak_queue_stop(void);

/* To be called from module_pause.  */
void module_speak_queue_pause(void);

/* To be called first from module_close to terminate audio early.  */
void module_speak_queue_terminate(void);

/* To be called last from module_close to release resources.  */
void module_speak_queue_free(void);


/* To be provided by the module, shall stop the synthesizer, i.e. make
 * it stop calling the module callback, and thus make the module stop calling
 * module_speak_queue_add_*.  */
void module_speak_queue_cancel(void);

#endif /* #ifndef __MODULE_UTILS_SPEAK_QUEUE_H */
