# Makefile to convert Speech Daemon documentation into various output formats
# Original xml documentation uses DocBook XML format (see README)
# CVS revision: $Id: Makefile,v 1.10 2002-11-18 16:54:01 cerha Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

DECL = /usr/lib/sgml/declaration/xml.decl

# DocBook root document.
MAIN = speechd.xml

# Destination directory for HTML version.  This variable must be here, to
# be able to auto generate HTML documentation after CVS commits.
DST = html

PWD     = $(shell pwd)
XML_SRC = $(shell ls *.xml 2>/dev/null)

all: html_doc html_img

####################################################
html_doc: $(DST)/index.html

$(DST)/index.html: $(XML_SRC) Makefile
	@mkdir -p $(DST)
	(cd $(DST); jade -d $(PWD)/html.dsl -t sgml $(DECL) $(PWD)/$(MAIN))

html_img:
	@mkdir -p $(DST)/img
	@cp -uv figures/*.png $(DST)/img

###################################################
clean:
	rm -rf $(DST)/*
