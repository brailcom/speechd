/*
 * pulse.c -- The simple pulseaudio backend for the spd_audio library.
 *
 * Copyright 2007-2009 Gilles Casse <gcasse@oralux.org>
 * Copyright 2008-2010 Brailcom, o.p.s
 * Copyright 2008-2015 Luke Yelavich <luke.yelavich@canonical.com>
 * Copyright 2009 Rui Batista <ruiandrebatista@gmail.com>
 * Copyright 2009 Marco Skambraks <marco@openblinux.de>
 * Copyright 2010 Andrei Kholodnyi <Andrei.Kholodnyi@gmail.com>
 * Copyright 2010 Christopher Brannon <cmbrannon79@gmail.com>
 * Copyright 2010-2011 William Hubbs <w.d.hubbs@gmail.com>
 * Copyright 2015 Jeremy Whiting <jpwhiting@kde.org>
 * Copyright 2018-2025 Samuel Thibault <samuel.thibault@ens-lyon.org>
 * Copyright 2004-2006 Lennart Poettering
 *
 * Copied from Luke Yelavich's libao.c driver, and merged with code from
 * Marco's ao_pulse.c driver, by Bill Cox, Dec 21, 2009.
 *
 * Minor changes be Rui Batista <rui.batista@ist.utl.pt> to configure settings through speech-dispatcher configuration files
 * Date: Dec 22, 2009
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <glib.h>

#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>
#include <pulse/error.h>

#ifdef USE_DLOPEN
#define SPD_AUDIO_PLUGIN_ENTRY spd_audio_plugin_get
#else
#define SPD_AUDIO_PLUGIN_ENTRY spd_pulse_LTX_spd_audio_plugin_get
#endif
#include <spd_audio_plugin.h>

#include "../common/common.h"

typedef struct spd_pa_simple spd_pa_simple;

typedef struct {
	AudioID id;
	spd_pa_simple *pa_simple;
	char *pa_server;
	char *pa_device;
	char *pa_name;
	int pa_min_audio_length;	// in ms
	int pa_current_rate;	// Sample rate for currently PA connection
	int pa_current_bps;	// Bits per sample rate for currently PA connection
	int pa_current_channels;	// Number of channels for currently PA connection
} spd_pulse_id_t;

/* Initial values, most often what synths will requests */
#define DEF_RATE 44100
#define DEF_CHANNELS 1
#define DEF_BYTES_PER_SAMPLE 2

/* This is the smallest audio sound we are expected to play immediately without buffering. */
/* Changed to define on config file. Default is the same. */
/* Default to 10 ms of latency */
#define DEFAULT_PA_MIN_AUDIO_LENGTH 10

static int pulse_log_level;
static char const *pulse_play_cmd = "paplay -n speech-dispatcher-generic";

/* Put a message into the logfile (stderr) */
#define MSG(level, arg, ...) if (level <= pulse_log_level) { MSG(level, "Pulse: " arg, ##__VA_ARGS__); }
#define ERR(arg, ...) MSG(0, "Pulse ERROR: " arg, ##__VA_ARGS__)

/* The following is a copy of pulseaudio's src/pulse/simple.c
 * that was modified to fit our needs: very low-latency playback start and stop,
 * but still ample buffering to avoid underruns. */
struct spd_pa_simple {
	pa_threaded_mainloop *mainloop;
	pa_context *context;
	pa_stream *stream;

	int operation_success;

	int32_t bytes_per_s;

	int playing;

	pthread_mutex_t drain_lock;
	pthread_cond_t drain_cond;

	/* Stopping. Flushing the stream cuts the audio off mid-wave, which
	   clicks, so instead the part not played yet is overwritten with a
	   fade-out and silence. As it cannot be read back from the server, a
	   copy of what was written last is kept here, in the order the server
	   queues it. */
	uint8_t *history;
	size_t history_size;
	size_t history_pos;	/* where the next byte goes */
	size_t history_fill;
	size_t frame_size;
	int can_fade;		/* 16 bit samples in native byte order */
	int unfaded;		/* audio was written since the last stop */

	/* A drain has finished, start the next write at the read index, see
	   spd_pa_simple_server_drain() */
	int seek_on_read;
};

/* How long the fade-out on a stop takes */
#define FADE_OUT_MS 8
/* The most audio left as it is before the fade-out, see
   spd_pa_simple_fade_out() */
