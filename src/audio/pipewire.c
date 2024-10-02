/*
 * a pipewire backend for Speech Dispatcher
 *
 * Copyright (C) 2024 Alberto Tirla <albertotirla@gmail.com>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include <pipewire/pipewire.h>
#include <pipewire/thread-loop.h>

#include <spa/param/audio/format-utils.h>
#include <spa/utils/ringbuffer.h>
#include <spa/param/props.h>

#include <spd_audio_plugin.h>
#include "../common/common.h"
#include <spa/utils/defs.h>

#include <pthread.h>
#include <pipewire/loop.h>
#include <spa/support/system.h>

// for the buffer size which will be allocated on the heap, to store the samples produced by speech dispatcher and read by pipewire
//  this buffer will be used as backing storage by a ring buffer, which will also insure that the threads are syncronised when reading from and writing to the storage area
#define SAMPLE_BUFFER_SIZE (16 * 1024) // taken from the pipewire ringbuffer example

// speech dispatcher backend entry point, defined in multiple configurations
#ifdef USE_DLOPEN
#define SPD_AUDIO_PLUGIN_ENTRY spd_audio_plugin_get
#else
#define SPD_AUDIO_PLUGIN_ENTRY spd_pipewire_LTX_spd_audio_plugin_get
#endif

// state of the backend, where all components are gathered. This will be allocated on the heap
typedef struct
{
    AudioID id;                  // to comply with the speech dispatcher contract, make the first field of the struct the type expected by the spd callbacks, so that casting from it isn't entirely undefined behaviour
    struct pw_thread_loop *loop; // a pipewire loop object which can be used without blocking the main thread, in this case the thread of speech dispatcher
    struct pw_stream *stream;    // this represents an instance of this module, a node of the media graph along with other metadata, which will be linked to the default output device
    struct spa_ringbuffer rb;    // a thread-safe ring buffer implementation which uses atomic operations  to guarantee some semblance of thread safety, therefore it doesn't require a mutex
    char *sample_buffer;         // the heap storage memory which will be backing the atomic ring buffer implementation, it's on the heap because this struct would be transfered across callback functions a lot, and such a large buffer could cause a stack overflow on some hardware architectures
    uint32_t stride;             // the amount of bytes per frame. This is in state because it's computed dynamically, according to each format specifyer
    int32_t eventfd_number;      // used in on_process to signal the thread where pipewire_play is running that the ringbuffer has been drained to the point where playback either finished or is in progress, but for sure to the point where we could push more audio, because our side of the buffer at least is drained
} module_state;

// pipewire on process callback
// this function would be called each time the server requests data from us
static void on_process(void *userdata)
{
    module_state *state = (module_state *)userdata;
    // a lot of the code below has been taken from the playing a tone pipewire tutorial, see the pipewire documentation for more details

    struct pw_buffer *b;
    struct spa_buffer *buf;
    void *destination_memory; // where we end up writing to, after reading from the ringbuffer
    uint32_t read_index, available_for_loading, must_load_with_silence;
    int32_t available_bytes, number_of_bytes;
    if ((b = pw_stream_dequeue_buffer(state->stream)) == NULL)
    {
        pw_log_warn("out of buffers: %m");
        return;
    }
    buf = b->buffer;
    // pipewire's buffers are mapped directly from the server to the client
    // As such, if the memory we're given doesn't exist, we have to abort imediately, because doing otherwise most certainly would lead to undefined behaviour
    if ((destination_memory = buf->datas[0].data) == NULL)
        return;
    // in the process callback, we take the samples from the ring buffer we used in the pipewire_play function and send it to pipewire for playback
    // this will be done using the following two step algorythm:
    // we first get the read index of the ringbuffer, which also returns the amount available in there
    // if we have enough data in the ringbuffer, aka if the read index isn't less than requested, then we load all of it at once in pipewire
    // however, if the remainder between the filled part of the buffer and the value of requested is greatter than 0, then we fill that part of the buffer with silence, while taking everything else, hoping we will have more samples next time
    available_bytes = spa_ringbuffer_get_read_index(&state->rb, &read_index);
    number_of_bytes = buf->datas[0].maxsize; // how many samples, in bytes,  of audio are in this chunk. Technically, requested itself could be used, but this is done instead to avoid some very rare, but important, edge cases, for example buggy hardware drivers which don't probe the card correctly, so on_process gets called very often, with a value of requested samples greatter than what pipewire could allocate, in maxsize
#if PW_CHECK_VERSION(0, 3, 49)
    if (b->requested > 0)
    {
        number_of_bytes = SPA_MIN(number_of_bytes, b->requested * state->stride); // because this value is given in frames/samples, and we want bytes
    }
#endif

    // if there's something to be read from the ring buffer at this time, we do so now
    if (available_bytes > 0)
    {
        available_for_loading = SPA_MIN(available_bytes, number_of_bytes);
    }
    else
    {
        available_for_loading = 0;
    }
    // if there's anything else to fill and the ring buffer doesn't contain enough at this point, fill the difference with silence
    must_load_with_silence = number_of_bytes - available_for_loading;
    if (available_for_loading > 0)
    {
        spa_ringbuffer_read_data(&state->rb, state->sample_buffer, SAMPLE_BUFFER_SIZE, read_index % SAMPLE_BUFFER_SIZE, destination_memory, available_for_loading);
        spa_ringbuffer_read_update(&state->rb, read_index + available_for_loading);
    }
// if the required pipewire version doesn't match for requested to be available, we may have had plenty more time to buffer some more data, and instead we fill the whole buffer, while reading everything from the ringbuffer prematurely
#if PW_CHECK_VERSION(0, 3, 49)
    if (b->requested > 0)
    {
        if (must_load_with_silence > 0)
        {
            memset(destination_memory + available_for_loading, 0, must_load_with_silence);
        }
    }
#endif
    // now, we should have most of the data ready, we fill in some metadata and push the buffer object to pipewire
    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->size = number_of_bytes;
    buf->datas[0].chunk->stride = state->stride;
    pw_stream_queue_buffer(state->stream, b);
    // signal to the main thread that data can be written again, make sure it's woken only once
    spa_system_eventfd_write(pw_thread_loop_get_loop(state->loop)->system, state->eventfd_number, 1);
}
// pipewire internal: structure describing what kind of events we subscribe to
// For now, this is only on_process
static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
};

//  open a pipewire connection and initialize a non-blocking main loop
//  pars[1] contains the name of the sink
static AudioID *pipewire_open(void **pars)
{
    module_state *state = (module_state *)malloc(sizeof(module_state));
    pw_init(0, NULL);
    state->loop = pw_thread_loop_new("pipewire audio thread", NULL);
    state->rb = SPA_RINGBUFFER_INIT();
    state->sample_buffer = malloc(SAMPLE_BUFFER_SIZE);
    // initialise the event file descripter and the stream
    pw_thread_loop_lock(state->loop);
    state->stream = pw_stream_new_simple(pw_thread_loop_get_loop(state->loop), pars[1], pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_ROLE, "Accessibility", NULL), &stream_events, state);
    if ((state->eventfd_number = spa_system_eventfd_create(pw_thread_loop_get_loop(state->loop)->system, SPA_FD_CLOEXEC)) < 0)
        return NULL;
    pw_thread_loop_unlock(state->loop);
    // start the threaded loop
    pw_thread_loop_start(state->loop);
    return (AudioID *)state;
}
// configure a pipewire stream for the given speech dispatcher configuration and then prepair loop for playback
static int pipewire_begin(AudioID *id, AudioTrack track)
{
    module_state *state = (module_state *)id;
    const struct spa_pod *params;
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    int format;
    if (track.bits == 16)
    {
        state->stride = track.num_channels * sizeof(uint16_t);
        switch (state->id.format)
        {
        case SPD_AUDIO_LE:
            format = SPA_AUDIO_FORMAT_S16_LE;
            break;
        case SPD_AUDIO_BE:
            format = SPA_AUDIO_FORMAT_S16_BE;
            break;
        default:
            pw_log_error("invalid audio format specifier");
            return -1;
        }
    }
    else if (track.bits == 8)
    {
        state->stride = track.num_channels * sizeof(uint8_t);
        format = SPA_AUDIO_FORMAT_S8;
    }
    else
    {
        pw_log_error("invalid audio format specifier");
        return -1;
    }

    params = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &SPA_AUDIO_INFO_RAW_INIT(.format = format, .channels = track.num_channels, .rate = track.sample_rate));
    pw_thread_loop_lock(state->loop);
    pw_stream_connect(state->stream, PW_DIRECTION_OUTPUT, PW_ID_ANY, PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS, &params, 1);
    pw_thread_loop_unlock(state->loop);
    return 0;
}
static int pipewire_feed_sink_overlap(AudioID *id, AudioTrack track)
{
    module_state *state = (module_state *)id;
    uint32_t write_index, fill_quantity, room_left_in_buffer = 0, track_buffer_size;
    uint64_t dummy; // used to read from the event file descripter. We don't use this for anything, at the moment.
    uint8_t *samples_buffer_copy = (uint8_t *)track.samples;
    // if the stream has been deactivated because of a pipewire_stop call, we activate it
    pw_thread_loop_lock(state->loop);
    if (pw_stream_get_state(state->stream, NULL) == PW_STREAM_STATE_PAUSED)
    {
        pw_stream_set_active(state->stream, true);
    }
    pw_thread_loop_unlock(state->loop);
    track_buffer_size = track.num_samples * state->stride; // we want bytes, not frames
    // as long as we have samples in the buffer sent by speech dispatcher, we loop, blocking spd from calling us till the audio system played what we loaded in it
    while (track_buffer_size > 0)
    {
        // drainage spinlock!
        // we wait till fill_quantity is not empty after this thread can continue, aka after on_process updated that ringbuffer index with what it managed to play, so that we have some room in the buffer to fill from what's remaining of what was given to us by speech dispatcher
        while (true)
        {
            fill_quantity = spa_ringbuffer_get_write_index(&state->rb, &write_index);
            // see if there's some room in the buffer, if so, stop locking
            room_left_in_buffer = SAMPLE_BUFFER_SIZE - fill_quantity;
            if (room_left_in_buffer > 0)
                // then we have room in the buffer, so stop locking our thread
                break;
            // otherwise, to not starve a cpu core of resources, we suspend our spinning until something, in this case on_process, wakes us up. In an embedded system without these primitives, spinning endlessly like that would be the only option. We could do the same here, but for efficiency reasons alone, we don't
            pw_thread_loop_lock(state->loop);
            struct pw_loop *program_loop = pw_thread_loop_get_loop(state->loop);
            pw_thread_loop_unlock(state->loop);
            spa_system_eventfd_read(program_loop->system, state->eventfd_number, &dummy);
        }
        // we write the amount of samples we can at this time without overflowing the buffer, to the memory area represented by the amount of room that's free after the last on_process call, to then be enqueued by pipewire
        //  we assume speech dispatcher gives us the correct number of bytes for the format it chose, enough for this chunk. If that's not the case, there's not much we could do besides reading uninitialized memory or an incomplete chunk, unfortunately
        // in case we produced more audio than the available room in the buffer that got freed by on_process, we only push what we can, otherwise we would read from uninitialised memory
        room_left_in_buffer = SPA_MIN(track_buffer_size, room_left_in_buffer);
        // everything has been accounted for, so write the first batch of data, keeping into account that we can't overflow the initially allocated block for the buffer, represented by SAMPLE_BUFFER_SIZE
        spa_ringbuffer_write_data(&state->rb, state->sample_buffer, SAMPLE_BUFFER_SIZE, write_index % SAMPLE_BUFFER_SIZE, samples_buffer_copy, room_left_in_buffer);
        // subtract from the total buffer what we were already able to write, in order to indicate that we played some of the initial capacity of the buffer by queueing it to on_process
        track_buffer_size -= room_left_in_buffer;

        // now, we update the write index to account for the chunk of samples we just wrote
        spa_ringbuffer_write_update(&state->rb, write_index + room_left_in_buffer);
        // advance our pointer by how much we were able to free by playing it
        samples_buffer_copy += room_left_in_buffer;
    }
    return 0;
}
static int pipewire_stop(AudioID *id)
{
    module_state *state = (module_state *)id;
    // I don't know how to pause or stop a stream without setting properties on it, so that won't be used in the first version of this. So, for now, setting a stream as inactive will suspend it from being called at all, similar to pausing it
    //  this function returns error codes directly, so I use this as the return value
    pw_thread_loop_lock(state->loop);
    pw_stream_set_active(state->stream, false);
    pw_thread_loop_unlock(state->loop);
    return 0;
}
static int pipewire_set_volume(AudioID *id, int volume)
{
    module_state *state = (module_state *)id;
    // because volume is given to us in the range -100, 100 and pipewire requests a floating point number between 0 and 1, we have to normalize it first
    int min_value = -100;
    int max_value = 100;
    float normalized = (float)(volume - min_value) / (max_value - min_value);
    // use that normalized value to set the volume
    pw_thread_loop_lock(state->loop);
    pw_stream_set_control(state->stream, SPA_PROP_volume, 1, &normalized, 0);
    pw_thread_loop_unlock(state->loop);
    return 0;
}
// this function is responsible for cleanning up all the memory used by all the objects collected by the module_state structure
// if any memory leaks appear, this is the place to look for, or if this is never called, why it's never called is yet another mystery which should be resolved
static int pipewire_close(AudioID *id)
{
    module_state *state = (module_state *)id;
    // stop the thread loop first
    pw_thread_loop_stop(state->loop);
    pw_thread_loop_destroy(state->loop);
    // unlink and disconnect the stream
    pw_stream_disconnect(state->stream);
    pw_stream_destroy(state->stream);
    // free the memory allocated by  the sample buffer used to hold samples between pipewire and speech dispatcher
    free(state->sample_buffer);
    // uninitialize pipewire
    pw_deinit();
    // free the module state structure itself, since it was allocated on the heap at the beginning of pipewire_open
    free(state);
    // if there are any more memory leaks past this point, then I forgot to free something, and I have no idea what. If you recently made memory allocation changes in this module, or if you added another heap allocated value, make sure you free it here, in the appropriate order, aka don't free the state structure before you free all of its members
    return 0;
}
// set the play command for the pipewire backend
static const char *pipewire_get_play_command()
{
    return "pw-play -";
}
static void pipewire_set_log_level(int level)
{
    // I don't care about this
    return;
}
static spd_audio_plugin_t pipewire_exports = {
    .name = "pipewire",
    .open = pipewire_open,
    .begin = pipewire_begin,
    .close = pipewire_close,
    .stop = pipewire_stop,
    .feed_sync_overlap = pipewire_feed_sink_overlap,
    .get_playcmd = pipewire_get_play_command,
    .set_volume = pipewire_set_volume,
    .set_loglevel = pipewire_set_log_level};
spd_audio_plugin_t *pipewire_plugin_get(void)
{
    return &pipewire_exports;
}

spd_audio_plugin_t *__attribute__((weak))
SPD_AUDIO_PLUGIN_ENTRY(void)
{
    return &pipewire_exports;
}
