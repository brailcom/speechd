
/*
 * speechd.h - Speech Deamon header
 *
 * Copyright (C) 2001,2002,2003 Ceska organizace pro podporu free software
 * (Czech Free Software Organization), Prague 2, Vysehradska 3/255, 128 00,
 * <freesoft@freesoft.cz>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: speechd.h,v 1.18 2003-04-06 20:01:27 hanke Exp $
 */

#ifndef SPEECHDH
 #define SPEECHDH

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <glib.h>
#include <gmodule.h>
#include <dotconf.h>
#include <pthread.h>
#include <limits.h>
#include <signal.h>

#ifndef PATH_MAX
#  define PATH_MAX 255
#endif

#include "def.h"
#include "module.h"
#include "history.h"
#include "fdset.h"
#include "set.h"
#include "msg.h"
#include "parse.h"

/* We should get rid of this very soon */
#define MAX_CLIENTS 20

/*  TSpeechDQueue is a queue for messages. */
typedef struct{
	GList *p1;			/* priority 1 queue */
	GList *p2;			/* priority 2 queue */
	GList *p3;			/* priority 3 queue */
}TSpeechDQueue;

/*  TSpeechDMessage is an element of TSpeechDQueue,
    that is, some text with ''formating'' tags inside. */
typedef struct{
   guint id;			/* unique id */
   time_t time;			/* when was this message received */
   char *buf;			/* the actual text */
   int bytes;			/* number of bytes in buf */
   TFDSetElement settings;	/* settings of the client when queueing this message */
}TSpeechDMessage;

/* EStopCommands describes the stop family of commands */
typedef enum{
	STOP = 1,
	PAUSE = 2,
	RESUME = 3
}EStopCommands;

/* Message logging */
void MSG(int level, char *format, ...);
#define FATAL(msg) { MSG(0,"Fatal error [%s:%d]:"msg, __FILE__, __LINE__); exit(EXIT_FAILURE); }
#define DIE(msg) { MSG(0,"Error [%s:%d]:"msg, __FILE__, __LINE__); exit(EXIT_FAILURE); }
FILE *logfile;

/* Variables for socket communication */
int fdmax;
fd_set readfds;

/* The largest assigned uid + 1 */
int max_uid;

/* speak() thread */
pthread_t speak_thread;
pthread_mutex_t element_free_mutex;

/* How many messages in total are waiting in the queues */
int msgs_to_say;

/* Table of all configured (and succesfully loaded) output modules */
GHashTable *output_modules;	
/* Table of settings for each active client (=each active socket)*/
GHashTable *fd_settings;	
/* Table of sound icons for different languages */
GHashTable *snd_icon_langs;
/* Table of relations between client file descriptors and their uids*/
GHashTable *fd_uid;

/* Speech Deamon main priority queue for messages */
TSpeechDQueue *MessageQueue;
/* List of messages from paused clients waiting for resume */
GList *MessagePausedList;
/* List of settings related to history */
GList *history_settings;
/* List of messages in history */
GList *message_history;

/* Global default settings */
TFDSetElement GlobalFDSet;

/* Arrays needed for receiving data over socket */
GArray *awaiting_data;
GArray *o_bytes;
GString *o_buf[MAX_CLIENTS];

/* Loads output module */
OutputModule* load_output_module(gchar* modname);

/* speak() runs in a separate thread, pulls messages from
 * the priority queue and sends them to synthesizers */
void* speak(void*);						

/* serve() reads data from clients and sends it to parse() */
int serve(int fd);

/* Decides if the message should (not) be spoken now */
gint message_nto_speak (TSpeechDMessage*, gpointer, gpointer);

/* Functions for parsing the input from clients */
/* also parse() */
char* get_param(char *buf, int n, int bytes);

void speaking_stop(int uid);
void speaking_cancel(int uid);
int speaking_pause(int uid);
int speaking_resume(int uid);	
	
/* Stops all messages from client on fd */
void stop_from_client(int fd);

/* isanum() tests if the given string is a number,
 * returns 1 if yes, 0 otherwise. */
int isanum(char *str);

/* Functions for searching through lists */
gint message_list_compare_fd (gconstpointer, gconstpointer, gpointer);
gint message_list_compare_uid (gconstpointer, gconstpointer, gpointer);

TFDSetElement* get_client_settings_by_uid(int uid);
TFDSetElement* get_client_settings_by_fd(int fd);
int get_client_uid_by_fd(int fd);

/* Some pointers to functions for searching through lists */
gint (*p_msg_nto_speak)();
gint (*p_msg_lc)();
gint (*p_msg_uid_lc)();
gint (*p_cli_comp_id)();
gint (*p_cli_comp_fd)();

#endif
