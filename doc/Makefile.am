
info_TEXINFOS = ssip.texi speech-dispatcher.texi speech-dispatcher-cs.texi spd-say.texi

SUFFIXES = .html
html_docs = speechd.html ssip.html spd-say.texi
HTMLDIR = htmldoc

MAINTAINERCLEANFILES = speechd/*

.texi.html:
	$(MAKEINFO) --html -o $(HTMLDIR) $<
	cp -duv --parents figures/*.png $(HTMLDIR)

.PHONY: html
html: version.texi $(html_docs)


