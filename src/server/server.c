
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
  * $Id: server.c,v 1.63 2003-10-21 22:58:09 hanke Exp $
  */

#include "speechd.h"
#include "set.h"
#include "speaking.h"

int last_message_id = -1;

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

#define COPY_SET_STR(name) \
    new->settings.name = (char*) spd_strdup(settings->name);

int
queue_message(TSpeechDMessage *new, int fd, int history_flag,
              EMessageType type, int reparted)
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

    MSG(5, "In queue_message desired output module is %s", settings->output_module);

    /* Copy the settings to the new to-be-queued element */
    new->settings = *settings;
    new->settings.type = type;
    new->settings.index_mark = -1;
    COPY_SET_STR(language);
    COPY_SET_STR(client_name);
    COPY_SET_STR(output_module);

    new->settings.reparted = reparted;

    MSG(5, "Queueing message |%s| with priority %d", new->buf, settings->priority);

    /* And we set the global id (note that this is really global, not
     * depending on the particular client, but unique) */
    last_message_id++;				
    new->id = last_message_id;

    /* If desired, put the message also into history */
    /* NOTE: This should be before we put it into queues() to
     avoid conflicts with the other thread (it could delete
     the message before we woud copy it) */
    if (history_flag){
        /* We will make an exact copy of the message for inclusion into history. */
        hist_msg = (TSpeechDMessage*) spd_message_copy(new); 

        pthread_mutex_lock(&element_free_mutex);
        if(hist_msg != NULL){
            /* Do the necessary expiration of old messages*/
            if (g_list_length(message_history) >= MaxHistoryMessages){
                GList *gl;
                MSG(5, "Discarding older history message, limit reached");
                gl = g_list_first(message_history);
                if (gl != NULL){
                    message_history = g_list_remove_link(message_history, gl);
                    if (gl->data != NULL)
                        mem_free_message(gl->data);
                }
            }
            /* Save the message into history */
            message_history = g_list_append(message_history, hist_msg);
        }else{
            if(SPEECHD_DEBUG) FATAL("Can't include message into history\n");
        }
        pthread_mutex_unlock(&element_free_mutex);
    }

    pthread_mutex_lock(&element_free_mutex);
    /* Put the element new to queue according to it's priority. */
    switch(settings->priority){
    case 1: MessageQueue->p1 = g_list_append(MessageQueue->p1, new); 
        break;
    case 2: MessageQueue->p2 = g_list_append(MessageQueue->p2, new);
        break;
    case 3: MessageQueue->p3 = g_list_append(MessageQueue->p3, new);        
        break;
    case 4: MessageQueue->p4 = g_list_append(MessageQueue->p4, new);        
        break;
    case 5: MessageQueue->p5 = g_list_append(MessageQueue->p5, new);        
        last_p5_message = (TSpeechDMessage*) spd_message_copy(new);
        break;
    default: FATAL("Nonexistent priority given");
    }

    /* Look what is the highest priority of waiting
     * messages and take the desired actions on other
     * messages */
    /* TODO: Do the entire resolve_priorities() here is certainly
    not the best approach possible. Especially the part that
    calls output_stop() should be moved to speaking.c speak()
    function in future */
    resolve_priorities(settings->priority);
    pthread_mutex_unlock(&element_free_mutex);

    speaking_semaphore_post();

    return 0;
}
#undef COPY_SET_STR

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
    size_t bytes;           /* Number of bytes we got */
    char buf[BUF_SIZE+1];   /* The content (commands or data) we got */
    char *reply;            /* Reply to the client */
    int ret;
 
    /* Read data from socket */
    bytes = read(fd, buf, BUF_SIZE-1);
    if(bytes == -1) return -1;
    buf[bytes] = 0;
    MSG(4,"Read %d bytes from client on fd %d", bytes, fd);

    /* Parse the data and read the reply*/
    MSG2(5, "protocol", "DATA:|%s|", buf);
    reply = parse(buf, bytes, fd);

    if (reply == NULL) FATAL("Internal error, reply from parse() is NULL!");

    /* Send the reply to the socket */
    if (strlen(reply) == 0) return 0;
    if(reply[0] != '9'){        /* Don't reply to data etc. */
        MSG2(5, "protocol", "REPLY:|%s|", reply);
        ret = write(fd, reply, strlen(reply));
        if (ret == -1) return -1;	
    }

    return 0;
}


