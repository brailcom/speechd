# Makefile to build speechd clients
# CVS revision: $Id: Makefile,v 1.7 2002-07-18 17:51:31 hanke Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

include ../../Makefile.config

CFLAGS = -g -Wall -ansi -pedantic `glib-config --cflags`
LIBS = `glib-config --libs`
INCLUDE = ../../include

# all executable files to make
CLIENTS = say
ROBODLIB = robodlib.so

all: $(ROBODLIB) $(CLIENTS)

# === client ===
say: say.o robodlib.so
say.o: say.c robodlib.h $(INCLUDE)/def.h
	$(CC) -I$(INCLUDE) $(CFLAGS) robodlib.so -c say.c

# === robodlib.so ===
robodlib.so: robodlib.o
	$(CC) $(CFLAGS) $(LIBS) -shared -export-dynamic -avoid-version robodlib.o -o robodlib.so

robodlib.o: robodlib.c robodlib.h $(INCLUDE)/def.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -c robodlib.c

#######################
clean:
	rm -f *.o $(CLIENTS)
