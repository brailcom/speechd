# Makefile to build speechd clients
# by Tomas Cerha <cerha@brailcom.cz>

include ../../Makefile.config

# all executable files to make
CLIENTS = client

all: $(CLIENTS)

# === client ===
client: client.o
client.o: client.c $(INCLUDE)/def.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -c client.c

#######################
clean:
	rm -f *.o $(CLIENTS)
