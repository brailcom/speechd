# CVS revision: $Id: Makefile,v 1.4 2001-06-29 07:39:38 cerha Exp $
# Author: Tomas Cerha <cerha@brailcom.cz>

BASEDIR = ../..
include $(BASEDIR)/Makefile.config

MODULES = mluvitko.so
SRCS = mluvitko.c

all: $(MODULES)

mluvitko.so: mluvitko.o
	$(CC) $(CFLAGS) $(LIBS) -shared -export-dynamic -avoid-version mluvitko.o -o mluvitko.so 

dep:
	makedepend -- $(CFLAGS) -- $(SRCS)
	rm -f Makefile.bak

clean:
	rm -f *.o $(MODULES)

# DO NOT DELETE THIS LINE -- make  depend  depends  on  it.

mluvitko.o: /usr/include/stdio.h /usr/include/features.h
mluvitko.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
mluvitko.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/stddef.h
mluvitko.o: /usr/include/bits/types.h /usr/include/bits/pthreadtypes.h
mluvitko.o: /usr/include/bits/sched.h /usr/include/libio.h
mluvitko.o: /usr/include/_G_config.h /usr/include/wchar.h
mluvitko.o: /usr/include/bits/wchar.h /usr/include/gconv.h
mluvitko.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/stdarg.h
mluvitko.o: /usr/include/bits/stdio_lim.h /usr/include/glib-1.2/glib.h
mluvitko.o: /usr/lib/glib/include/glibconfig.h /usr/include/limits.h
mluvitko.o: /usr/include/bits/wordsize.h /usr/include/bits/posix1_lim.h
mluvitko.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
mluvitko.o: /usr/include/bits/posix2_lim.h
mluvitko.o: /usr/lib/gcc-lib/i386-linux/2.95.4/include/float.h
mluvitko.o: ../../include/module.h
