# Makefile to convert Speech Daemon documentation into various output formats
# CVS revision: $Id: Makefile,v 1.17 2003-04-08 11:26:04 cerha Exp $
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
	cp -duv --parents figures/*.png $(HTMLDIR)

pdf: $(NAME).pdf
$(NAME).pdf: $(NAME).texi
	texi2pdf $<

dvi: $(NAME).dvi
$(NAME).dvi: $(NAME).texi
	texi2dvi $<

###################################################

clean:
	rm -rf $(HTMLDIR)/*
	rm -f $(NAME).info $(NAME).pdf $(NAME).dvi
