# Makefile to build speechd clients
# CVS revision: $Id: Makefile,v 1.6 2002-07-07 21:43:42 hanke Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

include ../../Makefile.config

CFLAGS = -g -Wall -ansi -pedantic `glib-config --cflags`
LIBS = `glib-config --libs`
INCLUDE = ../../include

# all executable files to make
CLIENTS = say
SPEECHDLIB = speechdlib.so

all: $(SPEECHDLIB) $(CLIENTS)

# === client ===
say: say.o speechdlib.so
say.o: say.c speechdlib.h $(INCLUDE)/def.h
	$(CC) -I$(INCLUDE) $(CFLAGS) speechdlib.so -c say.c

# === speechdlib.so ===
speechdlib.so: speechdlib.o
	$(CC) $(CFLAGS) $(LIBS) -shared -export-dynamic -avoid-version speechdlib.o -o speechdlib.so

speechdlib.o: speechdlib.c speechdlib.h $(INCLUDE)/def.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -c speechdlib.c

#######################
clean:
	rm -f *.o $(CLIENTS)
