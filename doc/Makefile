# Makefile to convert Speech Daemon documentation into various output formats
# Original xml documentation uses DocBook XML format (see README)
# CVS revision: $Id: Makefile,v 1.8 2002-11-18 15:55:34 cerha Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

DECL = /usr/lib/sgml/declaration/xml.decl

# DocBook root document
MAIN    = speechd.xml

DST = html

XML_SRC = $(shell ls *.xml 2>/dev/null)

all: html_doc html_img

####################################################
html_doc: $(DST)/index.html

$(DST)/index.html: $(XML_SRC)
	@mkdir -p $(DST)
	jade -d html.dsl -t sgml $(DECL) $(MAIN)

html_img:
	@mkdir -p $(DST)/img
	@cp -uv figures/*.png $(DST)/img

###################################################
clean:
	rm -rf $(DST)/*
