## Process this file with automake to produce Makefile.in

SUBDIRS= en cs modules clients

all-local:
	sed -e s-INCLUDE_PATH-\"$(spdconfdir)\"-g speechd.conf.in > speechd.conf


spdconf_DATA= speechd.conf
EXTRA_DIST = speechd.conf.in
