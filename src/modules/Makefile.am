localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/include/"

SNDFILE_CFLAGS = @SNDFILE_CFLAGS@
SNDFILE_LIBS = @SNDFILE_LIBS@

EXTRA_DIST = module_main.c module_utils_addvoice.c festival_client.c festival_client.h ivona_client.c dummy.c dummy-message.wav

AM_CFLAGS = @ERROR_CFLAGS@ -DLOCALEDIR=\"$(localedir)\" -DDATADIR=\"$(snddatadir)\" $(inc_local) $(DOTCONF_CFLAGS) $(GLIB_CFLAGS) $(GTHREAD_CFLAGS) -L$(top_builddir)/src/audio -I$(top_srcdir)/src/audio $(ibmtts_include) @SNDFILE_CFLAGS@ -D_GNU_SOURCE

modulebin_PROGRAMS = sd_dummy sd_generic sd_festival sd_cicero

snddata_DATA = dummy-message.wav

if flite_support
modulebin_PROGRAMS += sd_flite
endif

if ibmtts_support
modulebin_PROGRAMS += sd_ibmtts
endif

if espeak_support
modulebin_PROGRAMS += sd_espeak
endif

if ivona_support
modulebin_PROGRAMS += sd_ivona
endif

sd_flite_SOURCES = flite.c module_main.c module_utils.c module_utils.h
sd_flite_LDFLAGS = @RPATH@ '$(spdlibdir)'
sd_flite_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio $(flite_kal) $(flite_basic) -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

sd_generic_SOURCES = generic.c module_main.c module_utils.c module_utils_addvoice.c module_utils.h
sd_generic_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

sd_festival_SOURCES = festival.c festival_client.c festival_client.h module_main.c module_utils.c module_utils.h
sd_festival_LDFLAGS = @RPATH@ '$(spdlibdir)'
sd_festival_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS) @EXTRA_SOCKET_LIBS@

sd_ibmtts_SOURCES = ibmtts.c module_main.c module_utils.c module_utils_addvoice.c module_utils.h
sd_ibmtts_LDFLAGS = @RPATH@ '$(spdlibdir)'
sd_ibmtts_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -lm -lpthread -libmeci @SNDFILE_LIBS@ $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

sd_cicero_SOURCES = cicero.c module_main.c module_utils.c module_utils.h
sd_cicero_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

sd_espeak_SOURCES = espeak.c module_main.c module_utils.c module_utils.h
sd_espeak_LDFLAGS = @RPATH@ '$(spdlibdir)'
sd_espeak_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -lespeak @SNDFILE_LIBS@ @EXTRA_ESPEAK_LIBS@ $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

sd_ivona_SOURCES = ivona.c ivona_client.c ivona_client.h module_main.c module_utils.c module_utils.h
sd_ivona_LDFLAGS = @RPATH@ '$(spdlibdir)'
sd_ivona_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -ldumbtts -lpthread @SNDFILE_LIBS@ $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

sd_dummy_SOURCES = dummy.c module_main.c module_utils.c module_utils_addvoice.c module_utils.h
sd_dummy_LDFLAGS = @RPATH@ '$(spdlibdir)'
sd_dummy_LDADD = $(top_builddir)/src/common/libcommon.la -lsdaudio -lpthread @SNDFILE_LIBS@ $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)
