
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

CFLAGS = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@

lib_LTLIBRARIES = libsdflite.la libsdmluvitko.la # libsdfestival.la

libsdflite_la_SOURCES = flite.c
libsdflite_la_LDFLAGS = -lflite_cmu_us_kal -lflite_cmu_time_awb -lflite_usenglish -lflite_cmulex -lflite -lm

# libsdfestival_la_SOURCES = festival.cc
# libsdfestival_la_LDFLAGS =  -lFestival -lestools -lestbase -leststring

libsdmluvitko_la_SOURCES = mluvitko.c


