
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

CFLAGS = -I../intl -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@

bin_PROGRAMS = say
say_SOURCES = say.c libspeechd.h
say_LDADD = libspeechd.la

lib_LTLIBRARIES = libspeechd.la
libspeechd_la_SOURCES= libspeechd.c libspeechd.h


