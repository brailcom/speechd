#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <pthread.h>
#include <glib.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#ifdef USE_DLOPEN
#define SPD_AUDIO_PLUGIN_ENTRY spd_audio_plugin_get
#else
#define SPD_AUDIO_PLUGIN_ENTRY spd_pipewire_LTX_spd_audio_plugin_get
#endif
#include <spd_audio_plugin.h>

#include "../common/common.h"


struct module_state {
        struct pw_main_loop *loop;
        struct pw_stream *stream;
        };


