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
#include <gdsl/gdsl.h>

#ifndef SPEECHDH
 #define SPEECHDH

#ifndef EXIT_FAILURE
#  define EXIT_FAILURE        1
#endif

#include <limits.h>
#ifndef PATH_MAX
#  define PATH_MAX 255
#endif

/* ==================================================================== */

#define LOG_LEVEL 3
#define SPEECH_PORT 9876

#define FATAL(msg) { printf("robod: "msg"\n"); exit(EXIT_FAILURE); }
#define DIE(msg)   { perror("robod: "msg); exit(EXIT_FAILURE); }
#define MSG(level,args...) if (level <= LOG_LEVEL) printf(args)

#include "module.h"
#include "fdset.h"
/* ==================================================================== */

fd_set readfds;
int fdmax;

/* associative array of all configured (and succesfully loaded) output modules */

GHashTable *output_modules;

/* 
  TSpeechDQueue is a queue for messages. 
*/

typedef struct{
	char *name[100];	// name of the output module
	gdsl_queue_t p1;	// priority 1 queue
	gdsl_queue_t p2;	// priority 2 queue
	gdsl_queue_t p3;	// priority 3 queue
}TSpeechDQueue;

/*
  TSpeechDMessage is an element of TSpeechDQueue,
  that is, some text with ''formating'' tags inside.
*/

typedef struct{
   guint id;
   time_t time;
   char *buf;		// the actual text
   int bytes;		// number of bytes in buf
   TFDSetElement settings;
}TSpeechDMessage;


TSpeechDQueue *MessageQueue;

gdsl_list_t MessagePausedList;

gdsl_list_t fd_settings;	// list of current settings for each
				// client (= each active socket)
/* History */

gdsl_list_t history;

typedef enum{
   BY_TIME = 0,
   BY_ALPHABET = 1
}ESort;

typedef struct{
   int fd;
   guint cur_client_id;
   int cur_pos;
   ESort sorted;
}THistSetElement;

gdsl_list_t history_settings;

typedef struct{
   int fd;
   char *client_name;
   guint id;
   int active;
   gdsl_list_t messages;
}THistoryClient;

OutputModule* load_output_module(gchar* modname);
int serve(int fd); 	// serve the client on this file descriptor (socket)

/* define dotconf callbacks */
DOTCONF_CB(cb_addmodule);

int fdset_list_compare (gdsl_element_t, void*);

#endif
