## Process this file with automake to produce Makefile.in

SUBDIRS = audio

inc_local = -I$(top_srcdir)/include -I$(top_srcdir)/src/modules/audio
lib_audio = -L$(top_builddir)/src/modules/audio
common_SOURCES = module_main.c module_utils.c module_utils.h

AM_CFLAGS = $(ERROR_CFLAGS)
AM_CPPFLAGS = $(inc_local) -DDATADIR=\"$(snddatadir)\" -D_GNU_SOURCE \
	$(DOTCONF_CFLAGS) $(GLIB_CFLAGS) $(GTHREAD_CFLAGS) \
	$(ibmtts_include) $(SNDFILE_CFLAGS)

modulebin_PROGRAMS = sd_dummy sd_generic sd_festival sd_cicero

sd_dummy_SOURCES = dummy.c $(common_SOURCES) module_utils_addvoice.c
sd_dummy_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_dummy_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -lpthread \
	$(SNDFILE_LIBS) $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)
dist_snddata_DATA = dummy-message.wav

sd_festival_SOURCES = festival.c festival_client.c festival_client.h \
	$(common_SOURCES)
sd_festival_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_festival_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio \
	-lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS) $(EXTRA_SOCKET_LIBS)

sd_generic_SOURCES = generic.c $(common_SOURCES) module_utils_addvoice.c
sd_generic_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_generic_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio \
	-lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

sd_cicero_SOURCES = cicero.c $(common_SOURCES)
sd_cicero_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_cicero_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio \
	-lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

if flite_support
modulebin_PROGRAMS += sd_flite
sd_flite_SOURCES = flite.c $(common_SOURCES)
sd_flite_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_flite_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio \
	$(flite_kal) $(flite_basic) -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) \
	$(GTHREAD_LIBS)
endif

if ibmtts_support
modulebin_PROGRAMS += sd_ibmtts
sd_ibmtts_SOURCES = ibmtts.c $(common_SOURCES) module_utils_addvoice.c
sd_ibmtts_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_ibmtts_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio \
	-lm -lpthread -libmeci $(SNDFILE_LIBS) $(DOTCONF_LIBS) \
	$(GLIB_LIBS) $(GTHREAD_LIBS)
endif

if espeak_support
modulebin_PROGRAMS += sd_espeak
sd_espeak_SOURCES = espeak.c $(common_SOURCES)
sd_espeak_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_espeak_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio \
	-lespeak $(SNDFILE_LIBS) $(EXTRA_ESPEAK_LIBS) $(DOTCONF_LIBS) \
	$(GLIB_LIBS) $(GTHREAD_LIBS)
endif

if ivona_support
modulebin_PROGRAMS += sd_ivona
sd_ivona_SOURCES = ivona.c ivona_client.c ivona_client.h $(common_SOURCES)
sd_ivona_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_ivona_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio \
	-ldumbtts -lpthread $(SNDFILE_LIBS) $(DOTCONF_LIBS) $(GLIB_LIBS) \
	$(GTHREAD_LIBS)
endif
