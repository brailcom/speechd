
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@ -L../audio/ -I../audio

lib_LTLIBRARIES = libsdflite.la libsdmluvitko.la libsdapollo.la # libsdfestival.la

libsdflite_la_SOURCES = flite.c
libsdflite_la_LDFLAGS = -lflite_cmu_us_kal -lflite_cmu_time_awb -lflite_usenglish -lflite_cmulex -lflite -lsdaudio -lm

# libsdfestival_la_SOURCES = festival.cc
# libsdfestival_la_LDFLAGS =  -lFestival -lestools -lestbase -leststring

libsdmluvitko_la_SOURCES = mluvitko.c

libsdapollo_la_SOURCES = apollo.c
libsdapollo_la_LDFLAGS = -lrecode -lm
