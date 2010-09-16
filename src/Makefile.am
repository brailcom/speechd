## Process this file with automake to produce Makefile.in

# APIs in languages other than C must be included here
# (either directly listing all files or indirectly
# in SUBDIRS if they contain another Makefile)
# otherwise they won't be included in distribution packages

EXTRA_DIST = \
             guile/ChangeLog guile/Makefile guile/README guile/gssip.scm.in \
             guile/gssip.c guile/gssip.h

SUBDIRS=common server audio modules api clients tests

if HAVE_PYTHON
SUBDIRS += python
endif

DIST_SUBDIRS=common server audio modules api clients tests python