#define FADE_OUT_MAX_DELAY_MS 50

static void history_append(spd_pa_simple *p, const uint8_t *data, size_t len)
{
	if (len >= p->history_size) {
		data += len - p->history_size;
		len = p->history_size;
	}
	while (len > 0) {
		size_t n = p->history_size - p->history_pos;
		if (n > len)
			n = len;
		memcpy(p->history + p->history_pos, data, n);
		p->history_pos = (p->history_pos + n) % p->history_size;
		p->history_fill = MIN(p->history_fill + n, p->history_size);
		data += n;
		len -= n;
	}
}

/* Copies n bytes, starting back bytes before the end of the history. */
static void history_get(spd_pa_simple *p, size_t back, uint8_t *out, size_t n)
{
	size_t pos =
	    (p->history_pos + p->history_size - back) % p->history_size;
	while (n > 0) {
		size_t c = p->history_size - pos;
		if (c > n)
			c = n;
		memcpy(out, p->history + pos, c);
		pos = (pos + c) % p->history_size;
		out += c;
		n -= c;
	}
}

/* Forgets the last n bytes of the history. */
static void history_rewind(spd_pa_simple *p, size_t n)
{
	p->history_pos =
	    (p->history_pos + p->history_size - n) % p->history_size;
	p->history_fill -= n;
}

/* The gain of the frame at position of count frames of a fade-out, going
   smoothly from just below 1 to just above 0. */
static float fade_out_gain(size_t position, size_t count)
{
	float t = (float)(position + 1) / (float)(count + 1);
	return 1.0f - t * t * (3.0f - 2.0f * t);
}

#define CHECK_SUCCESS_GOTO(p, rerror, expression, label)			\
	do {									\
		if (!(expression)) {						\
			if (rerror)						\
				*(rerror) = pa_context_errno((p)->context);	\
			goto label;						\
		}								\
	} while(0);

#define CHECK_DEAD_GOTO(p, rerror, label)					\
	do {									\
		if (!(p)->context || !PA_CONTEXT_IS_GOOD(pa_context_get_state((p)->context)) || \
			!(p)->stream || !PA_STREAM_IS_GOOD(pa_stream_get_state((p)->stream))) { \
			if (((p)->context && pa_context_get_state((p)->context) == PA_CONTEXT_FAILED) || \
				((p)->stream && pa_stream_get_state((p)->stream) == PA_STREAM_FAILED)) { \
				if (rerror)					\
					*(rerror) = pa_context_errno((p)->context);	\
			} else							\
				if (rerror)					\
					*(rerror) = PA_ERR_BADSTATE;		\
			goto label;						\
		}								\
	} while(0);

static void context_state_cb(pa_context *c, void *userdata) {
	spd_pa_simple *p = userdata;

	switch (pa_context_get_state(c)) {
		case PA_CONTEXT_READY:
		case PA_CONTEXT_TERMINATED:
		case PA_CONTEXT_FAILED:
			pa_threaded_mainloop_signal(p->mainloop, 0);
			break;

		case PA_CONTEXT_UNCONNECTED:
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
			break;
	}
}

static void stream_state_cb(pa_stream *s, void * userdata) {
	spd_pa_simple *p = userdata;

	switch (pa_stream_get_state(s)) {

		case PA_STREAM_READY:
		case PA_STREAM_FAILED:
		case PA_STREAM_TERMINATED:
			pa_threaded_mainloop_signal(p->mainloop, 0);
			break;

		case PA_STREAM_UNCONNECTED:
		case PA_STREAM_CREATING:
			break;
	}
}

static void stream_request_cb(pa_stream *s, size_t length, void *userdata) {
	spd_pa_simple *p = userdata;

	pa_threaded_mainloop_signal(p->mainloop, 0);
}

static void stream_latency_update_cb(pa_stream *s, void *userdata) {
	spd_pa_simple *p = userdata;

	pa_threaded_mainloop_signal(p->mainloop, 0);
}

static void spd_pa_simple_free(spd_pa_simple *s);

