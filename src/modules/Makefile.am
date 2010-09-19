## Process this file with automake to produce Makefile.in

inc_local = -I$(top_srcdir)/include -I$(top_srcdir)/src/audio
lib_audio = -L$(top_builddir)/src/audio

AM_CFLAGS = $(ERROR_CFLAGS)
AM_CPPFLAGS = $(inc_local) -DDATADIR=\"$(snddatadir)\" $(DOTCONF_CFLAGS) $(GLIB_CFLAGS) $(GTHREAD_CFLAGS) $(ibmtts_include) $(SNDFILE_CFLAGS) -D_GNU_SOURCE

modulebin_PROGRAMS = sd_dummy sd_generic sd_festival sd_cicero

sd_dummy_SOURCES = dummy.c module_main.c module_utils.c module_utils_addvoice.c module_utils.h
sd_dummy_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_dummy_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -lpthread $(SNDFILE_LIBS) $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)
dist_snddata_DATA = dummy-message.wav

sd_festival_SOURCES = festival.c festival_client.c festival_client.h module_main.c module_utils.c module_utils.h
sd_festival_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_festival_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS) $(EXTRA_SOCKET_LIBS)

sd_generic_SOURCES = generic.c module_main.c module_utils.c module_utils_addvoice.c module_utils.h
sd_generic_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_generic_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

sd_cicero_SOURCES = cicero.c module_main.c module_utils.c module_utils.h
sd_cicero_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_cicero_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

if flite_support
modulebin_PROGRAMS += sd_flite
sd_flite_SOURCES = flite.c module_main.c module_utils.c module_utils.h
sd_flite_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_flite_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio $(flite_kal) $(flite_basic) -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)
endif

if ibmtts_support
modulebin_PROGRAMS += sd_ibmtts
sd_ibmtts_SOURCES = ibmtts.c module_main.c module_utils.c module_utils_addvoice.c module_utils.h
sd_ibmtts_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_ibmtts_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -lm -lpthread -libmeci $(SNDFILE_LIBS) $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)
endif

if espeak_support
modulebin_PROGRAMS += sd_espeak
sd_espeak_SOURCES = espeak.c module_main.c module_utils.c module_utils.h
sd_espeak_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_espeak_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -lespeak $(SNDFILE_LIBS) $(EXTRA_ESPEAK_LIBS) $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)
endif

if ivona_support
modulebin_PROGRAMS += sd_ivona
sd_ivona_SOURCES = ivona.c ivona_client.c ivona_client.h module_main.c module_utils.c module_utils.h
sd_ivona_LDFLAGS = $(RPATH) '$(spdlibdir)' $(lib_audio)
sd_ivona_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -ldumbtts -lpthread $(SNDFILE_LIBS) $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)
endif
