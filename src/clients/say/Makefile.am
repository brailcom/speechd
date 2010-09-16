
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/include/"
c_api = $(top_builddir)/src/api/c

AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" $(inc_local) -I$(top_srcdir)/src/api/c

bin_PROGRAMS = spd-say
spd_say_SOURCES = say.c options.c options.h
spd_say_LDADD = $(c_api)/libspeechd.la -lpthread @EXTRA_SOCKET_LIBS@
