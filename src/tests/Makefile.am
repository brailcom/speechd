
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"
c_api = $(top_srcdir)/src/c/api

AM_CFLAGS = -I$(c_api) -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@ -I../audio/

bin_PROGRAMS = long_message clibrary run_test #audio

long_message_SOURCES = long_message.c $(c_api)/libspeechd.h
long_message_LDADD = $(c_api)/libspeechd.la

clibrary_SOURCES = clibrary.c $(c_api)/libspeechd.h
clibrary_LDADD = $(c_api)/libspeechd.la

run_test_SOURCES = run_test.c $(c_api)/libspeechd.h
run_test_LDADD = $(c_api)/libspeechd.la

audio_SOURCES = audio.c
audio_LDADD = -lglib-2.0 ../audio/libsdaudio.la

EXTRA_DIST= *.test
