# Makefile to convert Speech Daemon documentation into various output formats
# CVS revision: $Id: Makefile,v 1.14 2003-03-16 09:09:21 cerha Exp $
# Author: Hynek Hanke <hanke@volny.cz>

# HTMLDIR variable is set by automatic update script during CVS commits.
HTMLDIR = htmldoc

NAME = speechd

####################################################
info: $(NAME).info
$(NAME).info: $(NAME).texi
	makeinfo $<

html: $(HTMLDIR)/index.html
$(HTMLDIR)/index.html: $(NAME).texi
	makeinfo --html -o $(HTMLDIR) $<

pdf: $(NAME).pdf
$(NAME).pdf: $(NAME).texi
	texi2pdf $<

dvi: $(NAME).dvi
$(NAME).dvi: $(NAME).texi
	texi2dvi $<

###################################################

clean:
	rm -rf $(HTMLDIR)/*
	rm $(NAME).info $(NAME).pdf $(NAME).dvi
