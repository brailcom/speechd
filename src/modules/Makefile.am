
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

INCLUDES = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local)

lib_LTLIBRARIES = librflite.la librmluvitko.la librfestival.la

librflite_la_SOURCES = flite.c

librmluvitko_la_SOURCES = mluvitko.c

librfestival_la_SOURCES = festival.c

