
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

AM_CFLAGS = -I../intl -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@ -lncurses -lmenu

bin_PROGRAMS = say spd_center
say_SOURCES = say.c libspeechd.h
say_LDADD = libspeechd.la

spd_center_SOURCES = spd_center.c spd_center.h libspeechd.h
spd_center_LDADD = libspeechd.la

lib_LTLIBRARIES = libspeechd.la
libspeechd_la_SOURCES= libspeechd.c libspeechd.h


