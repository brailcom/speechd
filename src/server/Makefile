# Makefile to build robod server
# CVS revision: $Id: Makefile,v 1.7 2002-07-18 17:46:29 hanke Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

include ../../Makefile.config

CFLAGS = -g -Wall -pedantic `glib-config --cflags`
LIBS = `glib-config --libs gmodule` -ldotconf
INCLUDE = ../../include

all: robod

loader: loader.c
	$(CC) -I$(INCLUDE) $(CFLAGS) -o loader loader.c -ldl

# ===  speech server  ===
robod: robod.o server.o module.o
	$(CC) $(LIBS)  /usr/local/lib/libgdsl.so robod.o server.o module.o -o robod

robod.o: robod.c robod.h $(INCLUDE)/def.h
	$(CC) -I$(INCLUDE) $(CFLAGS) /usr/local/lib/libgdsl.so -c robod.c

server.o: server.c robod.h history.c history.h $(INCLUDE)/module.h $(INCLUDE)/def.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -c server.c

module.o: module.c robod.h $(INCLUDE)/module.h $(INCLUDE)/def.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -c module.c

#########################
clean:
	rm -f *.o robod
