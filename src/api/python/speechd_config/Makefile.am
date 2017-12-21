## Process this file with automake to produce Makefile.in

dist_snddata_DATA = test.wav
DESKTOP_FILES = speechd.desktop.in
desktop_DATA = $(DESKTOP_FILES:.desktop.in=.desktop)
desktopdir = $(spddesktopconforigdir)

speechd.desktop: speechd.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

dist_bin_SCRIPTS = spd-conf

speechd_pythondir = $(pyexecdir)/speechd_config
speechd_python_PYTHON = __init__.py
nodist_speechd_python_PYTHON = config.py paths.py

paths_edit = sed \
	-e "s:[@]spdconforigdir[@]:$(spdconforigdir):" \
	-e "s:[@]spdconfdir[@]:$(spdconfdir):" \
	-e "s:[@]snddatadir[@]:$(snddatadir):" \
	-e "s:[@]spddesktopconforigdir[@]:$(spddesktopconforigdir):"

config_edit = sed \
	-e "s:[@]GETTEXT_PACKAGE[@]:$(GETTEXT_PACKAGE):" \
	-e "s:[@]localedir[@]:$(localedir):"

paths.py: Makefile
	rm -f $@
	srcdir=; \
	test -f ./$@.in || srcdir=$(srcdir)/; \
	$(paths_edit) $${srcdir}$@.in > $@

config.py: Makefile
	rm -f $@
	srcdir=; \
	test -f ./$@.in || srcdir=$(srcdir)/; \
	$(config_edit) $${srcdir}$@.in > $@

paths.py: $(srcdir)/paths.py.in

CLEANFILES = paths.py config.py speechd.desktop

EXTRA_DIST = paths.py.in speechd.desktop.in config.py.in

-include $(top_srcdir)/git.mk
