# Makefile for speechd server, clients and output modules.
# CVS revision: $Id: Makefile,v 1.3 2001-06-29 09:26:43 cerha Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

all: server clients modules

# ===  server  ===
server:
	$(MAKE) --directory src/server

# ===  output modules  ===
modules:
	$(MAKE) --directory src/modules

# ===  clients  ===
clients:
	$(MAKE) --directory src/clients

# ===  documentation  ===
doc:
	$(MAKE) --directory doc

# ===  clean  ===
# doc is not cleaned by default ...
clean:
	$(MAKE) --directory src/server clean 
	$(MAKE) --directory src/modules clean
	$(MAKE) --directory src/clients clean