static spd_pa_simple* spd_pa_simple_new(
		const char *server,
		const char *name,
		const char *dev,
		const char *stream_name,
		const pa_sample_spec *ss,
		const pa_buffer_attr *attr,
		int *rerror) {

	spd_pa_simple *p;
	int error = PA_ERR_INTERNAL, r;

	p = pa_xnew0(spd_pa_simple, 1);

	if (!(p->mainloop = pa_threaded_mainloop_new()))
		goto fail;

	if (!(p->context = pa_context_new(pa_threaded_mainloop_get_api(p->mainloop), name)))
		goto fail;

	pa_context_set_state_callback(p->context, context_state_cb, p);

	if (pa_context_connect(p->context, server, 0, NULL) < 0) {
		error = pa_context_errno(p->context);
		goto fail;
	}

	pa_threaded_mainloop_lock(p->mainloop);

	if (pa_threaded_mainloop_start(p->mainloop) < 0)
		goto unlock_and_fail;

	for (;;) {
		pa_context_state_t state;

		state = pa_context_get_state(p->context);

		if (state == PA_CONTEXT_READY)
			break;

		if (!PA_CONTEXT_IS_GOOD(state)) {
			error = pa_context_errno(p->context);
			goto unlock_and_fail;
		}

		/* Wait until the context is ready */
		pa_threaded_mainloop_wait(p->mainloop);
	}

	if (!(p->stream = pa_stream_new(p->context, stream_name, ss, NULL))) {
		error = pa_context_errno(p->context);
		goto unlock_and_fail;
	}

	pa_stream_set_state_callback(p->stream, stream_state_cb, p);
	pa_stream_set_read_callback(p->stream, stream_request_cb, p);
	pa_stream_set_write_callback(p->stream, stream_request_cb, p);
	pa_stream_set_latency_update_callback(p->stream, stream_latency_update_cb, p);

	r = pa_stream_connect_playback(p->stream, dev, attr,
				       PA_STREAM_INTERPOLATE_TIMING
				       |PA_STREAM_ADJUST_LATENCY
				       |PA_STREAM_AUTO_TIMING_UPDATE, NULL, NULL);

	if (r < 0) {
		error = pa_context_errno(p->context);
		goto unlock_and_fail;
	}

	for (;;) {
		pa_stream_state_t state;

		state = pa_stream_get_state(p->stream);

		if (state == PA_STREAM_READY)
			break;

		if (!PA_STREAM_IS_GOOD(state)) {
			error = pa_context_errno(p->context);
			goto unlock_and_fail;
		}

		/* Wait until the stream is ready */
		pa_threaded_mainloop_wait(p->mainloop);
	}

	pa_threaded_mainloop_unlock(p->mainloop);

	p->playing = 0;
	p->bytes_per_s = ss->rate * ss->channels * (ss->format == PA_SAMPLE_U8 ? 1 : 2);
	p->frame_size = pa_frame_size(ss);
	p->can_fade = ss->format == PA_SAMPLE_S16NE;
	/* One second, far more than the server ever queues */
	p->history_size = p->bytes_per_s - p->bytes_per_s % p->frame_size;
	p->history = pa_xmalloc(p->history_size);
	pthread_mutex_init(&p->drain_lock, NULL);
	pthread_cond_init(&p->drain_cond, NULL);

	return p;

unlock_and_fail:
	pa_threaded_mainloop_unlock(p->mainloop);

fail:
	if (rerror)
		*rerror = error;
	spd_pa_simple_free(p);
	return NULL;
}

static void spd_pa_simple_free(spd_pa_simple *s) {
	if (s->mainloop)
		pa_threaded_mainloop_stop(s->mainloop);

	pa_xfree(s->history);

	if (s->stream)
		pa_stream_unref(s->stream);

	if (s->context) {
		pa_context_disconnect(s->context);
		pa_context_unref(s->context);
	}

	if (s->mainloop)
		pa_threaded_mainloop_free(s->mainloop);

	pa_xfree(s);
}

static void success_cb(pa_stream *s, int success, void *userdata) {
	spd_pa_simple *p = userdata;

	p->operation_success = success;
	pa_threaded_mainloop_signal(p->mainloop, 0);
}

static int spd_pa_simple_fade_out(spd_pa_simple *p);

