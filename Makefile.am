## Process this file with automake to produce Makefile.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = speech-dispatcher.pc

SUBDIRS= include src config doc po

EXTRA_DIST= ANNOUNCE BUGS FAQ README.packagers README.translators build.sh

MAINTAINERCLEANFILES=configure

testinstall: install check
	cd src/tests && $(MAKE) $(AM_MAKEFLAGS) testinstall
