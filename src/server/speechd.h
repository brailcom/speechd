
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
 * $Id: speechd.h,v 1.10 2003-02-01 22:16:55 hanke Exp $
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

/*#ifndef EXIT_FAILURE
#  define EXIT_FAILURE        1
#endif*/

#include <limits.h>
#ifndef PATH_MAX
#  define PATH_MAX 255
#endif

/* ==================================================================== */

#include "def.h"

#include "module.h"
#include "fdset.h"
#include "set.h"

#include "msg.h"
/* ==================================================================== */

fd_set readfds;

int fdmax;

int msgs_to_say;
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
   int uid;
   guint cur_client_id;
   int cur_pos;
   ESort sorted;
}THistSetElement;

GList *history_settings;

typedef struct{
   int fd;
   guint uid;
   char *client_name;
   int active;
   GList *messages;
}THistoryClient;

OutputModule* load_output_module(gchar* modname);
int serve(int fd); 	// serve the client on this file descriptor (socket)

gint fdset_list_compare_fd (gconstpointer, gconstpointer, gpointer);
gint fdset_list_compare_uid (gconstpointer, gconstpointer, gpointer);
gint message_nto_speak (TSpeechDMessage*, gpointer, gpointer);
gint message_list_compare_fd (gconstpointer, gconstpointer, gpointer);

gint (*p_fdset_lc_fd)();
gint (*p_fdset_lc_uid)();
gint (*p_msg_nto_speak)();
gint (*p_hc_lc)();
gint (*p_msg_lc)();
gint (*p_cli_comp_id)();
gint (*p_cli_comp_fd)();

GArray *awaiting_data;
GArray *o_bytes;

char* parse_history(char *buf, int bytes, int fd);
char* parse_set(char *buf, int bytes, int fd);
char* get_param(char *buf, int n, int bytes);

typedef enum{
	STOP = 1,
	PAUSE = 2,
	RESUME = 3
}EStopCommands;

int stop_c(EStopCommands command, int fd, int target);
		
/* isanum() tests if the given string is a number,
 * returns 1 if yes, 0 otherwise. */
int isanum(char *str);

void stop_from_client(int fd);
		

#endif
