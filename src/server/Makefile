# Makefile to build speechd server
# by Tomas Cerha <cerha@brailcom.cz>

include ../../Makefile.config

SERVER = speechd

all: speechd

# ===  speech server  ===
$(SERVER): $(SERVER).o

$(SERVER).o: $(SERVER).c $(INCLUDE)/def.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -c $(SERVER).c

#########################
clean:
	rm -f *.o $(SERVER)