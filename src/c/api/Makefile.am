
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@

lib_LTLIBRARIES = libspeechd.la
libspeechd_la_SOURCES= libspeechd.c libspeechd.h


