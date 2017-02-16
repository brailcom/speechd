## Process this file with automake to produce Makefile.in

inc_local = -I$(top_srcdir)/include
audio_SOURCES = spd_audio.c spd_audio.h
common_SOURCES = module_main.c module_utils.c module_utils.h
common_LDADD = $(SNDFILE_LIBS) $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

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
	$(common_LDADD)
dist_snddata_DATA = dummy-message.wav

sd_festival_SOURCES = festival.c festival_client.c festival_client.h \
	$(audio_SOURCES) $(common_SOURCES)
sd_festival_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	$(common_LDADD) $(EXTRA_SOCKET_LIBS)

sd_generic_SOURCES = generic.c $(audio_SOURCES) $(common_SOURCES) \
	module_utils_addvoice.c
sd_generic_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	$(common_LDADD)

sd_cicero_SOURCES = cicero.c $(audio_SOURCES) $(common_SOURCES)
sd_cicero_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	$(common_LDADD)

if flite_support
modulebin_PROGRAMS += sd_flite
sd_flite_SOURCES = flite.c $(audio_SOURCES) $(common_SOURCES)
sd_flite_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	$(flite_kal) $(flite_basic) \
	$(common_LDADD)
endif

if ibmtts_support
modulebin_PROGRAMS += sd_ibmtts
sd_ibmtts_SOURCES = ibmtts.c $(audio_SOURCES) $(common_SOURCES) \
	module_utils_addvoice.c
sd_ibmtts_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	-libmeci \
	$(common_LDADD)
endif

if espeak_support
modulebin_PROGRAMS += sd_espeak
sd_espeak_SOURCES = espeak.c $(audio_SOURCES) $(common_SOURCES)
sd_espeak_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	-lespeak $(EXTRA_ESPEAK_LIBS) \
	$(common_LDADD)
endif

if espeak_ng_support
modulebin_PROGRAMS += sd_espeak-ng
sd_espeak_ng_SOURCES = espeak-ng.c $(audio_SOURCES) $(common_SOURCES)
sd_espeak_ng_CFLAGS = -DESPEAK_NG_INCLUDE $(ESPEAK_NG_CFLAGS)
sd_espeak_ng_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	$(ESPEAK_NG_LIBS) $(EXTRA_ESPEAK_LIBS) \
	$(common_LDADD)
endif

if ivona_support
modulebin_PROGRAMS += sd_ivona
sd_ivona_SOURCES = ivona.c ivona_client.c ivona_client.h $(audio_SOURCES) \
	$(common_SOURCES)
sd_ivona_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) \
	-ldumbtts \
	$(common_LDADD)
endif

if pico_support
modulebin_PROGRAMS += sd_pico
sd_pico_SOURCES = pico.c $(audio_SOURCES) $(common_SOURCES)
sd_pico_LDADD = $(top_builddir)/src/common/libcommon.la \
	$(audio_dlopen_modules) -lttspico \
	$(common_LDADD)
endif
