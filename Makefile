# Makefile for speechd server, clients and output modules.
# by Tomas Cerha <cerha@brailcom.cz>

include Makefile.config

all: server clients

# ===  server  ===
server:
	(cd src/server; $(MAKE))

# ===  output modules  ===
modules:
	(cd src/modules; $(MAKE))

# ===  clients  ===
clients:
	(cd src/clients; $(MAKE))

# ===  documentation  ===
doc:
	(cd doc; $(MAKE))

# ===  clean  ===
# doc is not cleaned by default ...
clean: 
	(cd src/server;  $(MAKE) clean)
	(cd src/modules; $(MAKE) clean)
	(cd src/clients; $(MAKE) clean)
