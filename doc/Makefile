# Makefile to convert speechd documentation into various output formats
# Original xml documentation uses DocBook XML format (see README)
# CVS revision: $Id: Makefile,v 1.3 2002-06-20 11:26:45 cerha Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

# Destination directory for HTML documentation. This variable must be here, to
# be able to auto generate HTML documentation after CVS commits.
DST = html

DSL  = /usr/lib/sgml/stylesheet/dsssl/docbook/nwalsh/html/docbook.dsl
DECL = /usr/lib/sgml/declaration/xml.decl

MAIN    = speechd.xml

PWD     = $(shell pwd)
XML_SRC = $(shell ls *.xml 2>/dev/null)

all: html_doc html_img

####################################################
html_doc: $(DST)/book1.html

$(DST)/book1.html: $(XML_SRC)
	@mkdir -p $(DST)
	(cd $(DST); jade -d $(DSL) -t sgml $(DECL) $(PWD)/$(MAIN))

html_img:
	@mkdir -p $(DST)/img
	@cp -uv figures/*.png $(DST)/img

###################################################
clean:
	rm -rf $(DST)/*
