# CVS revision: $Id: Makefile,v 1.3 2001-06-28 15:09:10 cerha Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

BASEDIR = ../..

include $(BASEDIR)/Makefile.config

CFLAGS = -g -Wall -ansi -pedantic `glib-config --cflags` -I$(BASEDIR)/include
LIBS = `glib-config --libs`

CLIENTS = say
SRCS = say.c

all: $(CLIENTS)

say: say.o

dep:
	makedepend -- $(CFLAGS) -- $(SRCS)

clean:
	rm -f *.o $(CLIENTS)

# DO NOT DELETE THIS LINE -- make  depend  depends  on  it.

say.o: /usr/include/sys/types.h /usr/include/features.h
say.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
say.o: /usr/include/bits/types.h
say.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/stddef.h
say.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
say.o: /usr/include/time.h /usr/include/endian.h /usr/include/bits/endian.h
say.o: /usr/include/sys/select.h /usr/include/bits/select.h
say.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
say.o: /usr/include/sys/sysmacros.h /usr/include/sys/socket.h
say.o: /usr/include/sys/uio.h /usr/include/bits/uio.h
say.o: /usr/include/bits/socket.h /usr/include/limits.h
say.o: /usr/include/bits/wordsize.h /usr/include/bits/posix1_lim.h
say.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
say.o: /usr/include/bits/posix2_lim.h /usr/include/bits/sockaddr.h
say.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
say.o: /usr/include/netinet/in.h /usr/include/stdint.h
say.o: /usr/include/bits/wchar.h /usr/include/bits/in.h
say.o: /usr/include/bits/byteswap.h /usr/include/arpa/inet.h
say.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
say.o: /usr/include/bits/confname.h /usr/include/getopt.h
say.o: /usr/include/stdio.h /usr/include/libio.h /usr/include/_G_config.h
say.o: /usr/include/wchar.h /usr/include/gconv.h
say.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/stdarg.h
say.o: /usr/include/bits/stdio_lim.h /usr/include/stdlib.h
say.o: /usr/include/alloca.h /usr/include/glib-1.2/glib.h
say.o: /usr/lib/glib/include/glibconfig.h
say.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/float.h ../../include/def.h
