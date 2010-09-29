## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libcommon.la
libcommon_la_CFLAGS = $(ERROR_CFLAGS)
libcommon_la_CPPFLAGS = "-I$(top_srcdir)/include/" $(GLIB_CFLAGS)
libcommon_la_LIBADD = $(GLIB_LIBS)
libcommon_la_SOURCES = fdsetconv.c spd_getline.c