/* Assumes p is locked */
static int spd_pa_simple_do_flush(spd_pa_simple *p, int *rerror) {
	pa_operation *o = NULL;

	p->playing = 0;
	CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);

	/* A stop gets repeated until playback has ended, see
	   speak_queue_stop_or_pause(), so there is often nothing new to stop */
	if (!p->unfaded)
		return 0;
	/* Cleared before stopping, not after: waiting for the server below
	   unlocks the mainloop, and another thread stopping meanwhile must not
	   stop the same audio a second time */
	p->unfaded = 0;

	if (p->can_fade && spd_pa_simple_fade_out(p) == 0)
		return 0;

	o = pa_stream_flush(p->stream, success_cb, p);
	CHECK_SUCCESS_GOTO(p, rerror, o, unlock_and_fail);

	p->operation_success = 0;
	while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
		pa_threaded_mainloop_wait(p->mainloop);
		CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);
	}
	CHECK_SUCCESS_GOTO(p, rerror, p->operation_success, unlock_and_fail);

	pa_operation_unref(o);

	return 0;

unlock_and_fail:

	if (o) {
		pa_operation_cancel(o);
		pa_operation_unref(o);
	}

	return -1;
}

/* Stops what is queued on the server without a click: overwrites the part not
   played yet, from the read index the server reports, with a fade-out and then
   silence. By the time the write reaches the server, it may have read on by a
   cycle, and skipping what it has passed, it would skip the fade-out too. So
   the first cycle, the minimum request, is left as it is, and the fade-out
   comes after it.

   Returns 0 when done, a negative value when the stream has to be flushed
   instead. Assumes p is locked. */
static int spd_pa_simple_fade_out(spd_pa_simple *p)
{
	pa_operation *o;
	const pa_timing_info *info;
	const pa_buffer_attr *attr;
	int64_t queued;
	size_t keep, fade, frames, channels, i, c;
	uint8_t *buf;
	int r;

	o = pa_stream_update_timing_info(p->stream, success_cb, p);
	if (!o)
		return -1;
	p->operation_success = 0;
	while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
		pa_threaded_mainloop_wait(p->mainloop);
		if (!p->stream
		    || !PA_STREAM_IS_GOOD(pa_stream_get_state(p->stream))) {
			pa_operation_cancel(o);
			pa_operation_unref(o);
			return -1;
		}
	}
	pa_operation_unref(o);
	if (!p->operation_success)
		return -1;

	info = pa_stream_get_timing_info(p->stream);
	if (!info || info->write_index_corrupt || info->read_index_corrupt)
		return -1;

	queued = info->write_index - info->read_index;
	queued -= queued % (int64_t) p->frame_size;
	if (queued <= 0)
		return 0;	/* everything has been played */
	if ((size_t)queued > p->history_fill)
		return -1;

	fade = (size_t)p->bytes_per_s * FADE_OUT_MS / 1000;
	fade -= fade % p->frame_size;
	fade = MIN(fade, (size_t)queued);

	keep = 0;
	attr = pa_stream_get_buffer_attr(p->stream);
	if (attr && attr->minreq != (uint32_t)-1)
		keep =
		    MIN(attr->minreq,
			(size_t)p->bytes_per_s * FADE_OUT_MAX_DELAY_MS / 1000);
	keep -= keep % p->frame_size;
	keep = MIN(keep, (size_t)queued - fade);

	buf = pa_xmalloc0(queued);
	history_get(p, queued, buf, keep + fade);
	frames = fade / p->frame_size;
	channels = p->frame_size / sizeof(int16_t);
	for (i = 0; i < frames; i++) {
		int16_t *frame = (int16_t *) (buf + keep + i * p->frame_size);
		float gain = fade_out_gain(i, frames);
		for (c = 0; c < channels; c++)
			frame[c] = (int16_t) (frame[c] * gain);
	}

	r = pa_stream_write(p->stream, buf, queued, NULL, info->read_index,
			    PA_SEEK_ABSOLUTE);
	if (r >= 0) {
		/* The history mirrors the server's queue, so this is now its
		   end */
		history_rewind(p, queued);
		history_append(p, buf, queued);
		MSG(4,
		    "Stopped with %lld bytes queued, faded out %zu after %zu\n",
		    (long long)queued, fade, keep);
	}
	pa_xfree(buf);
	return r < 0 ? -1 : 0;
}

