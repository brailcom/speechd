
localedir = $(datadir)/locale
inc_local = "-I$(top_srcdir)/intl/"

INCLUDES = -I../intl -DLOCALEDIR=\"$(localedir)\" -I/usr/include/ $(inc_local)

bin_PROGRAMS = say
say_SOURCES = say.c robodlib.h
say_LDADD = librobod.la

lib_LTLIBRARIES = librobod.la
librobod_la_SOURCES= robodlib.c robodlib.h


