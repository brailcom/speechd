
 /*
  * server.c - Speech Dispatcher server core
  * 
  * Copyright (C) 2001,2002,2003 Brailcom, o.p.s
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
  * $Id: server.c,v 1.47 2003-06-05 16:29:40 hanke Exp $
  */

#include "speechd.h"

int last_message_id = -1;

/* Switches `receiving data' mode on and off for specified client */
int server_data_on(int fd);
void server_data_off(int fd);

/* Put a message into its queue.
 *
 * Parameters:
 *   new -- the (allocated) message structure to queue, it must contain
 *          the text to be queued in new->buf
 *   fd  -- file descriptor of the calling client (positive)       
 *          unique id if the client is gone (negative)
 *   history_flag -- should this message be included in history?
 *   type -- type of the message (see intl/fdset.h)
 *   reparted -- if this is a preprocessed message reparted
 *             in more pieces
 * It returns 0 on success, -1 otherwise.
 */
int
queue_message(TSpeechDMessage *new, int fd, int history_flag, EMessageType type, int reparted)
{
    GList *gl;
    TFDSetElement *settings;
    TSpeechDMessage *hist_msg;

    /* Check function parameters */
    if (new == NULL) return -1;
    if (new->buf == NULL) return -1;
    if (strlen(new->buf) < 1) return -1;
	
    /* Find settings for this particular client */
    if (fd>0){
        settings = get_client_settings_by_fd(fd);
        if (settings == NULL)
            FATAL("Couldn't find settings for active client, internal error.");
    }else if (fd<0){
        settings = get_client_settings_by_uid(-fd);
    }else{
        if (SPEECHD_DEBUG) FATAL("fd == 0, this shouldn't happen...");
        return -1;
    }

    /* Copy the settings to the new to-be-queued element */
    new->settings = *settings;
    new->settings.type = type;
    new->settings.output_module = (char*) spd_malloc(strlen(settings->output_module) + 1);
    new->settings.language = (char*) spd_malloc( strlen(settings->language) + 1);
    new->settings.client_name = (char*) spd_malloc( strlen(settings->client_name) + 1);
    strcpy(new->settings.output_module, settings->output_module);
    strcpy(new->settings.language, settings->language);
    strcpy(new->settings.client_name, settings->client_name);
    new->settings.reparted = reparted;

    MSG(5, "Queueing message |%s| with priority %d", new->buf, settings->priority);

    /* And we set the global id (note that this is really global, not
     * depending on the particular client, but unique) */
    last_message_id++;				
    new->id = last_message_id;

    /* Put the element new to queue according to it's priority. */
    switch(settings->priority){
    case 1: MessageQueue->p1 = g_list_append(MessageQueue->p1, new); 
        speechd_alarm(0);
        break;
    case 2: MessageQueue->p2 = g_list_append(MessageQueue->p2, new);
        speechd_alarm(0);
        break;
    case 3: MessageQueue->p3 = g_list_append(MessageQueue->p3, new);        
        speechd_alarm(0);
        break;
    case 4: MessageQueue->p4 = g_list_append(MessageQueue->p4, new);        
        speechd_alarm(0);
        break;
    case 5: MessageQueue->p5 = g_list_append(MessageQueue->p5, new);        
        last_p5_message = (TSpeechDMessage*) history_list_new_message(new);
        speechd_alarm(2000);
        break;

    default: FATAL("Nonexistent priority given");
    }
    sem_post(sem_messages_waiting);

    /* If desired, put the message also into history */
    if (history_flag){
        /* We will make an exact copy of the message for inclusion into history. */
        hist_msg = (TSpeechDMessage*) history_list_new_message(new); 
        if(hist_msg != NULL){
            message_history = g_list_append(message_history, hist_msg);
        }else{
            if(SPEECHD_DEBUG) FATAL("Can't include message into history\n");
        }
    }

    return 0;
}

/* Queue more messages in a list.
 *
 * Parameters:
 *     msg_list -- list of texts of the messages that contain
 *     it's text in msg->buf and it's type in msg->settings.type          
 *     (for the other parameters please see queue_message()
 * It returns 0 on success, -1 otherwise 
 */
int
queue_messages(GList* msg_list, int fd, int history_flag, int reparted)
{
    GList *gl;
    TSpeechDMessage *msg;
    int i, len;

    len = g_list_length(msg_list)-1;
    gl = g_list_first(msg_list);

    for (i=0; i <= len; i++){
        if (gl == NULL) break;
        if (gl->data == NULL){
            MSG(4,"WARNING: skipping blank text in queue_messages");
            continue;
        }
        
        msg = gl->data;
        queue_message(msg, fd, history_flag, msg->settings.type, reparted);
        gl = g_list_next(gl);
    }
}

/* Switch data mode on for the particular client. */
int
server_data_on(int fd)
{
    /* Mark this client as ,,sending data'' */
    awaiting_data[fd] = 1;
    /* Create new output buffer */
    o_buf[fd] = g_string_new("\0");
    assert(o_buf[fd] != NULL);
    assert(o_buf[fd]->str != NULL);
    MSG(4, "Switching to data mode...");
    return 0;
}

/* Switch data mode of for the particular client. */
void
server_data_off(int fd)
{
    assert (o_buf[fd] != NULL);

    o_bytes[fd] = 0;
    g_string_free(o_buf[fd],1);
    o_buf[fd] = NULL;
}

/* Serve the client on _fd_ if we got some activity. */
int
serve(int fd)
{
    int bytes;              /* Number of bytes we got */
    char buf[BUF_SIZE];     /* The content (commands or data) we got */
    char *reply;            /* Reply to the client */
    int ret;
 
    /* Read data from socket */
    bytes = read(fd, buf, BUF_SIZE);
    if(bytes == -1) return -1;
    buf[bytes] = 0;
    MSG(4,"Read %d bytes from client on fd %d", bytes, fd);

    /* Parse the data and read the reply*/
    MSG(5, "DATA:|%s|", buf);
    reply = parse(buf, bytes, fd);

    /* Send the reply to the socket */
    if (strlen(reply) == 0) return 0;
    if(reply[0] != '9'){        /* Don't reply to data etc. */
        ret = write(fd, reply, strlen(reply));
        if (ret == -1) return -1;	
    }

    return 0;
}


