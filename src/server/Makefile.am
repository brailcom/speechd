
bin_PROGRAMS = speechd
speechd_SOURCES = speechd.c speechd.h server.c history.c history.h module.c dc_decl.h parse.c parse.h set.c msg.h alloc.c alloc.h
speechd_LDADD = -ldotconf -L/usr/lib/ -lglib-2.0 -lgmodule-2.0 -ldl  -L/usr/local/lib @glib_libs@ -lpthread
speechd_LDFLAGS = -rdynamic

inc_local = "-I$(top_srcdir)/intl/"
localedir = $(datadir)/locale
CFLAGS = $(inc_local) -I$(localedir) @glib_include@  -DSYS_CONF=\"@sysconfdir@\" -DLIB_DIR=\"@libdir@\"
