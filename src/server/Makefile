# CVS revision: $Id: Makefile,v 1.3 2001-06-28 14:53:17 cerha Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

BASEDIR = ../..

include $(BASEDIR)/Makefile.config

CFLAGS = -g -Wall -pedantic `glib-config --cflags` -I$(BASEDIR)/include -I/usr/local/include
LIBS = `glib-config --libs gmodule` -ldotconf
 
SRCS = speechd.c server.c module.c

all: speechd

speechd: speechd.o server.o module.o
	$(CC) $(LIBS) speechd.o server.o module.o -o speechd

dep:
	makedepend -- $(CFLAGS) -- $(SRCS)

clean:
	rm -f *.o speechd

# DO NOT DELETE THIS LINE -- make  depend  depends  on  it.

speechd.o: speechd.h /usr/include/netinet/in.h /usr/include/features.h
speechd.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
speechd.o: /usr/include/stdint.h
speechd.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/stddef.h
speechd.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
speechd.o: /usr/include/bits/types.h /usr/include/bits/pthreadtypes.h
speechd.o: /usr/include/bits/sched.h /usr/include/bits/socket.h
speechd.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
speechd.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
speechd.o: /usr/include/bits/posix2_lim.h /usr/include/sys/types.h
speechd.o: /usr/include/time.h /usr/include/endian.h
speechd.o: /usr/include/bits/endian.h /usr/include/sys/select.h
speechd.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
speechd.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
speechd.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
speechd.o: /usr/include/asm/sockios.h /usr/include/bits/in.h
speechd.o: /usr/include/bits/byteswap.h /usr/include/sys/socket.h
speechd.o: /usr/include/sys/uio.h /usr/include/bits/uio.h
speechd.o: /usr/include/sys/time.h /usr/include/sys/ioctl.h
speechd.o: /usr/include/bits/ioctls.h /usr/include/asm/ioctls.h
speechd.o: /usr/include/asm/ioctl.h /usr/include/bits/ioctl-types.h
speechd.o: /usr/include/sys/ttydefaults.h /usr/include/stdio.h
speechd.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
speechd.o: /usr/include/gconv.h
speechd.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/stdarg.h
speechd.o: /usr/include/bits/stdio_lim.h /usr/include/unistd.h
speechd.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
speechd.o: /usr/include/getopt.h /usr/include/stdlib.h /usr/include/alloca.h
speechd.o: /usr/include/string.h /usr/include/glib-1.2/glib.h
speechd.o: /usr/lib/glib/include/glibconfig.h
speechd.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/float.h
speechd.o: /usr/include/glib-1.2/gmodule.h /usr/local/include/dotconf.h
speechd.o: ../../include/module.h
server.o: speechd.h /usr/include/netinet/in.h /usr/include/features.h
server.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
server.o: /usr/include/stdint.h
server.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/stddef.h
server.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
server.o: /usr/include/bits/types.h /usr/include/bits/pthreadtypes.h
server.o: /usr/include/bits/sched.h /usr/include/bits/socket.h
server.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
server.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
server.o: /usr/include/bits/posix2_lim.h /usr/include/sys/types.h
server.o: /usr/include/time.h /usr/include/endian.h
server.o: /usr/include/bits/endian.h /usr/include/sys/select.h
server.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
server.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
server.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
server.o: /usr/include/asm/sockios.h /usr/include/bits/in.h
server.o: /usr/include/bits/byteswap.h /usr/include/sys/socket.h
server.o: /usr/include/sys/uio.h /usr/include/bits/uio.h
server.o: /usr/include/sys/time.h /usr/include/sys/ioctl.h
server.o: /usr/include/bits/ioctls.h /usr/include/asm/ioctls.h
server.o: /usr/include/asm/ioctl.h /usr/include/bits/ioctl-types.h
server.o: /usr/include/sys/ttydefaults.h /usr/include/stdio.h
server.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
server.o: /usr/include/gconv.h
server.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/stdarg.h
server.o: /usr/include/bits/stdio_lim.h /usr/include/unistd.h
server.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
server.o: /usr/include/getopt.h /usr/include/stdlib.h /usr/include/alloca.h
server.o: /usr/include/string.h /usr/include/glib-1.2/glib.h
server.o: /usr/lib/glib/include/glibconfig.h
server.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/float.h
server.o: /usr/include/glib-1.2/gmodule.h /usr/local/include/dotconf.h
server.o: ../../include/module.h
module.o: speechd.h /usr/include/netinet/in.h /usr/include/features.h
module.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
module.o: /usr/include/stdint.h
module.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/stddef.h
module.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
module.o: /usr/include/bits/types.h /usr/include/bits/pthreadtypes.h
module.o: /usr/include/bits/sched.h /usr/include/bits/socket.h
module.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
module.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
module.o: /usr/include/bits/posix2_lim.h /usr/include/sys/types.h
module.o: /usr/include/time.h /usr/include/endian.h
module.o: /usr/include/bits/endian.h /usr/include/sys/select.h
module.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
module.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
module.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
module.o: /usr/include/asm/sockios.h /usr/include/bits/in.h
module.o: /usr/include/bits/byteswap.h /usr/include/sys/socket.h
module.o: /usr/include/sys/uio.h /usr/include/bits/uio.h
module.o: /usr/include/sys/time.h /usr/include/sys/ioctl.h
module.o: /usr/include/bits/ioctls.h /usr/include/asm/ioctls.h
module.o: /usr/include/asm/ioctl.h /usr/include/bits/ioctl-types.h
module.o: /usr/include/sys/ttydefaults.h /usr/include/stdio.h
module.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
module.o: /usr/include/gconv.h
module.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/stdarg.h
module.o: /usr/include/bits/stdio_lim.h /usr/include/unistd.h
module.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
module.o: /usr/include/getopt.h /usr/include/stdlib.h /usr/include/alloca.h
module.o: /usr/include/string.h /usr/include/glib-1.2/glib.h
module.o: /usr/lib/glib/include/glibconfig.h
module.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/float.h
module.o: /usr/include/glib-1.2/gmodule.h /usr/local/include/dotconf.h
module.o: ../../include/module.h
