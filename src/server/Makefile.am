
bin_PROGRAMS = speechd
speechd_SOURCES = speechd.c speechd.h server.c history.c history.h module.c dc_decl.h parse.c parse.h set.c set.h msg.h alloc.c alloc.h sndicon.c sndicon.h compare.c compare.h speaking.c speaking.h options.c options.h output.c output.h sem_functions.c sem_functions.h
speechd_LDADD = -ldotconf -L/usr/lib/ -lglib-2.0 -lgmodule-2.0 -ldl  -L/usr/local/lib @glib_libs@ -lpthread
speechd_LDFLAGS = -rdynamic

inc_local = -I$(top_srcdir)/intl/
localedir = $(datadir)/locale
AM_CFLAGS = $(inc_local) -I$(localedir) @glib_include@ -DSYS_CONF=\"@spdconfdir@\" -DSND_DATA=\"@snddatadir@\" -DMODULEBINDIR=\"@modulebindir@\" -DMODULECONFDIR=\"@moduleconfdir@\"
