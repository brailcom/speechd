# Makefile to convert Speech Daemon documentation into various output formats
# Original xml documentation uses DocBook XML format (see README)
# CVS revision: $Id: Makefile,v 1.7 2002-11-18 15:47:36 cerha Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

DECL = /usr/lib/sgml/declaration/xml.decl

# DocBook root document
MAIN    = speechd.xml

HTMLDIR = html

XML_SRC = $(shell ls *.xml 2>/dev/null)

all: html_doc html_img

####################################################
html_doc: $(HTMLDIR)/index.html

$(HTMLDIR)/index.html: $(XML_SRC)
	@mkdir -p $(HTMLDIR)
	jade -d html.dsl -t sgml $(DECL) $(MAIN)

html_img:
	@mkdir -p $(HTMLDIR)/img
	@cp -uv figures/*.png $(HTMLDIR)/img

###################################################
clean:
	rm -rf $(HTMLDIR)/*
