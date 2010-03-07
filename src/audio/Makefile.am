
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

include_HEADERS = spd_audio_plugin.h

am_cflags = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@

spdlib_LTLIBRARIES = libsdaudio.la
libsdaudio_la_SOURCES = spd_audio.c spd_audio.h spd_audio_plugin.h static_plugins.c

if nas_support
libsdaudio_la_SOURCES += nas.c
endif

if pulse_support
libsdaudio_la_SOURCES += pulse.c
endif

if alsa_support
libsdaudio_la_SOURCES += alsa.c
endif

if libao_support
libsdaudio_la_SOURCES += libao.c
endif

if oss_support
libsdaudio_la_SOURCES += oss.c
endif

EXTRA_DIST = alsa.c libao.c oss.c nas.c pulse.c static_plugins.c.in

AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@
libsdaudio_la_LDFLAGS = -version-info @LIB_SDAUDIO_CURRENT@:@LIB_SDAUDIO_REVISION@:@LIB_SDAUDIO_AGE@ -lpthread $(SPD_AUDIO_LIBS)
