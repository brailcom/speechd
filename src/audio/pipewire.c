#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include <glib.h>

#include <pipewire/pipewire.h>
#include<pipewire/thread-loop.h>

#include <spa/param/audio/format-utils.h>
#include<spa/utils/ringbuffer.h>

#include <spd_audio_plugin.h>
#include "../common/common.h"

//for the buffer size which will be allocated on the heap, to store the samples produced by speech dispatcher and read by pipewire
//128  integers wide, this buffer will be used as backing storage by a ring buffer, which will also insure that the threads are syncronised when reading from and writing to the storage area
#define SAMPLE_BUFFER_SIZE 128

//speech dispatcher backend entry point, defined in multiple configurations
#ifdef USE_DLOPEN
#define SPD_AUDIO_PLUGIN_ENTRY spd_audio_plugin_get
#else
#define SPD_AUDIO_PLUGIN_ENTRY spd_pipewire_LTX_spd_audio_plugin_get
#endif

//state of the backend, where all components are gathered. This will be allocated on the heap
struct module_state
{
    AudioID id; //to comply with the speech dispatcher contract, make the first field of the struct the type expected by the spd callbacks, so that casting from it isn't entirely undefined behaviour
    struct pw_thread_loop *loop; //a pipewire loop object which can be used without blocking the main thread, in this case the thread of speech dispatcher
    struct pw_stream *stream; //this represents an instance of this module, a node of the media graph along with other metadata, which will be linked to the default output device
    struct spa_ringbuffer *rb; //a thread-safe ring buffer implementation which uses atomic operations  to guarantee some semblance of thread safety, therefore it doesn't require a mutex
char * sample_buffer; //the heap storage memory which will be backing the atomic ring buffer implementation, it's on the heap because this struct would be transfered across callback functions a lot, and such a large buffer could cause a stack overflow on some hardware architectures
};

