# Makefile to build speechd clients
# CVS revision: $Id: Makefile,v 1.2 2001-04-10 10:42:05 cerha Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

include ../../Makefile.config

CFLAGS = -g -Wall -ansi -pedantic `glib-config --cflags`
LIBS = `glib-config --libs`

# all executable files to make
CLIENTS = say

all: $(CLIENTS)

# === client ===
say: say.o
say.o: say.c $(INCLUDE)/def.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -c say.c

#######################
clean:
	rm -f *.o $(CLIENTS)
