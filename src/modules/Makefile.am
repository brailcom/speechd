
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

CFLAGS = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@

lib_LTLIBRARIES = libsdflite.la libsdmluvitko.la libsdfestival.la

libsdflite_la_SOURCES = flite.c

libsdmluvitko_la_SOURCES = mluvitko.c

libsdfestival_la_SOURCES = festival.c

