# Makefile to build speechd output modules
# by Tomas Cerha <cerha@brailcom.cz>

include ../../Makefile.config

MODULES = mluvitko.so

all: $(MODULES)

# === mluvitko ===
mluvitko.so: mluvitko.o
mluvitko.o: mluvitko.c $(INCLUDE)/def.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -c mluvitko.c

#######################
clean:
	rm -f *.o $(MODULES)
