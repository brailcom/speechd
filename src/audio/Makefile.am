
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@

lib_LTLIBRARIES = libsdaudio.la

libsdaudio_la_SOURCES = spd_audio.c spd_audio.h
libsdaudio_la_LDFLAGS = -lflite -lm -lglib-2.0

