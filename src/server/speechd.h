#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include <dotconf.h>

#ifndef EXIT_FAILURE
#  define EXIT_FAILURE        1
#endif

#include <limits.h>
#ifndef PATH_MAX
#  define PATH_MAX 255
#endif

/* ====================================================================================== */

#include "module.h"

#define LOG_LEVEL 3
#define SPEECH_PORT 9876

#define FATAL(msg) { printf("speechd: "msg"\n"); exit(EXIT_FAILURE); }
#define DIE(msg)   { perror("speechd: "msg); exit(EXIT_FAILURE); }
#define MSG(level,args...) if (level <= LOG_LEVEL) printf(args)

/* ====================================================================================== */

OutputModule* load_output_module(gchar* modname);
int serve(int fd, GHashTable *output_modules); /* serve the client on this file descriptor (socket) */

/* define dotconf callbacks */
DOTCONF_CB(cb_addmodule);

