
#ifndef SPEECHDH
 #define SPEECHDH
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

/*#ifndef EXIT_FAILURE
#  define EXIT_FAILURE        1
#endif*/

#include <limits.h>
#ifndef PATH_MAX
#  define PATH_MAX 255
#endif

/* ==================================================================== */

#define LOG_LEVEL 3
#define SPEECH_PORT 9876

#define FATAL(msg) { printf("speechd: "msg"\n"); exit(EXIT_FAILURE); }
#define DIE(msg)   { perror("speechd: "msg); exit(EXIT_FAILURE); }
#define MSG(level,args...) if (level <= LOG_LEVEL) printf(args)

#include "module.h"
#include "fdset.h"
#include "set.h"

#include "msg.h"
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
	GList *p1;	// priority 1 queue
	GList *p2;	// priority 2 queue
	GList *p3;	// priority 3 queue
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
GList *MessageList;

GList *MessagePausedList;

GList *fd_settings;	// list of current settings for each
				// client (= each active socket)

TFDSetElement GlobalFDSet;

/* History */

GList *history;

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

GList *history_settings;

typedef struct{
   int fd;
   char *client_name;
   guint id;
   int active;
   GList *messages;
}THistoryClient;

OutputModule* load_output_module(gchar* modname);
int serve(int fd); 	// serve the client on this file descriptor (socket)

gint fdset_list_compare (gconstpointer, gconstpointer, gpointer);
gint message_nto_speak (TSpeechDMessage*, gpointer, gpointer);

gint (*p_fdset_lc)();
gint (*p_msg_nto_speak)();
gint (*p_hc_lc)();

GArray *awaiting_data;
GArray *o_bytes;

char* parse_history(char *buf, int bytes, int fd);
char* parse_set(char *buf, int bytes, int fd);
char* get_param(char *buf, int n, int bytes);
int isanum(char *str);

typedef enum{
	STOP = 1,
	PAUSE = 2,
	RESUME = 3
}EStopCommands;

int stop_c(EStopCommands command, int fd);
		

#endif
