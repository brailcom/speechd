
bin_PROGRAMS = robod
robod_SOURCES = robod.c robod.h server.c history.c history.h module.c dc_decl.h parse.c set.c msg.h
robod_LDADD = -ldotconf -lgdsl -L/usr/lib -lgmodule -ldl  -L/usr/local/lib @glib_libs@
robod_LDFLAGS = -rdynamic

inc_local = "-I$(top_srcdir)/intl/"

localedir = $(datadir)/locale
INCLUDES = -DLOCALEDIR=\"$(localedir)\" $(inc_local) @glib_include@

