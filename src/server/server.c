
 /*
  * server.c - Speech Dispatcher server core
  * 
  * Copyright (C) 2001,2002,2003, 2004 Brailcom, o.p.s
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
  * $Id: server.c,v 1.76 2005-12-07 08:22:52 hanke Exp $
  */

#include "speechd.h"
#include "set.h"
#include "speaking.h"

int last_message_id = 0;

/* Put a message into its queue.
 *
 * Parameters:
 *   new -- the (allocated) message structure to queue, it must contain
 *          the text to be queued in new->buf
 *   fd  -- file descriptor of the calling client (positive)       
 *          unique id if the client is gone (negative) -- in this
 *          case it means we are reloading the message and the
 *          behavior is slightly different
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
    int id;


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

    if (fd > 0){
	new->settings.index_mark = NULL;
	COPY_SET_STR(client_name);
	COPY_SET_STR(output_module);
	COPY_SET_STR(language);

	/* And we set the global id (note that this is really global, not
	 * depending on the particular client, but unique) */
	last_message_id++;				
	new->id = last_message_id;
	new->time = time(NULL);

	new->settings.paused_while_speaking = 0;
    }
    id = new->id;
          
    new->settings.reparted = reparted;

    MSG(5, "Queueing message |%s| with priority %d", new->buf, settings->priority);

    /* If desired, put the message also into history */
    /* NOTE: This should be before we put it into queues() to
     avoid conflicts with the other thread (it could delete
     the message before we woud copy it) */
    //    if (history_flag){
    if (0){
        /* We will make an exact copy of the message for inclusion into history. */
        hist_msg = (TSpeechDMessage*) spd_message_copy(new); 

        pthread_mutex_lock(&element_free_mutex);
        if(hist_msg != NULL){
            /* Do the necessary expiration of old messages*/
            if (g_list_length(message_history) >= SpeechdOptions.max_history_messages){
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
	mem_free_message(last_p5_message);
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

    MSG(5, "Message inserted into queue.");

    return id;
}
#undef COPY_SET_STR

/* Switch data mode on for the particular client. */
int
server_data_on(int fd)
{
    /* Mark this client as ,,sending data'' */
    SpeechdSocket[fd].awaiting_data = 1;
    /* Create new output buffer */
    SpeechdSocket[fd].o_buf = g_string_new("");
    MSG(4, "Switching to data mode...");
    return 0;
}

/* Switch data mode of for the particular client. */
void
server_data_off(int fd)
{
    assert(SpeechdSocket[fd].o_buf != NULL);

    SpeechdSocket[fd].o_bytes = 0;
    g_string_free(SpeechdSocket[fd].o_buf,1);
    SpeechdSocket[fd].o_buf = NULL;
}

/* Serve the client on _fd_ if we got some activity. */
int
serve(int fd)
{
    char *reply;            /* Reply to the client */
    int ret;
    
    {
      size_t bytes = 0;       /* Number of bytes we got */
      int buflen = BUF_SIZE;
      char *buf = (char *)spd_malloc (buflen + 1);
      
      /* Read data from socket */
      /* Read exactly one complete line, the `parse' routine relies on it */
      {
        while (1)
          {
            int n = read (fd, buf+bytes, 1);
            if (n <= 0)
              {
                spd_free (buf);
                return -1;
              }
            if (buf[bytes] == '\n')
              {
                buf[++bytes] = '\0';
                break;
              }
	    if (buf[bytes] == '\0') buf[bytes] = '?';
            if ((++bytes) == buflen)
              {
                buflen *= 2;
                buf = spd_realloc (buf, buflen + 1);
              }
          }
      }
      
      /* Parse the data and read the reply*/
      MSG2(5, "protocol", "%d:DATA:|%s| (%d)", fd, buf, bytes);
      reply = parse(buf, bytes, fd);
      spd_free(buf);
    }

    if (reply == NULL) FATAL("Internal error, reply from parse() is NULL!");

    /* Send the reply to the socket */
    if (strlen(reply) == 0){
	spd_free(reply);
	return 0;
    }
    if(reply[0] != '9'){        /* Don't reply to data etc. */
        pthread_mutex_lock(&socket_com_mutex);	
        MSG2(5, "protocol", "%d:REPLY:|%s|", fd, reply);
        ret = write(fd, reply, strlen(reply));
	spd_free(reply);
        pthread_mutex_unlock(&socket_com_mutex);
        if (ret == -1){
	    MSG(5, "write() error: %s", strerror(errno));
	    return -1;
	}
    }else{
	spd_free(reply);
    }

    return 0;
}