static int spd_pa_simple_write(spd_pa_simple *p, const void*data, size_t length, int *rerror) {
	pa_threaded_mainloop_lock(p->mainloop);

	CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);

	p->playing = 1;

	while (length > 0) {
		size_t l;
		int r;

		while (p->playing && !(l = pa_stream_writable_size(p->stream))) {
			pa_threaded_mainloop_wait(p->mainloop);
			CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);
		}

		if (!p->playing)
			break;

		CHECK_SUCCESS_GOTO(p, rerror, l != (size_t) -1, unlock_and_fail);

		if (l > length)
			l = length;

		if (p->seek_on_read) {
			p->seek_on_read = 0;
			r = pa_stream_write(p->stream, data, l, NULL, 0LL,
					    PA_SEEK_RELATIVE_ON_READ);
		} else
			r = pa_stream_write(p->stream, data, l, NULL, 0LL,
					    PA_SEEK_RELATIVE);
		CHECK_SUCCESS_GOTO(p, rerror, r >= 0, unlock_and_fail);
		history_append(p, data, l);
		p->unfaded = 1;

		data = (const uint8_t*) data + l;
		length -= l;
	}

	if (pulse_log_level >= 4) {
		pa_usec_t t = 0;

		if (pa_stream_get_latency(p->stream, &t, NULL) >= 0) {
			MSG(4, "Wrote data, playing with latency %lldµs\n", (long long) t);
		}
	}

	if (!p->playing) {
		/* We got interrupted, flush.  */
		spd_pa_simple_do_flush(p, NULL);
	}

	pa_threaded_mainloop_unlock(p->mainloop);
	return 0;

unlock_and_fail:
	pa_threaded_mainloop_unlock(p->mainloop);
	return -1;
}

static int spd_pa_simple_drain(spd_pa_simple *p, pa_usec_t overlap, int *rerror) {
	pa_operation *o = NULL;

	pa_threaded_mainloop_lock(p->mainloop);

	CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);

	MSG(4, "draining to overlap %llu\n", (unsigned long long) overlap);

	/* Drain until requested overlap */
	while(1) {
		if (!p->playing) {
			/* We got interrupted, flush.  */
			spd_pa_simple_do_flush(p, NULL);
			break;
		}

		o = pa_stream_update_timing_info(p->stream, success_cb, p);
		CHECK_SUCCESS_GOTO(p, rerror, o, unlock_and_fail);

		p->operation_success = 0;
		while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
			pa_threaded_mainloop_wait(p->mainloop);
			CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);
		}
		CHECK_SUCCESS_GOTO(p, rerror, p->operation_success, unlock_and_fail);

		pa_operation_unref(o);
		o = NULL;

		const pa_timing_info *info = pa_stream_get_timing_info(p->stream);
		int64_t left_us;

		if (info) {
			int64_t left = info->write_index - info->read_index;
			left_us = left * PA_USEC_PER_SEC / p->bytes_per_s - info->transport_usec - info->sink_usec;

			//MSG(5, "left %lld bytes, %lldµs\n", (long long) left, (long long) left_us);

			if (left_us <= (int64_t) overlap)
				break;
			left_us -= overlap;
		} else {
			/* Wait a bit for information */
			left_us = 1000;
		}

		/* Do not sleep too much to avoid miss-ups */
		if (left_us > 10000)
			left_us = 10000;

		pa_threaded_mainloop_unlock(p->mainloop);

		/* Ideally pulseaudio would provide a pa_threaded_mainloop_timedwait */
		pthread_mutex_lock(&p->drain_lock);
		if (p->playing)
		{
			struct timespec ts;

			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_nsec += left_us * 1000;
			while (ts.tv_nsec >= 1000000000)
			{
				ts.tv_nsec -= 1000000000;
				ts.tv_sec++;
			}

			pthread_cond_timedwait(&p->drain_cond, &p->drain_lock, &ts);
		}
		pthread_mutex_unlock(&p->drain_lock);
		pa_threaded_mainloop_lock(p->mainloop);
	}

	pa_threaded_mainloop_unlock(p->mainloop);
	return 0;

unlock_and_fail:
	if (o) {
		pa_operation_cancel(o);
		pa_operation_unref(o);
	}

	pa_threaded_mainloop_unlock(p->mainloop);
	return -1;
}

