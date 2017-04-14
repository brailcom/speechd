## Process this file with automake to produce Makefile.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = speech-dispatcher.pc

BUILT_SOURCES = $(top_srcdir)/.version

SUBDIRS= include locale src config doc po

EXTRA_DIST= ANNOUNCE BUGS FAQ README.packagers README.translators README.style build.sh git-version-gen split-version.sh

MAINTAINERCLEANFILES=configure

testinstall: install check
	cd src/tests && $(MAKE) $(AM_MAKEFLAGS) testinstall

$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@

dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version
	echo $(VERSION) > $(distdir)/.version

-include $(top_srcdir)/git.mk

GITIGNOREFILES = $(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) py-compile
