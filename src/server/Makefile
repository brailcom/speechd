# Makefile to build speechd server
# CVS revision: $Id: Makefile,v 1.2 2001-04-10 10:42:05 cerha Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

include ../../Makefile.config

CFLAGS = -g -Wall -pedantic `glib-config --cflags`
LIBS = `glib-config --libs gmodule` -ldotconf

all: speechd

loader: loader.c
	$(CC) -I$(INCLUDE) $(CFLAGS) -o loader loader.c -ldl

# ===  speech server  ===
speechd: speechd.o server.o module.o
	$(CC) $(LIBS) speechd.o server.o module.o -o speechd

speechd.o: speechd.c speechd.h $(INCLUDE)/def.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -c speechd.c

server.o: server.c speechd.h $(INCLUDE)/module.h $(INCLUDE)/def.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -c server.c

module.o: module.c speechd.h $(INCLUDE)/module.h $(INCLUDE)/def.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -c module.c

#########################
clean:
	rm -f *.o speechd