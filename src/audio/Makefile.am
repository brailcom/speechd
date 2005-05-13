
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

am_cflags = -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local) @glib_include@ -DWITH_OSS

if nas_support
NAS_FLAGS = -DWITH_NAS
NAS_LIBS = -laudio -L/usr/X11R6/lib -lXau
endif

if alsa_support
ALSA_FLAGS = -DWITH_ALSA
ALSA_LIBS = -lasound
endif


AM_CFLAGS = $(am_cflags) $(NAS_FLAGS) $(ALSA_FLAGS)
libsdaudio_la_LDFLAGS = -version-info @LIB_SDAUDIO_CURRENT@:@LIB_SDAUDIO_REVISION@:@LIB_SDAUDIO_AGE@ -lm $(NAS_LIBS) $(ALSA_LIBS)

spdlib_LTLIBRARIES = libsdaudio.la

libsdaudio_la_SOURCES = spd_audio.c spd_audio.h