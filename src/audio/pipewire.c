#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include <pipewire/pipewire.h>
#include <pipewire/thread-loop.h>

#include <spa/param/audio/format-utils.h>
#include <spa/utils/ringbuffer.h>

#include <spd_audio_plugin.h>
#include "../common/common.h"

// for the buffer size which will be allocated on the heap, to store the samples produced by speech dispatcher and read by pipewire
//  this buffer will be used as backing storage by a ring buffer, which will also insure that the threads are syncronised when reading from and writing to the storage area
#define SAMPLE_BUFFER_SIZE 16*1024 //taken from the pipewire ringbuffer example

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
    char *sink_name;             // the name of the sink, as given by speech dispatcher
} module_state;

// pipewire on process callback
// this function would be called each time the server requests data from us
// for now, it playes silence
static void on_process(void *userdata)
{
    module_state *state = (module_state *)userdata;
    // a lot of the code below has been taken from the playing a tone pipewire tutorial, see the pipewire documentation for more details

    struct pw_buffer *b;
    struct spa_buffer *buf;
    uint16_t *dst;
    if ((b = pw_stream_dequeue_buffer(state->stream)) == NULL)
    {
        pw_log_warn("out of buffers: %m");
        return;
    }

    buf = b->buffer;
    // pipewire's buffers are mapped directly from the server to the client
    // As such, if the memory we're given doesn't exist, we have to abort imediately, because doing otherwise most certainly would lead to undefined behaviour
    if ((dst = buf->datas[0].data) == NULL)
        return;
    // play silence, for now
    b->size = 0;
    pw_stream_queue_buffer(state->stream, b);
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
    // we don't know if the params array belongs to us
    // it  could be freed by speech dispatcher at a later date, which means we could be storing a pointer to freed memory
    // to ensure that the params array belongs to us and only we can free it, we will duplicate the string contained in params[1], even if such an operation would have not been necesary in the end
    state->sink_name = strdup(pars[1]);
    state->stream = pw_stream_new_simple(pw_thread_loop_get_loop(state->loop), state->sink_name, pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_ROLE, "Accessibility", NULL), &stream_events, &state);
return (AudioID *)state;
}
// configure a pipewire stream for the given speech dispatcher configuration and then prepair loop for playback
static int pipewire_begin(AudioID *id, AudioTrack track)
{
    module_state *state = (module_state *)id;
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    int format;
    int rate = track.sample_rate;
    int channels = track.num_channels;
    if (track.bits == 16)
    {
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
        format = SPA_AUDIO_FORMAT_S8;
    }
    else
    {
        pw_log_error("invalid audio format specifier");
        return -1;
    }

    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &SPA_AUDIO_INFO_RAW_INIT(.format = format, .channels = channels, .rate = rate));
    pw_stream_connect(state->stream, PW_DIRECTION_OUTPUT, PW_ID_ANY, PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS, params, 1);
    pw_thread_loop_start(state->loop);
    return 0;
}
