
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@ -L$(top_srcdir)/src/audio -I$(top_srcdir)/src/audio -I../../intl/

EXTRA_DIST = module_utils.c module_main.c module_utils_addvoice.c festival_client.c festival_client.h festival-interface

modulebin_PROGRAMS = sd_flite sd_apollo sd_cstsnd sd_generic sd_festival
sd_flite_SOURCES = flite.c
sd_flite_LDFLAGS = -lsdaudio -lflite_cmu_us_kal -lflite_cmu_time_awb -lflite_usenglish -lflite_cmulex -lflite -lm

sd_apollo_SOURCES = apollo.c
sd_apollo_LDFLAGS = -lm -lrecode

sd_cstsnd_SOURCES = cstsnd.c
sd_cstsnd_LDFLAGS = -lsdaudio

sd_generic_SOURCES = generic.c
sd_generic_LDFLAGS = -lm

sd_festival_SOURCES = festival.c
sd_festival_LDFLAGS = -lm -lsdaudio
