# Makefile for spdsend
#
# Copyright (C) 2004 Brailcom, o.p.s.
# Author: Milan Zamazal <pdm@brailcom.org>
#
# COPYRIGHT NOTICE
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.


prefix := /usr/local
bindir := $(prefix)/bin

PROGRAM := spdsend
OBJ := spdsend.o server.o client.o common.o

CC := gcc
CFLAGS := -Wall -g
LDFLAGS :=
INSTALL_PROGRAM := install

.PHONY: all install install-strip uninstall clean distclean mostlyclean \
	maintainer-clean TAGS info dvi dist check

all: $(PROGRAM)

$(PROGRAM): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) -lpthread

%.o: %.c spdsend.h
	$(CC) -c $(CFLAGS) $<

install:
	$(INSTALL_PROGRAM) $(PROGRAM) $(bindir)/

install-strip:
	$(MAKE) INSTALL_PROGRAM='$(INSTALL_PROGRAM) -s' install

uninstall:

mostlyclean:
	rm -f $(OBJ)

clean: mostlyclean
	rm -f $(PROGRAM)

distclean: clean

maintainer-clean: distclean

TAGS:

info:

dvi:

dist:

check:

