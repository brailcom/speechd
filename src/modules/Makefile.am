
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

CFLAGS = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@

lib_LTLIBRARIES = librflite.la librmluvitko.la librfestival.la

librflite_la_SOURCES = flite.c

librmluvitko_la_SOURCES = mluvitko.c

librfestival_la_SOURCES = festival.c

