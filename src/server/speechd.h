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

#define FATAL(msg) { printf("speechd: "msg"\n"); exit(EXIT_FAILURE); }
#define DIE(msg)   { perror("speechd: "msg); exit(EXIT_FAILURE); }
#define MSG(level,args...) if (level <= LOG_LEVEL) printf(args)

#include "module.h"
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

typedef enum {			// type of voice
	MALE = 0,		// (numbers divisible by 2 should mean male)
	FEMALE = 1,
	CHILD_MALE = 2,
	CHILD_FEMALE = 3
}EVoiceType;

typedef struct{
	int fd;
        int paused;
	int priority;		// priority between 1 and 3 
	int punctuation_mode;	// this will of course not be integer 
	int speed;		// speed: 100 = normal, ??? 
	float pitch;		// pitch: ???
	char *language;		// language: default = english
	char *output_module;	// output module: festival, odmluva, epos,...
	EVoiceType voice_type;  // see EVoiceType definition
	int spelling;		// spelling mode: 0 -- off, 1 -- on
	int cap_let_recogn;	// cap. letters recogn.: 0 -- off, 1 -- on
}TFDSetElement;

typedef struct{
   guint id;
   time_t time;
   char *buf;		// the actual text with formating tags
   int bytes;		// number of bytes in buf
   TFDSetElement settings;
}TSpeechDMessage;


TSpeechDQueue *MessageQueue;

gdsl_list_t fd_settings;	// list of current settings for each
				// client (= each active socket)
/* History */

gdsl_list_t history;

typedef struct{
//	char *client_name;
	int fd;
	gdsl_list_t messages;
}THistoryClient;

OutputModule* load_output_module(gchar* modname);
int serve(int fd); 	// serve the client on this file descriptor (socket)

/* define dotconf callbacks */
DOTCONF_CB(cb_addmodule);

int fdset_list_compare (gdsl_element_t, void*);

#endif
