
info_TEXINFOS = ssip.texi speechd.texi speechd-cs.texi
speechd_TEXINFOS = fdl.texi gpl.texi


SUFFIXES = .html
html_docs = speechd.html ssip.html
HTMLDIR = htmldoc

MAINTAINERCLEANFILES = speechd/*

.texi.html:
	$(MAKEINFO) --html -o $(HTMLDIR) $<
	cp -duv --parents figures/*.png $(HTMLDIR)

.PHONY: html
html: version.texi $(html_docs)