/* Waits until everything written has been played, for the end of an utterance.

   Waiting for the latency to reach 0, as spd_pa_simple_drain() does, may never
   end: a server may keep back data shorter than one of its cycles until more
   comes, pipewire-pulse does, so the end of a short sound like a single letter
   either waits for the next sound, or never plays. A drain request tells the
   server that nothing more is coming, so it plays everything out.

   After a drain, pipewire-pulse may have moved its read index past the end of
   the data, by up to a cycle. The next write at the write index would then land
   behind the read index, and the start of the next utterance would be skipped,
   so the next write is at the read index instead.

   A stop during the drain fades out what is left, and the drain is still waited
   for: the server stays draining until its queue runs out, and should that
   happen only after the next utterance has been written, it would skip the
   start of it. What is left to wait for is short, the server's buffer at
   most. */
static int spd_pa_simple_server_drain(spd_pa_simple *p)
{
	pa_operation *o;
	int interrupted = 0;

	pa_threaded_mainloop_lock(p->mainloop);

	if (!p->context || !PA_CONTEXT_IS_GOOD(pa_context_get_state(p->context))
	    || !p->stream
	    || !PA_STREAM_IS_GOOD(pa_stream_get_state(p->stream))) {
		pa_threaded_mainloop_unlock(p->mainloop);
		return -1;
	}

	if (!p->playing) {
		/* We got interrupted, flush.  */
		spd_pa_simple_do_flush(p, NULL);
		pa_threaded_mainloop_unlock(p->mainloop);
		return 0;
	}

	o = pa_stream_drain(p->stream, success_cb, p);
	if (!o) {
		pa_threaded_mainloop_unlock(p->mainloop);
		return -1;
	}

	while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
		if (!p->playing && !interrupted) {
			interrupted = 1;
			spd_pa_simple_do_flush(p, NULL);
		}
		pa_threaded_mainloop_wait(p->mainloop);
		if (!p->stream
		    || !PA_STREAM_IS_GOOD(pa_stream_get_state(p->stream))) {
			pa_operation_cancel(o);
			pa_operation_unref(o);
			pa_threaded_mainloop_unlock(p->mainloop);
			return -1;
		}
	}
	pa_operation_unref(o);
	p->seek_on_read = 1;
	p->unfaded = 0;
	MSG(4, "Drained\n");

	pa_threaded_mainloop_unlock(p->mainloop);
	return 0;
}

static int spd_pa_simple_flush(spd_pa_simple *p) {
	pa_threaded_mainloop_lock(p->mainloop);

	pthread_mutex_lock(&p->drain_lock);
	if (p->playing) {
		/* Still writing or draining, stop it and let it flush.  */
		p->playing = 0;
		pa_threaded_mainloop_signal(p->mainloop, 0);
		pthread_cond_signal(&p->drain_cond);
	} else {
		/* Not writing, playback is ongoing, flush it. */
		spd_pa_simple_do_flush(p, NULL);
	}
	pthread_mutex_unlock(&p->drain_lock);

	pa_threaded_mainloop_unlock(p->mainloop);
	return -1;
}

/* Now pure-spd code */

static int _pulse_open(spd_pulse_id_t * id, int sample_rate,
		       int num_channels, int bytes_per_sample)
{
	pa_buffer_attr buffAttr;
	pa_sample_spec ss;
	int error;
	char *client_name;

	ss.rate = sample_rate;
	ss.channels = num_channels;
	if (bytes_per_sample == 2) {
		switch (id->id.format) {
		case SPD_AUDIO_LE:
			ss.format = PA_SAMPLE_S16LE;
			break;
		case SPD_AUDIO_BE:
			ss.format = PA_SAMPLE_S16BE;
			break;
		}
	} else {
		ss.format = PA_SAMPLE_U8;
	}

	/* Set prebuf to one sample so that keys are spoken as soon as typed rather than delayed until the next key pressed */
	buffAttr.maxlength = (uint32_t) - 1;
	//buffAttr.tlength = (uint32_t)-1; - this is the default, which causes key echo to not work properly.
	buffAttr.tlength = id->pa_min_audio_length * sample_rate * num_channels * bytes_per_sample / 1000;
	buffAttr.prebuf = (uint32_t) - 1;
	buffAttr.minreq = (uint32_t) - 1;
	buffAttr.fragsize = (uint32_t) - 1;

	if (!id->pa_name ||
	    asprintf(&client_name, "speech-dispatcher-%s", id->pa_name) < 0)
		client_name = strdup("speech-dispatcher");

	/* Open new connection */
	if (!
	    (id->pa_simple =
	     spd_pa_simple_new(id->pa_server, client_name,
			   id->pa_device, "playback",
			   &ss, &buffAttr, &error))) {
		fprintf(stderr, __FILE__ ": pa_simple_new() failed: %s\n",
			pa_strerror(error));
		free(client_name);
		return 1;
	}
	free(client_name);
	return 0;
}

