# Makefile to convert Speech Daemon documentation into various output formats
# Original xml documentation uses DocBook XML format (see README)
# CVS revision: $Id: Makefile,v 1.11 2002-11-21 12:54:12 cerha Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

DECL = /usr/lib/sgml/declaration/xml.decl

# DocBook root document.
MAIN = speechd.xml

# HTMLDIR variable is set by automatic update script during CVS commits.
HTMLDIR = html

PWD     = $(shell pwd)
XML_SRC = $(shell ls *.xml 2>/dev/null)

all: html html_img

####################################################
html: $(HTMLDIR)/index.html
$(HTMLDIR)/index.html: $(XML_SRC) Makefile
	@mkdir -p $(HTMLDIR)
	(cd $(HTMLDIR); jade -d $(PWD)/html.dsl -t sgml $(DECL) $(PWD)/$(MAIN))

html_img:
	@mkdir -p $(HTMLDIR)/img
	@cp -uv figures/*.png $(HTMLDIR)/img

###################################################
clean:
	rm -rf $(HTMLDIR)/*
