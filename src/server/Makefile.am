
bin_PROGRAMS = robod
robod_SOURCES = robod.c robod.h server.c history.c history.h module.c dc_decl.h parse.c set.c msg.h
robod_LDADD = -ldotconf -L/usr/lib/ -lglib-2.0 -lgmodule-2.0 -ldl  -L/usr/local/lib @glib_libs@
robod_LDFLAGS = -rdynamic

inc_local = "-I$(top_srcdir)/intl/"
localedir = $(datadir)/locale
CFLAGS = $(inc_local) -I$(localedir) @glib_include@