/* Close the connection to the server.  Does not free the AudioID struct. */
/* Usable in pulse_play, which closes connections on failure or */
/* changes in audio parameters. */
static void pulse_connection_close(spd_pulse_id_t * pulse_id)
{
	if (pulse_id->pa_simple != NULL) {
		spd_pa_simple_free(pulse_id->pa_simple);
		pulse_id->pa_simple = NULL;
	}
	/* Make the next pulse_begin open a new connection, e.g. after a write
	 * error when the sound server went away */
	pulse_id->pa_current_rate = -1;
	pulse_id->pa_current_bps = -1;
	pulse_id->pa_current_channels = -1;
}

static AudioID *pulse_open(void **pars)
{
	spd_pulse_id_t *pulse_id;
	int ret;

	if (pars[3] == NULL) {
		ERR("Can't open Pulse sound output, missing parameters in argument.");
		return NULL;
	}

	pulse_id = (spd_pulse_id_t *) g_malloc(sizeof(spd_pulse_id_t));

	/* Select an Endianness for the initial connection. */
#if defined(BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)
	pulse_id->id.format = SPD_AUDIO_BE;
#else
	pulse_id->id.format = SPD_AUDIO_LE;
#endif
	pulse_id->pa_simple = NULL;
	pulse_id->pa_server = NULL;
	pulse_id->pa_device = (char *)pars[3];
	pulse_id->pa_name = (char *)pars[5];
	pulse_id->pa_min_audio_length = DEFAULT_PA_MIN_AUDIO_LENGTH;

	pulse_id->pa_current_rate = -1;
	pulse_id->pa_current_bps = -1;
	pulse_id->pa_current_channels = -1;

	if (!strcmp(pulse_id->pa_device, "default")) {
		pulse_id->pa_device = NULL;
	}

	if (pars[4] != NULL && atoi(pars[4]) != 0)
		pulse_id->pa_min_audio_length = atoi(pars[4]);

	ret = _pulse_open(pulse_id, DEF_RATE, DEF_CHANNELS, DEF_BYTES_PER_SAMPLE);
	if (ret) {
		g_free(pulse_id);
		pulse_id = NULL;
	}

	return (AudioID *) pulse_id;
}

/* Configure pulse playback for the given configuration of track
   But do not play anything yet */
static int pulse_begin(AudioID * id, AudioTrack track)
{
	int bytes_per_sample;
	int error;
	spd_pulse_id_t *pulse_id = (spd_pulse_id_t *) id;

	if (id == NULL) {
		return -1;
	}
	if (track.samples == NULL || track.num_samples <= 0) {
		return 0;
	}
	MSG(4, "Starting playback\n");
	/* Choose the correct format */
	if (track.bits == 16) {
		bytes_per_sample = 2;
	} else if (track.bits == 8) {
		bytes_per_sample = 1;
	} else {
		ERR("ERROR: Unsupported sound data format, track.bits = %d\n",
		    track.bits);
		return -1;
	}

	/* Check if the current connection has suitable parameters for this track */
	if (pulse_id->pa_current_rate != track.sample_rate
	    || pulse_id->pa_current_bps != track.bits
	    || pulse_id->pa_current_channels != track.num_channels) {
		MSG(4, "Reopening connection due to change in track parameters sample_rate:%d bps:%d channels:%d\n", track.sample_rate, track.bits, track.num_channels);
		/* Close old connection if any */
		pulse_connection_close(pulse_id);
		/* Open a new connection */
		error = _pulse_open(pulse_id, track.sample_rate, track.num_channels,
			    bytes_per_sample);
		if (error) {
			pulse_id->pa_current_rate = -1;
			pulse_id->pa_current_bps = -1;
			pulse_id->pa_current_channels = -1;
			return -1;
		}
		/* Keep track of current connection parameters */
		pulse_id->pa_current_rate = track.sample_rate;
		pulse_id->pa_current_bps = track.bits;
		pulse_id->pa_current_channels = track.num_channels;
	}

	return 0;
}

