
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

CFLAGS = -I../clients -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@

bin_PROGRAMS = multiple_messages
multiple_messages_SOURCES = multiple_messages.c libspeechd.h
multiple_messages_LDADD = ../clients/libspeechd.la

