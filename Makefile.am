## Process this file with automake to produce Makefile.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = speech-dispatcher.pc

BUILT_SOURCES = $(top_srcdir)/.version

SUBDIRS= include locale src config doc po

EXTRA_DIST= config.rpath ANNOUNCE BUGS FAQ README.packagers README.translators README.overview.md README.style.md build.sh git-version-gen split-version.sh speech-dispatcherd.service.in po/Makevars.in

CLEANFILES= po/Makevars

MAINTAINERCLEANFILES=configure

AM_DISTCHECK_CONFIGURE_FLAGS=--without-systemdsystemunitdir

nodist_systemdsystemunit_DATA = \
	speech-dispatcherd.service
CLEANFILES += \
	speech-dispatcherd.service
%.service: %.service.in
	$(AM_V_GEN)sed -e 's,@bindir\@,$(bindir),g' $< > $@

testinstall: install check
	cd src/tests && $(MAKE) $(AM_MAKEFLAGS) testinstall

$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@

dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version
	echo $(VERSION) > $(distdir)/.version

-include $(top_srcdir)/git.mk

GITIGNOREFILES = $(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) py-compile

ACLOCAL_AMFLAGS = -I m4
