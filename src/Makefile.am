## Process this file with automake to produce Makefile.in

# APIs in languages other than C must be included here
# (either directly listing all files or indirectly
# in SUBDIRS if they contain another Makefile)
# otherwise they won't be included in distribution packages
EXTRA_DIST = cl/ChangeLog cl/README cl/*.lisp cl/*.asd \
guile/ChangeLog guile/Makefile guile/README guile/*.in guile/*.c guile/*.h \
python/*.py python/speechd/*.py python/Makefile.in python/speechd_config/*.py \
python/speechd_config/spd-conf python/speechd_config/test.wav \
python/speechd_config/speechd.desktop session/Makefile.in session/speechd-user-port.sh.in

SUBDIRS= server audio c modules tests python session


