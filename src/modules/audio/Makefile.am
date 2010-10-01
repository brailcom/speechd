## Process this file with automake to produce Makefile.in

inc_local = -I$(top_srcdir)/include/

include_HEADERS = spd_audio_plugin.h

spdlib_LTLIBRARIES = libsdaudio.la
libsdaudio_la_CPPFLAGS = $(ALSA_CFLAGS) $(LIBAO_CFLAGS) $(PULSE_CFLAGS) \
	$(GLIB_CFLAGS) $(inc_local)
libsdaudio_la_LDFLAGS = -version-info \
	$(LIB_SDAUDIO_CURRENT):$(LIB_SDAUDIO_REVISION):$(LIB_SDAUDIO_AGE)
libsdaudio_la_LIBADD = $(GLIB_LIBS) -lpthread \
	$(ALSA_LIBS) $(LIBAO_LIBS) $(NAS_LIBS) $(PULSE_LIBS)
libsdaudio_la_SOURCES = spd_audio.c spd_audio.h spd_audio_plugin.h \
	static_plugins.c

if alsa_support
libsdaudio_la_SOURCES += alsa.c
endif

if libao_support
libsdaudio_la_SOURCES += libao.c
endif

if nas_support
libsdaudio_la_SOURCES += nas.c
endif

if oss_support
libsdaudio_la_SOURCES += oss.c
endif

if pulse_support
libsdaudio_la_SOURCES += pulse.c
endif
