## Process this file with automake to produce Makefile.in

inc_local = -I$(top_srcdir)/include/

spdlib_LTLIBRARIES = libsdaudio.la
libsdaudio_la_SOURCES = spd_audio.c spd_audio.h
libsdaudio_la_CPPFLAGS = $(GLIB_CFLAGS) $(inc_local) \
	-DPLUGIN_DIR="\"$(audiodir)\""
libsdaudio_la_LDFLAGS = -version-info \
	$(LIB_SDAUDIO_CURRENT):$(LIB_SDAUDIO_REVISION):$(LIB_SDAUDIO_AGE)
libsdaudio_la_LIBADD = $(GLIB_LIBS) -lpthread

audio_LTLIBRARIES =

if alsa_support
audio_LTLIBRARIES +=  spd_alsa.la
spd_alsa_la_SOURCES = alsa.c
spd_alsa_la_CPPFLAGS = $(GLIB_CFLAGS) $(inc_local)  $(ALSA_CFLAGS)
spd_alsa_la_LIBADD = $(ALSA_LIBS) $(GLIB_LIBS)
spd_alsa_la_LDFLAGS = -module -avoid-version
endif

if libao_support
audio_LTLIBRARIES +=  spd_libao.la
spd_libao_la_SOURCES = libao.c
spd_libao_la_CPPFLAGS = $(GLIB_CFLAGS) $(inc_local)  $(LIBAO_CFLAGS)
spd_libao_la_LIBADD = $(LIBAO_LIBS) $(GLIB_LIBS)
spd_libao_la_LDFLAGS = -module -avoid-version
endif

if nas_support
audio_LTLIBRARIES +=  spd_nas.la
spd_nas_la_SOURCES = nas.c
spd_nas_la_CPPFLAGS = $(GLIB_CFLAGS) $(inc_local)
spd_nas_la_LIBADD = $(NAS_LIBS) $(GLIB_LIBS)
spd_nas_la_LDFLAGS = -module -avoid-version
endif

if oss_support
audio_LTLIBRARIES +=  spd_oss.la
spd_oss_la_SOURCES = oss.c
spd_oss_la_CPPFLAGS = $(GLIB_CFLAGS) $(inc_local)
spd_oss_la_LIBADD = $(GLIB_LIBS)
spd_oss_la_LDFLAGS = -module -avoid-version
endif

if pulse_support
audio_LTLIBRARIES +=  spd_pulse.la
spd_pulse_la_SOURCES = pulse.c
spd_pulse_la_CPPFLAGS = $(GLIB_CFLAGS) $(inc_local)  $(PULSE_CFLAGS)
spd_pulse_la_LIBADD = $(PULSE_LIBS) $(GLIB_LIBS)
spd_pulse_la_LDFLAGS = -module -avoid-version
endif
