
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

AM_CFLAGS = -Wall -DLOCALEDIR=\"$(localedir)\" -D_GNU_SOURCE -I/usr/include/ $(inc_local) @glib_include@

lib_LTLIBRARIES = libspeechd.la
libspeechd_la_SOURCES = libspeechd.c
libspeechd_la_HEADERS = libspeechd.h
libspeechd_ladir = $(includedir)
libspeechd_la_LDFLAGS = -version-info @LIB_SPD_CURRENT@:@LIB_SPD_REVISION@:@LIB_SPD_AGE@ -lpthread
libspeechd_la_LIBADD = @glib_libs@

if ENABLE_SESSION
AM_CFLAGS += -DENABLE_SESSION -DSPD_SPAWN_CMD=\"@prefix@/bin/speechd-server-spawn\"
endif
