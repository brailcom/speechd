
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"
c_api = $(top_srcdir)/src/c/api

AM_CFLAGS = -I$(c_api) -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@ -I../audio/

bin_PROGRAMS = long_message clibrary clibrary2 run_test connection_recovery

long_message_SOURCES = long_message.c $(c_api)/libspeechd.h
long_message_LDADD = $(c_api)/libspeechd.la @EXTRA_SOCKET_LIBS@

clibrary_SOURCES = clibrary.c $(c_api)/libspeechd.h
clibrary_LDADD = $(c_api)/libspeechd.la @EXTRA_SOCKET_LIBS@

clibrary2_SOURCES = clibrary2.c $(c_api)/libspeechd.h
clibrary2_LDADD = $(c_api)/libspeechd.la @EXTRA_SOCKET_LIBS@

connection_recovery_SOURCES = connection-recovery.c $(c_api)/libspeechd.h
connection_recovery_LDADD = $(c_api)/libspeechd.la @EXTRA_SOCKET_LIBS@

run_test_SOURCES = run_test.c $(c_api)/libspeechd.h
run_test_LDADD = $(c_api)/libspeechd.la @EXTRA_SOCKET_LIBS@

EXTRA_DIST= *.test *.wav
