# Makefile to build speechd output modules
# CVS revision: $Id: Makefile,v 1.2 2001-04-10 10:42:05 cerha Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

include ../../Makefile.config

MODULES = mluvitko.so
CFLAGS = `glib-config --cflags`
LIBS = `glib-config --libs`

all: $(MODULES)

# === mluvitko ===
mluvitko.so: mluvitko.o
	$(CC) $(CFLAGS) $(LIBS) -shared -export-dynamic -avoid-version mluvitko.o -o mluvitko.so 

mluvitko.o: mluvitko.c $(INCLUDE)/module.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -fPIC -c mluvitko.c

#######################
clean:
	rm -f *.o $(MODULES)
