
 /*
  * speaking.h - Speech Dispatcher speech output functions header
  * 
  * Copyright (C) 2001,2002,2003 Brailcom, o.p.s, Prague 2,
  * Vysehradska 3/255, 128 00, <freesoft@freesoft.cz>
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
  * $Id: speaking.h,v 1.9 2003-10-29 11:10:43 hanke Exp $
  */

#ifndef SPEAKING_H
#define SPEAKING_H

#include "fdset.h"

OutputModule *speaking_module;
int speaking_uid;
int speaking_gid;
int highest_priority;

/* Pause and resume handling */
int pause_requested;
int pause_requested_fd;
int pause_requested_uid;
int resume_requested;

/* Speak() is responsible for getting right text from right
 * queue in right time and saying it loud through corresponding
 * synthetiser. (Note that there can be a big problem with synchronization).
 * This runs in a separate thread. */
void* speak(void* data);

/* Put this message into queue again, stripping index marks etc. */
int reload_message(TSpeechDMessage *msg);


/* Speech flow control functions */
void speaking_stop(int uid);
void speaking_stop_all();

void speaking_cancel(int uid);
void speaking_cancel_all();

int speaking_pause(int fd, int uid);
int speaking_pause_all(int fd);

int speaking_resume(int uid);
int speaking_resume_all();

/* Internal speech flow control functions */

/* If there is someone speaking on some output
 * module, return 1, otherwise 0. */
int is_sb_speaking();

/* Stops speaking and cancels currently spoken message.*/
void stop_speaking_active_module();

int stop_priority(int priority);

void stop_from_uid(int uid);

/* Decides if the message should (not) be spoken now */
static gint message_nto_speak (gconstpointer, gconstpointer);

static void set_speak_thread_attributes();


/* Do priority interaction */
void resolve_priorities(int priority);


/* Queue interaction helper functions */
static TSpeechDMessage* get_message_from_queues();
static GList* speaking_get_queue(int priority);
static void speaking_set_queue(int priority, GList *queue);
gint sortbyuid (gconstpointer a,  gconstpointer b);

/* Get the unique id of the client who is speaking
 * on some output module */
int get_speaking_client_uid();

#endif /* SPEAKING_H */
