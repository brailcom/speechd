## Process this file with automake to produce Makefile.in

SUBDIRS= modules clients en cs generic

all-local: speechd.conf

speechd.conf: speechd.conf.in
	$(SED) -e s:INCLUDE_PATH:\"$(spdconfdir)\":g speechd.conf.in > speechd.conf


spdconf_DATA= speechd.conf
EXTRA_DIST = speechd.conf.in