static int pulse_feed(AudioID * id, AudioTrack track)
{
	int num_bytes;
	int bytes_per_sample;
	int error;
	spd_pulse_id_t *pulse_id = (spd_pulse_id_t *) id;

	if (track.bits == 16) {
		bytes_per_sample = 2;
	} else if (track.bits == 8) {
		bytes_per_sample = 1;
	} else {
		ERR("ERROR: Unsupported sound data format, track.bits = %d\n",
		    track.bits);
		return -1;
	}
	num_bytes = track.num_samples * bytes_per_sample;

	if (pulse_id->pa_simple == NULL) {
		MSG(4, "ERROR: Audio: pulse_feed(): no connection to the sound server\n");
		return -1;
	}

	MSG(4, "bytes to play: %d, (%f secs)\n", num_bytes,
	    (((float)(num_bytes) / 2) / (float)track.sample_rate));

	if (spd_pa_simple_write
	    (pulse_id->pa_simple, track.samples, num_bytes,
	     &error) < 0) {
		spd_pa_simple_flush(pulse_id->pa_simple);
		pulse_connection_close(pulse_id);
		MSG(4, "ERROR: Audio: pulse_play(): %s - closing device - re-open it in next run\n", pa_strerror(error));
		return -1;
	}

	/* TODO: pa_stream_get_time() or pa_stream_get_latency(). The former
	will return the current playback time of the hardware since the stream
	has been started. */

	return 0;
}

static int pulse_feed_sync(AudioID * id, AudioTrack track)
{
	spd_pulse_id_t *pulse_id = (spd_pulse_id_t *) id;
	int ret;

	ret = pulse_feed(id, track);
	if (ret)
		return ret;

	return spd_pa_simple_server_drain(pulse_id->pa_simple);
}

static int pulse_feed_sync_overlap(AudioID * id, AudioTrack track)
{
	spd_pulse_id_t *pulse_id = (spd_pulse_id_t *) id;
	int ret;

	ret = pulse_feed(id, track);
	if (ret)
		return ret;

	/* We typically want 20ms overlap: usually about a period size, and
	   small enough to be unnoticeable */
	return spd_pa_simple_drain(pulse_id->pa_simple, 20000, NULL);
}

static int pulse_end(AudioID * id)
{
	spd_pulse_id_t *pulse_id = (spd_pulse_id_t *) id;
	if (pulse_id->pa_simple != NULL)
		spd_pa_simple_server_drain(pulse_id->pa_simple);
	return 0;
}

static int pulse_play(AudioID * id, AudioTrack track)
{
	int ret;

	ret = pulse_begin(id, track);
	if (ret)
		return ret;

	ret = pulse_feed_sync(id, track);
	if (ret)
		return ret;

	return pulse_end(id);
}

/* stop the pulse_play() loop */
static int pulse_stop(AudioID * id)
{
	spd_pulse_id_t *pulse_id = (spd_pulse_id_t *) id;

	if (pulse_id->pa_simple != NULL)
		spd_pa_simple_flush(pulse_id->pa_simple);

	return 0;
}

static int pulse_close(AudioID * id)
{
	spd_pulse_id_t *pulse_id = (spd_pulse_id_t *) id;
	pulse_connection_close(pulse_id);
	g_free(pulse_id);
	id = NULL;

	return 0;
}

static int pulse_set_volume(AudioID * id, int volume)
{
	/* TODO */
	return 0;
}

static void pulse_set_loglevel(int level)
{
	if (level) {
		pulse_log_level = level;
	}
}

static char const *pulse_get_playcmd(void)
{
	return pulse_play_cmd;
}

/* Provide the pulse backend. */
static spd_audio_plugin_t pulse_functions = {
	"pulse",
	pulse_open,
	pulse_play,
	pulse_stop,
	pulse_close,
	pulse_set_volume,
	pulse_set_loglevel,
	pulse_get_playcmd,
	pulse_begin,
	pulse_feed_sync,
	pulse_feed_sync_overlap,
	pulse_end,
};

spd_audio_plugin_t *pulse_plugin_get(void)
{
	return &pulse_functions;
}

spd_audio_plugin_t *
    __attribute__ ((weak))
    SPD_AUDIO_PLUGIN_ENTRY(void)
{
	return &pulse_functions;
}

#undef MSG
#undef ERR
