
bin_PROGRAMS = speechd
speechd_SOURCES = speechd.c speechd.h server.c server.h history.c history.h module.c module.h config.c config.h parse.c parse.h set.c set.h msg.h alloc.c alloc.h compare.c compare.h speaking.c speaking.h options.c options.h output.c output.h sem_functions.c sem_functions.h index_marking.c index_marking.h
speechd_LDADD = -ldotconf -L/usr/lib/ -lglib-2.0 -lgmodule-2.0 -ldl  -L/usr/local/lib @glib_libs@ -lpthread
speechd_LDFLAGS = -rdynamic

inc_local = -I$(top_srcdir)/intl/
localedir = $(datadir)/locale
AM_CFLAGS = $(inc_local) -I$(localedir) @glib_include@ -DSYS_CONF=\"@spdconfdir@\" -DSND_DATA=\"@snddatadir@\" -DMODULEBINDIR=\"@modulebindir@\" -DMODULECONFDIR=\"@moduleconfdir@\"
