
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@ -L$(top_srcdir)/src/audio -I$(top_srcdir)/src/audio -I../../intl/

EXTRA_DIST = module_main.c module_utils_addvoice.c module_utils_audio.c festival_client.c festival_client.h

modulebin_PROGRAMS = sd_flite sd_generic sd_festival

sd_flite_SOURCES = flite.c module_utils.c
sd_flite_LDFLAGS = -lsdaudio -lm $(flite_basic) $(flite_kal)

sd_generic_SOURCES = generic.c module_utils.c
sd_generic_LDFLAGS = -lm

sd_festival_SOURCES = festival.c module_utils.c
sd_festival_LDFLAGS = -lm -lsdaudio
