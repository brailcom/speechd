
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@ -L$(top_srcdir)/src/audio -I$(top_srcdir)/src/audio -I../../intl/

EXTRA_DIST = module_utils.c festival_client.c festival_client.h

plugins_LTLIBRARIES = libsdfestival.la libsdflite.la libsdmluvitko.la libsdapollo.la libsdcstsnd.la libsdfestival.la libsdgeneric.la

libsdflite_la_SOURCES = flite.c
libsdflite_la_LDFLAGS = -lsdaudio -lflite_cmu_us_kal -lflite_cmu_time_awb -lflite_usenglish -lflite_cmulex -lflite -lm

libsdcstsnd_la_SOURCES = cstsnd.c
libsdcstsnd_la_LDFLAGS = -lsdaudio -lm

libsdfestival_la_SOURCES = festival.c
libsdfestival_la_LDFLAGS = -lm -lsdaudio

libsdgeneric_la_SOURCES = generic.c
libsdgeneric_la_LDFLAGS = -lm -lsdaudio

libsdmluvitko_la_SOURCES = mluvitko.c

libsdapollo_la_SOURCES = apollo.c
libsdapollo_la_LDFLAGS = -lrecode -lm
