# Makefile to convert speechd documentation into various output formats
# Original xml documentation uses DocBook XML format

DSL  = /usr/lib/sgml/stylesheet/dsssl/docbook/nwalsh/html/docbook.dsl
DECL = /usr/lib/sgml/declaration/xml.decl

MAIN = speechd.xml
SRC  = speechd.xml preface.xml intro.xml design.xml implementation.xml
HTML = html/book1.html

all: html

####################################################
html: $(HTML)
$(HTML): $(SRC)
	(PWD=`pwd`; cd html; jade -d $(DSL) -t sgml $(DECL) $(PWD)/$(MAIN))

###################################################
clean:
	rm -f html/*
