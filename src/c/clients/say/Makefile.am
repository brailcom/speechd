
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"
c_api = $(top_srcdir)/src/c/api

AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@ -I$(c_api)

bin_PROGRAMS = spd-say
spd_say_SOURCES = say.c $(c_api)/libspeechd.h
spd_say_LDADD = $(c_api)/libspeechd.la
