# Makefile to convert Speech Daemon documentation into various output formats
# Original xml documentation uses DocBook XML format (see README)
# CVS revision: $Id: Makefile,v 1.13 2003-02-01 22:09:48 hanke Exp $
# Author: Hynek Hanke <hanke@volny.cz>


# HTMLDIR variable is set by automatic update script during CVS commits.
HTMLDIR = html

all: info html pdf dvi

####################################################
info: 
	makeinfo speechd.texi

html:
	makeinfo --html speechd.texi

pdf:
	texi2pdf speechd.texi

dvi:
	texi2dvi speechd.texi

###################################################
clean:
	rm -rf $(HTMLDIR)/*
