## Process this file with automake to produce Makefile.in

SUBDIRS = audio

inc_local = -I$(top_srcdir)/include
audio_SOURCES = spd_audio.c spd_audio.h
common_SOURCES = module_main.c module_utils.c module_utils.h

AM_CFLAGS = $(ERROR_CFLAGS)
AM_CPPFLAGS = $(inc_local) -DDATADIR=\"$(snddatadir)\" -D_GNU_SOURCE \
	-DPLUGIN_DIR="\"$(audiodir)\"" \
	$(DOTCONF_CFLAGS) $(GLIB_CFLAGS) $(GTHREAD_CFLAGS) \
	$(ibmtts_include) $(SNDFILE_CFLAGS)

modulebin_PROGRAMS = sd_dummy sd_generic sd_festival sd_cicero

sd_dummy_SOURCES = dummy.c $(audio_SOURCES) $(common_SOURCES) \
	module_utils_addvoice.c
sd_dummy_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	$(SNDFILE_LIBS) $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)
dist_snddata_DATA = dummy-message.wav

sd_festival_SOURCES = festival.c festival_client.c festival_client.h \
	$(audio_SOURCES) $(common_SOURCES)
sd_festival_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	$(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS) $(EXTRA_SOCKET_LIBS)

sd_generic_SOURCES = generic.c $(audio_SOURCES) $(common_SOURCES) \
	module_utils_addvoice.c
sd_generic_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	$(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

sd_cicero_SOURCES = cicero.c $(audio_SOURCES) $(common_SOURCES)
sd_cicero_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	$(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

if flite_support
modulebin_PROGRAMS += sd_flite
sd_flite_SOURCES = flite.c $(audio_SOURCES) $(common_SOURCES)
sd_flite_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	$(flite_kal) $(flite_basic) $(DOTCONF_LIBS) $(GLIB_LIBS) \
	$(GTHREAD_LIBS)
endif

if ibmtts_support
modulebin_PROGRAMS += sd_ibmtts
sd_ibmtts_SOURCES = ibmtts.c $(audio_SOURCES) $(common_SOURCES) \
	module_utils_addvoice.c
sd_ibmtts_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	-libmeci $(SNDFILE_LIBS) $(DOTCONF_LIBS) \
	$(GLIB_LIBS) $(GTHREAD_LIBS)
endif

if espeak_support
modulebin_PROGRAMS += sd_espeak
sd_espeak_SOURCES = espeak.c $(audio_SOURCES) $(common_SOURCES)
sd_espeak_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	-lespeak $(SNDFILE_LIBS) $(EXTRA_ESPEAK_LIBS) $(DOTCONF_LIBS) \
	$(GLIB_LIBS) $(GTHREAD_LIBS)
endif

if ivona_support
modulebin_PROGRAMS += sd_ivona
sd_ivona_SOURCES = ivona.c ivona_client.c ivona_client.h $(audio_SOURCES) \
	$(common_SOURCES)
sd_ivona_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	-ldumbtts $(SNDFILE_LIBS) $(DOTCONF_LIBS) $(GLIB_LIBS) \
	$(GTHREAD_LIBS)
endif

if pico_support
modulebin_PROGRAMS += sd_pico
sd_pico_SOURCES = pico.c $(audio_SOURCES) $(common_SOURCES)
sd_pico_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) -lttspico \
	$(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)
endif
