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

// for properly sending error messages to speech dispatcher's logs
static int32_t pipewire_log_level; // only one per module, accessing this from multiple threads is unsafe, etc etc etc
// the message and error macros are used for the actual logging part
#define message(level, arg, ...)                 \
    if (level <= pipewire_log_level)             \
    {                                            \
        MSG(0, "pipewire: " arg, ##__VA_ARGS__); \
    }
#define error(arg, ...) message(0, "pipewire: fatal: " arg, ##__VA_ARGS__)

// state of the backend, where all components are gathered. This will be allocated on the heap
typedef struct
{
    AudioID id;                    // to comply with the speech dispatcher contract, make the first field of the struct the type expected by the spd callbacks, so that casting from it isn't entirely undefined behaviour
    bool is_running;               // a flag to tell the audio thread that we got stopped by pipewire_stop or similar, and as such the thread should unlock
    struct pw_thread_loop *loop;   // a pipewire loop object which can be used without blocking the main thread, in this case the thread of speech dispatcher
    struct pw_stream *stream;      // this represents an instance of this module, a node of the media graph along with other metadata, which will be linked to the default output device
    struct pw_loop *inner_loop;    // to be able to use pipewire functions without taking the lock too many times, potentially causing an xrun
    struct spa_ringbuffer rb;      // a thread-safe ring buffer implementation which uses atomic operations  to guarantee some semblance of thread safety, therefore it doesn't require a mutex
    char *sample_buffer;           // the heap storage memory which will be backing the atomic ring buffer implementation, it's on the heap because this struct would be transfered across callback functions a lot, and such a large buffer could cause a stack overflow on some hardware architectures
    uint32_t stride;               // the amount of bytes per frame. This is in state because it's computed dynamically, according to each format specifyer
    uint32_t playback_sample_rate; // store this in here so that we can detect when spd changes the sample rate because of another module
    int32_t eventfd_number;        // used in on_process to signal the thread where pipewire_play is running that the ringbuffer has been drained to the point where playback either finished or is in progress, but for sure to the point where we could push more audio, because our side of the buffer at least is drained
} module_state;

// pipewire on process callback
// this function would be called each time the server requests data from usa
static void on_process(void *userdata)
{
    module_state *state = (module_state *)userdata;
    // a lot of the code below has been taken from the playing a tone pipewire tutorial, see the pipewire documentation for more details

    struct pw_buffer *b;
    struct spa_buffer *buf;
    void *destination_memory; // where we end up writing to, after reading from the ringbuffer
    uint32_t read_index, available_for_loading, must_load_with_silence;
    int32_t available_bytes_in_speech_dispatcher_buffer, total_number_of_bytes_in_pipewire_buffer;
    message(5, "entering on process callback");
    if ((b = pw_stream_dequeue_buffer(state->stream)) == NULL)
    {
        error("out of buffers. Aborting");
        abort();
    }
    buf = b->buffer;
    // pipewire's buffers are mapped directly from the server to the client
    // As such, if the memory we're given doesn't exist, we have to abort imediately, because doing otherwise most certainly would lead to undefined behaviour
    if ((destination_memory = buf->datas[0].data) == NULL)
    {
        error("out of shared memory blocks. Aborting");
        abort();
    }
    // in the process callback, we take the samples from the ring buffer we used in the pipewire_play function and send it to pipewire for playback
    // this will be done using the following two step algorythm:
    // we first get the read index of the ringbuffer, which also returns the amount available in there
    // if we have enough data in the ringbuffer, aka if the read index isn't less than requested, then we load all of it at once in pipewire
    // however, if the remainder between the filled part of the buffer and the value of requested is greatter than 0, then we fill that part of the buffer with silence, while taking everything else, hoping we will have more samples next time
    available_bytes_in_speech_dispatcher_buffer = spa_ringbuffer_get_read_index(&state->rb, &read_index);
    message(4, "we have %i bytes in the spd ringbuffer", available_bytes_in_speech_dispatcher_buffer);
    total_number_of_bytes_in_pipewire_buffer = buf->datas[0].maxsize; // how many samples, in bytes,  of audio are in this chunk. Technically, requested itself could be used, but this is done instead to avoid some very rare, but important, edge cases, for example buggy hardware drivers which don't probe the card correctly, so on_process gets called very often, with a value of requested samples greatter than what pipewire could allocate, in maxsize
// if our pipewire is new enough, we know exactly how many samples to push, given through the value of the requested field, from the structure returned when we dequeue a buffer for filling
#if PW_CHECK_VERSION(0, 3, 49)
    if (b->requested > 0)
    {
        total_number_of_bytes_in_pipewire_buffer = SPA_MIN(total_number_of_bytes_in_pipewire_buffer, b->requested * state->stride); // because this value is given in frames/samples, and we want bytes
        // if there's anything else to fill and the ring buffer doesn't contain enough at this point, fill the difference with silence
        must_load_with_silence = total_number_of_bytes_in_pipewire_buffer - available_for_loading;
    }
// otherwise however, we have no idea how much we should push, so we push as much as we have in the ring buffer, making sure we don't push more than maxsize. If that ends up being less than what would have been requested if we had access to that, we have no choice but to underrun
#else
    total_number_of_bytes_in_pipewire_buffer = SPA_MIN(available_bytes_in_speech_dispatcher_buffer, total_number_of_bytes_in_pipewire_buffer);
#endif
    message(4, "we should fill %i bytes in the pipewire provided buffer", total_number_of_bytes_in_pipewire_buffer)
        // if there's something to be read from the ring buffer at this time, we do so now
        if (available_bytes_in_speech_dispatcher_buffer > 0)
    {
        available_for_loading = SPA_MIN(available_bytes_in_speech_dispatcher_buffer, total_number_of_bytes_in_pipewire_buffer);
        message(4, "we can load %i bytes for now", available_for_loading);
    }
    else
    {
        available_for_loading = 0;
        message(4, "nothing to be loaded");
    }
    if (available_for_loading > 0)
    {
        message(4, "copying data from spd ringbuffer to pipewire");
        spa_ringbuffer_read_data(&state->rb, state->sample_buffer, SAMPLE_BUFFER_SIZE, read_index % SAMPLE_BUFFER_SIZE, destination_memory, available_for_loading);
        spa_ringbuffer_read_update(&state->rb, read_index + available_for_loading);
    }
// if the required pipewire version doesn't match for requested to be available, we may have had plenty more time to buffer some more data, and instead we fill the whole buffer, while reading everything from the ringbuffer prematurely
#if PW_CHECK_VERSION(0, 3, 49)
    if (b->requested > 0)
    {
        if (must_load_with_silence > 0)
        {
            message(4, "not enough bytes to load from spd buffer, must fill %i bytes with silence", must_load_with_silence);
            memset(destination_memory + available_for_loading, 0, must_load_with_silence);
        }
    }
#endif
    // now, we should have most of the data ready, we fill in some metadata and push the buffer object to pipewire
    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->size = total_number_of_bytes_in_pipewire_buffer;
    buf->datas[0].chunk->stride = state->stride;
    pw_stream_queue_buffer(state->stream, b);
    message(4, "buffer enqueued");
    // signal to the main thread that data can be written again, make sure it's woken only once
    message(4, "waking up audio thread");
    spa_system_eventfd_write(state->inner_loop->system, state->eventfd_number, 1);
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
    message(3, "initialising pipewire output");
    pw_init(0, NULL);
    // initialise the main loop, and then get the inner loop from it, so that locks are kepped from the most often contended path as much as possible
    state->loop = pw_thread_loop_new("pipewire audio thread", NULL);
    state->inner_loop = pw_thread_loop_get_loop(state->loop);
    // the ring buffer and its backing memory
    state->rb = SPA_RINGBUFFER_INIT();
    state->sample_buffer = malloc(SAMPLE_BUFFER_SIZE);
    // initialise the event file descripter and the stream
    state->stream = pw_stream_new_simple(state->inner_loop, pars[1], pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_ROLE, "Accessibility", NULL), &stream_events, state);
    if ((state->eventfd_number = spa_system_eventfd_create(state->inner_loop->system, SPA_FD_CLOEXEC)) < 0)
    {
        error("can not create event file descriptor");
        abort();
    }
    // set our flag to true
    state->is_running = true;
    // set sample rate to 0, to then be able to check if this is the first time pipewire_begin has been called. We explicitly set this to 0 as a sentinel value of sorts, because we can't rely on uninitialised memory, as that's unpredictable
    state->playback_sample_rate = 0;
    message(3, "basic soundsystem initialisation completed without errors");
    //  start the threaded loop
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
    message(3, "entering per-module configuration procedure");
    if (track.bits == 16)
    {
        state->stride = track.num_channels * sizeof(uint16_t);
        switch (state->id.format)
        {
        case SPD_AUDIO_LE:
            format = SPA_AUDIO_FORMAT_S16_LE;
            message(3, "audio format is signed 16 bit, little endian");
            break;
        case SPD_AUDIO_BE:
            format = SPA_AUDIO_FORMAT_S16_BE;
            message(3, "audio format is signed 16 bit, big endian");
            break;
        default:
            error("invalid audio format specifier");
            return -1;
        }
    }
    else if (track.bits == 8)
    {
        state->stride = track.num_channels * sizeof(uint8_t);
        format = SPA_AUDIO_FORMAT_S8;
        message(3, "audio format is signed 8bit");
    }
    else
    {
        error("invalid audio format specifier");
        return -1;
    }
    message(4, "current stride is: %i", state->stride);
    message(3, "the sample rate provided by the module is: %i", track.sample_rate);
    // is this the first time this function is called? then our sentinel value must still be there
    if (state->playback_sample_rate == 0)
    {
        state->playback_sample_rate = track.sample_rate;
    }
    // if we went here before and we got a new rate from spd, aka a new module with a different sample rate connected
    else if (state->playback_sample_rate != 0 && state->playback_sample_rate != track.sample_rate)
    {
        // then we disconnect the stream, letting the rest of the function reconnect it with the new values, since format was computed before
        message(4, "backend initialised with different sample rate, another module connecting?");
        message(4, "disconnecting the old stream now");
        pw_stream_disconnect(state->stream);
        // but don't forget to update the value of sample rate we cache, for next time
        state->playback_sample_rate = track.sample_rate;
    }
    // otherwise, we went here before, but we got an identical sample rate, so this must be a new utterance begun by the same module. In that case, log that and do nothing more
    else
    {
        message(4, "new utterance from the same configuration, no action taken");
        // nothing to do here, so get out
        return 0;
    }
    params = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &SPA_AUDIO_INFO_RAW_INIT(.format = format, .channels = track.num_channels, .rate = state->playback_sample_rate));
    message(4, "connecting pipewire stream");
    pw_thread_loop_lock(state->loop);
    pw_stream_connect(state->stream, PW_DIRECTION_OUTPUT, PW_ID_ANY, PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS, &params, 1);
    pw_thread_loop_unlock(state->loop);
    message(4, "stream connecting successful");
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
        if (state->is_running == false)
        {
            message(4, "stream is paused, starting");
            state->is_running = true;
            pw_stream_set_active(state->stream, true);
        }
    }
    pw_thread_loop_unlock(state->loop);
    track_buffer_size = track.num_samples * state->stride; // we want bytes, not frames
    // as long as we have samples in the buffer sent by speech dispatcher, we loop, blocking spd from calling us till the audio system played what we loaded in it
    while (track_buffer_size > 0)
    {
        message(5, "still have samples in the spd buffer");
        // drainage spinlock!
        // we wait till fill_quantity is not empty after this thread can continue, aka after on_process updated that ringbuffer index with what it managed to play, so that we have some room in the buffer to fill from what's remaining of what was given to us by speech dispatcher
        while (true)
        {
            // we generally spend a lot of time waiting for this to happen, enough so that there can be something interrupting us, for example, speech dispatcher deciding that another piece of audio must have priority, while we're blocked in here. So, we check, each iteration of this loop, if  we were stopped by something, upon which we imediately return
            if (state->is_running == false)
                return 0;
            // how much of the buffer was actually filled and not played yet by on_process
            fill_quantity = spa_ringbuffer_get_write_index(&state->rb, &write_index);
            // see if there's some room in the buffer, if so, stop locking
            room_left_in_buffer = SAMPLE_BUFFER_SIZE - fill_quantity;
            if (room_left_in_buffer > 0)
            {
                // then we have room in the buffer, so stop locking our thread
                message(4, "we have %i bytes of free room in buffer, stop locking", room_left_in_buffer);
                break;
            }
            // otherwise, to not starve a cpu core of resources, we suspend our spinning until something, in this case on_process, wakes us up
            message(4, "no space, waiting for wakeup");
            spa_system_eventfd_read(state->inner_loop->system, state->eventfd_number, &dummy);
        }
        // we write the amount of samples we can at this time without overflowing the buffer, to the memory area represented by the amount of room that's free after the last on_process call, to then be enqueued by pipewire
        //  we assume speech dispatcher gives us the correct number of bytes for the format it chose, enough for this chunk. If that's not the case, there's not much we could do besides reading uninitialized memory or an incomplete chunk, unfortunately
        // in case we produced more audio than the available room in the buffer that got freed by on_process, we only push what we can, otherwise we would scramble existing audio samples
        room_left_in_buffer = SPA_MIN(track_buffer_size, room_left_in_buffer);
        // everything has been accounted for, so write the first batch of data, keeping into account that we can't overflow the initially allocated block for the buffer, represented by SAMPLE_BUFFER_SIZE
        message(4, "writing %i bytes to speech dispatcher buffer", room_left_in_buffer);
        spa_ringbuffer_write_data(&state->rb, state->sample_buffer, SAMPLE_BUFFER_SIZE, write_index % SAMPLE_BUFFER_SIZE, samples_buffer_copy, room_left_in_buffer);
        // subtract from the total buffer what we were already able to write, in order to indicate that we played some of the initial capacity of the buffer by queueing it to on_process
        track_buffer_size -= room_left_in_buffer;
        // now, we update the write index to account for the chunk of samples we just wrote
        spa_ringbuffer_write_update(&state->rb, write_index + room_left_in_buffer);
        // advance our pointer by how much we were able to free by playing it
        samples_buffer_copy += room_left_in_buffer;
        message(4, "writing completed without errors");
    }
    message(4, "buffer drained from spd side, nothing else to do");
    return 0;
}
static int pipewire_stop(AudioID *id)
{
    module_state *state = (module_state *)id;
    message(3, "stopping audio playback");
    // unset the running flag
    state->is_running = false;
    // drain the stream, to make sure there are no more samples playing, to avoid crackling
    pw_stream_flush(state->stream, false);
    // from here too, we must wake the audio thread, so that the locking loop can be aware of our flag change and terminate as a consequnce
    message(4, "waking up audio thread");
    spa_system_eventfd_write(state->inner_loop->system, state->eventfd_number, 1);
    // signal our intentions on the pipewire side
    pw_thread_loop_lock(state->loop);
    pw_stream_set_active(state->stream, false);
    pw_thread_loop_unlock(state->loop);
    // empty the ringbuffer, to make sure we don't play garbage next time, or that we play unnecesary silence
    state->rb = SPA_RINGBUFFER_INIT();
    // all done, quit
    message(4, "all done");
    return 0;
}
static int pipewire_set_volume(AudioID *id, int volume)
{
    module_state *state = (module_state *)id;
    message(3, "setting audio stream volume to %i", volume)
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
// if any memory leaks appear, this isa the place to look for, or if this is never called, why it's never called is yet another mystery which should be resolved
static int pipewire_close(AudioID *id)
{
    module_state *state = (module_state *)id;
    message(3, "cleanning up resources");
    // unlink and disconnect the stream
    // but make sure the loop doesn't continue to call on_process, so we lock the loop while doing this
    pw_thread_loop_lock(state->loop);
    pw_stream_disconnect(state->stream);
    pw_stream_destroy(state->stream);
    pw_thread_loop_unlock(state->loop);
    // now that the stream is destroyed, we can kill the loop
    pw_thread_loop_stop(state->loop);
    pw_thread_loop_destroy(state->loop);
    // free the memory allocated by  the sample buffer used to hold samples between pipewire and speech dispatcher
    free(state->sample_buffer);
    // uninitialize pipewire
    pw_deinit();
    // free the module state structure itself, since it was allocated on the heap at the beginning of pipewire_open
    free(state);
    message(3, "all resources are cleanned up");
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
    if (level != 0)
    {
        pipewire_log_level = level;
    }
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
