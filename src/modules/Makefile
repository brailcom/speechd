# Makefile to build speechd output modules
# CVS revision: $Id: Makefile,v 1.5 2002-06-30 00:15:59 hanke Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

include ../../Makefile.config

MODULES = flite.so
CFLAGS = `glib-config --cflags`
LIBS = `glib-config --libs`
INCLUDE = ../../include

all: $(MODULES)

# === mluvitko ===
mluvitko.so: mluvitko.o
	$(CC) $(CFLAGS) $(LIBS) -shared -export-dynamic -avoid-version mluvitko.o -o mluvitko.so 

mluvitko.o: mluvitko.c $(INCLUDE)/module.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -fPIC -c mluvitko.c

# === flite ===
flite.so: flite.o
	$(CC) $(CFLAGS) $(LIBS) -shared -export-dynamic -avoid-version flite.o -o flite.so 

flite.o: flite.c $(INCLUDE)/module.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -fPIC -c flite.c

# === festival ===
festival.so: festival.o
	$(CC) $(CFLAGS) $(LIBS) -shared -export-dynamic -avoid-version festival.o -o festival.so 

festival.o: festival.c $(INCLUDE)/module.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -fPIC -c festival.c


#######################
clean:
	rm -f *.o $(MODULES)
