
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

am_cflags = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@ -DWITH_OSS

if nas_support
AM_CFLAGS = $(am_cflags) -DWITH_NAS
libsdaudio_la_LDFLAGS = -version-info @LIB_SDAUDIO_CURRENT@:@LIB_SDAUDIO_REVISION@:@LIB_SDAUDIO_AGE@ -lm -laudio -lXau
else
AM_CFLAGS = $(am_cflags)
libsdaudio_la_LDFLAGS = -version-info @LIB_SDAUDIO_CURRENT@:@LIB_SDAUDIO_REVISION@:@LIB_SDAUDIO_AGE@ -lm
endif

spdlib_LTLIBRARIES = libsdaudio.la

libsdaudio_la_SOURCES = spd_audio.c spd_audio.h