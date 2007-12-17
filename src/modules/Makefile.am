
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

SNDFILE_CFLAGS = @SNDFILE_CFLAGS@
SNDFILE_LIBS = @SNDFILE_LIBS@

EXTRA_DIST = module_main.c module_utils_addvoice.c festival_client.c festival_client.h

AM_CFLAGS = -Wall -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@ -L$(top_srcdir)/src/audio -I$(top_srcdir)/src/audio -I../../intl/ $(ibmtts_include) @SNDFILE_CFLAGS@ -Wl,--rpath -Wl,$(spdlibdir)

if flite_support
if ibmtts_support
modulebin_PROGRAMS = sd_flite sd_generic sd_festival sd_ibmtts sd_cicero
else
modulebin_PROGRAMS = sd_flite sd_generic sd_festival sd_cicero
endif
else
if ibmtts_support
modulebin_PROGRAMS = sd_generic sd_festival sd_ibmtts sd_cicero
else
modulebin_PROGRAMS = sd_generic sd_festival sd_cicero
endif
endif

if espeak_support
modulebin_PROGRAMS += sd_espeak
endif

sd_flite_SOURCES = flite.c module_utils.c module_utils.h
sd_flite_LDFLAGS = -rpath '$(spdlibdir)'
sd_flite_LDADD = -lsdaudio $(flite_basic) $(flite_kal) -lpthread -ldotconf @glib_libs@

sd_generic_SOURCES = generic.c module_utils.c module_utils.h
sd_generic_LDADD = -lpthread -ldotconf @glib_libs@

sd_festival_SOURCES = festival.c module_utils.c
sd_festival_LDFLAGS = -rpath '$(spdlibdir)'
sd_festival_LDADD = -lsdaudio -lpthread -ldotconf @glib_libs@

sd_ibmtts_SOURCES = ibmtts.c module_utils.c module_utils.h
sd_ibmtts_LDFLAGS = -rpath '$(spdlibdir)'
sd_ibmtts_LDADD = -lsdaudio -lm -lpthread -ldotconf -libmeci @SNDFILE_LIBS@ @glib_libs@

sd_cicero_SOURCES = cicero.c module_utils.c module_utils.h
sd_cicero_LDADD = -lpthread -ldotconf @glib_libs@

sd_espeak_SOURCES = espeak.c module_utils.c module_utils.h
sd_espeak_LDFLAGS = -rpath '$(spdlibdir)'
sd_espeak_LDADD = -lsdaudio -lespeak -lpthread -ldotconf -lgthread-2.0 @SNDFILE_LIBS@
