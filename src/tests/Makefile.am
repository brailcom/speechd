
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"
c_api = $(top_srcdir)/src/c/api

AM_CFLAGS = -I$(c_api) -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@

bin_PROGRAMS = multiple_messages long_message
multiple_messages_SOURCES = multiple_messages.c libspeechd.h
multiple_messages_LDADD = $(c_api)/libspeechd.la

long_message_SOURCES = long_message.c $(c_api)/libspeechd.h
long_message_LDADD = $(c_api)/libspeechd.la
