
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"


EXTRA_DIST = module_main.c module_utils_addvoice.c festival_client.c festival_client.h

AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@ -L$(top_srcdir)/src/audio -I$(top_srcdir)/src/audio -I../../intl/ $(ibmtts_include) -Wl,--rpath -Wl,$(spdlibdir)

if flite_support
if ibmtts_support
modulebin_PROGRAMS = sd_flite sd_generic sd_festival sd_ibmtts
else
modulebin_PROGRAMS = sd_flite sd_generic sd_festival
endif
else
if ibmtts_support
modulebin_PROGRAMS = sd_generic sd_festival sd_ibmtts
else
modulebin_PROGRAMS = sd_generic sd_festival
endif
endif

sd_flite_SOURCES = flite.c module_utils.c module_utils.h
sd_flite_LDFLAGS = -rpath '$(spdlibdir)'
sd_flite_LDADD = -lsdaudio -lm $(flite_basic) $(flite_kal) -lpthread -ldotconf @glib_libs@

sd_generic_SOURCES = generic.c module_utils.c module_utils.h
sd_generic_LDADD = -lm -lpthread -ldotconf @glib_libs@

sd_festival_SOURCES = festival.c module_utils.c
sd_festival_LDFLAGS = -rpath '$(spdlibdir)'
sd_festival_LDADD = -lsdaudio -lm -lpthread -ldotconf @glib_libs@

sd_ibmtts_SOURCES = ibmtts.c module_utils.c module_utils.h
sd_ibmtts_LDFLAGS = -rpath '$(spdlibdir)'
sd_ibmtts_LDADD = -lsdaudio -lm -lpthread -ldotconf -libmeci @glib_libs@

