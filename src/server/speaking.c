
 /*
  * speaking.c - Speech Dispatcher speech output functions
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
  * $Id: speaking.c,v 1.34 2003-10-12 23:34:25 hanke Exp $
  */

#include <glib.h>
#include "speechd.h"
#include "index_marking.h"
#include "module.h"
#include "set.h"

#include "speaking.h"

TSpeechDMessage *current_message = NULL;

/*
  Speak() is responsible for getting right text from right
  queue in right time and saying it loud through the corresponding
  synthetiser.  This runs in a separate thread.
*/
void* 
speak(void* data)
{
    TSpeechDMessage *message;
    OutputModule *output;
    GList *gl = NULL;
    char *buffer;
    int ret;
    TSpeechDMessage *msg;

    /* Block all signals and set thread states */
    set_speak_thread_attributes();

    while(1){
        speaking_semaphore_wait();

        /* Handle pause requests */
        if (pause_requested){
            MSG(4, "Trying to pause...");
            if(pause_requested == 1)
                speaking_pause_all(pause_requested_fd);
            if(pause_requested == 2)
                speaking_pause(pause_requested_fd, pause_requested_uid);
            MSG(4, "Paused...");
            pause_requested = 0;
            resume_requested = 1;
            continue;
        }
        
        /* Handle resume requests */
        if (resume_requested){
            GList *gl;
            TSpeechDMessage *element;
            pthread_mutex_lock(&element_free_mutex);

            /* Is there any message after resume? */
            if(g_list_length(MessagePausedList) != 0){
                while(1){
                    gl = g_list_find_custom(MessagePausedList, (void*) NULL, message_nto_speak);
                    if ((gl != NULL) && (gl->data != NULL)){
                        reload_message((TSpeechDMessage*) gl->data);
                        MessagePausedList = g_list_remove_link(MessagePausedList, gl);
                    }else break;                    
                }
            }
            pthread_mutex_unlock(&element_free_mutex);
            resume_requested = 0;
        }
        
        /* Look what is the highest priority of waiting
         * messages and take the desired actions on other
         * messages */
        resolve_priorities();

        /* Check if sb is speaking or they are all silent. 
         * If some synthesizer is speaking, we must wait. */
        if (is_sb_speaking() == 1){
            continue;
        }

        pthread_mutex_lock(&element_free_mutex);
               
        /* Handle postponed priority progress message */
        if (highest_priority == 5 && (last_p5_message != NULL) 
            && (g_list_length(MessageQueue->p5) == 0)){
            message = last_p5_message;
            last_p5_message = NULL;
            highest_priority = 2;
            speaking_semaphore_post();
        }else{
            /* Extract the right message from priority queue */
            message = get_message_from_queues();
            if (message == NULL){
                pthread_mutex_unlock(&element_free_mutex);
                continue;     
            }
        }    

        /* Isn't the parent client of this message paused? 
         * If it is, insert the message to the MessagePausedList. */
        if (message_nto_speak(message, NULL)){
            MSG(4, "Inserting message to paused list...");
            MessagePausedList = g_list_append(MessagePausedList, message);              
            pthread_mutex_unlock(&element_free_mutex);
            continue;
        }

        /* Insert index marks into textual messages */
        if(message->settings.type == MSGTYPE_TEXT){
            insert_index_marks(message);
        }

        /* Write the message to the output layer. */
        ret = output_speak(message);
        MSG(4,"Message sent to output module");
        if (ret == -1) MSG(2, "Error: Output module failed");

        /* Set the id of the client who is speaking. */
        speaking_uid = message->settings.uid;

        /* Save the currently spoken message to be able to resume()
           it after pause */
        if (current_message != NULL) mem_free_message(current_message);
        current_message = message;

        /* Check if the last priority 5 message wasn't said yet */
        if (last_p5_message != NULL){
            if (last_p5_message->settings.uid == message->settings.uid)
                last_p5_message = NULL;
        }

        pthread_mutex_unlock(&element_free_mutex);        
    }	 
}

int
reload_message(TSpeechDMessage *msg)
{
    TFDSetElement *client_settings;
    int im;
    char *pos;
    char *newtext;

    if (msg == NULL) return -1;
    if (msg->settings.index_mark >= 0){
        client_settings = get_client_settings_by_uid(msg->settings.uid);
        /* Scroll back to provide context, if required */
        /* WARNING: This relies on ordered index marks! */
        im = msg->settings.index_mark + client_settings->pause_context;
        MSG(5, "index_marking", "Requested index mark is %d (%d+%d)",im,
            msg->settings.index_mark, client_settings->pause_context);
        if (im < 0){
            im = 0;
            pos = msg->buf;
        }else{
            pos = find_index_mark(msg, im);
            if (pos == NULL) return -1;
        }

        newtext = strip_index_marks(pos);
        spd_free(msg->buf);
        
        if (newtext == NULL) return -1;
        
        msg->buf = newtext;
        msg->bytes = strlen(msg->buf);

        if(queue_message(msg, -msg->settings.uid, 0, MSGTYPE_TEXT, 0) != 0){
            if(SPEECHD_DEBUG) FATAL("Can't queue message\n");
            spd_free(msg->buf);
            spd_free(msg);
            return -1;
        }

        return 0;
    }
}

void
speaking_stop(int uid)
{
    TSpeechDMessage* msg;
    GList *gl;
    GList *queue;
    signed int gid = -1;

    /* Only act if the currently speaking client is the specified one */
    if(get_speaking_client_uid() == uid){
        output_stop();

        /* Get the queue where the message being spoken came from */
        queue = speaking_get_queue(highest_priority);
        if (queue == NULL) return;

        /* Get group ID of the current message */
        gl = g_list_last(queue);
        if (gl == NULL) return;
        if (gl->data == NULL) return;

        msg = (TSpeechDMessage*) gl->data;
        if ((msg->settings.reparted != 0) && (msg->settings.uid == uid)){
            gid = msg->settings.reparted;           
        }else{
            return;
        }

        while(1){
            gl = g_list_last(queue);
            if (gl == NULL){
                speaking_set_queue(highest_priority, queue);
                return;
            }
            if (gl->data == NULL) return;

            msg = (TSpeechDMessage*) gl->data;

            if ((msg->settings.reparted == gid) && (msg->settings.uid == uid)){
                queue = g_list_remove_link(queue, gl);
                assert(gl->data != NULL);
                mem_free_message(gl->data);
            }else{
                speaking_set_queue(highest_priority, queue);
                return;
            }
        }
    }
}        

void
speaking_stop_all()
{
    TSpeechDMessage *msg;
    GList *gl;
    GList *queue;
    int gid = -1;

    output_stop();

    queue = speaking_get_queue(highest_priority);
    if (queue == NULL) return;

    gl = g_list_last(queue);
    if (gl == NULL) return;
    assert(gl->data != NULL);
    msg = (TSpeechDMessage*) gl->data;

    if (msg->settings.reparted != 0){
        gid = msg->settings.reparted;
    }else{
        return;
    }

    while(1){
        gl = g_list_last(queue);
        if (gl == NULL){
            speaking_set_queue(highest_priority, queue);
            return;
        }
        if (SPEECHD_DEBUG) assert(gl->data != NULL);

        msg = (TSpeechDMessage*) gl->data;
        if (msg->settings.reparted == 1){
            queue = g_list_remove_link(queue, gl);
            assert(gl->data != NULL);
            mem_free_message(gl->data);            
        }else{
            speaking_set_queue(highest_priority, queue);
            return;
        }
    }
}

void
speaking_cancel(int uid)
{
    speaking_stop(uid);
    stop_from_uid(uid);
}

void
speaking_cancel_all()
{
    output_stop();
    stop_priority(1);
    stop_priority(2);
    stop_priority(3);
}

int
speaking_pause_all(int fd)
{
    int err, i;
    int uid;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += speaking_pause(i, uid);
    }

    if (err>0) return 1;
    else return 0;
}

int
speaking_pause(int fd, int uid)
{
    TFDSetElement *settings;
    TSpeechDMessage *new;
    char *msg_rest;
    char *pos;
    int i;
    int im;

    /* Find settings for this particular client */
    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
    settings->paused = 1;     

    if (is_sb_speaking() == 0) return 0;
    if (speaking_uid != uid) return 0;    

    im = output_pause();
    if (im == -1) return 0;
    if (current_message == NULL) return 0;
    
    current_message->settings.index_mark = im;
    MessagePausedList = g_list_append(MessagePausedList, current_message);
    current_message = NULL;

    return 0;  
}

int
speaking_resume_all()
{
    int err, i;
    int uid;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += speaking_resume(uid);
    }

    if (err>0) return 1;
    else return 0;
}

int
speaking_resume(int uid)
{
    TFDSetElement *settings;
    TSpeechDMessage *element;
    GList *gl;

    /* Find settings for this particular client */
    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
    /* Set it to speak again. */
    settings->paused = 0;
    
    resume_requested = 1;
    speaking_semaphore_post();
    
    return 0;
}

/* Stops speaking and cancels currently spoken message.*/
int
is_sb_speaking()
{
    int speaking = 0;

    /* Determine is the current module is still speaking */
    if(speaking_module != NULL){
        speaking = output_is_speaking();
    } 
    if (speaking == 0) speaking_module = NULL;
    return speaking;
}

int
get_speaking_client_uid()
{
    int speaking = 0;
    if(is_sb_speaking() == 0){
        speaking_uid = 0;
        return 0;
    }
    if(speaking_uid != 0){
        speaking = speaking_uid;
    } 
    return speaking;
}

int
stop_p23()
{
    int ret1, ret2;
    ret1 = stop_priority(2);
    ret2 = stop_priority(3);
    if((ret1 == -1) || (ret2 == -1)) return -1;
    else return 0;
}

int
stop_p3()
{
    int ret;
    ret = stop_priority(3);
    return ret;
}

GList*
empty_queue(GList *queue)
{
    int num, i;
    GList *gl;

    num = g_list_length(queue);
    for(i=0;i<=num-1;i++){
        gl = g_list_first(queue);
        assert(gl != NULL);
        assert(gl->data != NULL);
        mem_free_message(gl->data);
        queue = g_list_delete_link(queue, gl);
    }

    return queue;
}

int
stop_priority(int priority)
{
    int num, i;
    GList *gl;
    GList *queue;

    queue = speaking_get_queue(priority);

    if (highest_priority == priority){
        output_stop();
    }

    queue = empty_queue(queue);

    speaking_set_queue(priority, queue);

    return 0;
}



GList*
stop_priority_from_uid(GList *queue, const int uid){
    GList *ret = queue;
    GList *gl;

    while(gl = g_list_find_custom(ret, &uid, p_msg_uid_lc)){
        if(gl->data != NULL) mem_free_message(gl->data);
        ret = g_list_delete_link(ret, gl);
    }

    return ret;
}

void
stop_from_uid(const int uid)
{
    GList *gl;

    pthread_mutex_lock(&element_free_mutex);
    MessageQueue->p1 = stop_priority_from_uid(MessageQueue->p1, uid);
    MessageQueue->p2 = stop_priority_from_uid(MessageQueue->p2, uid);
    MessageQueue->p3 = stop_priority_from_uid(MessageQueue->p3, uid);
    MessageQueue->p4 = stop_priority_from_uid(MessageQueue->p4, uid);
    MessageQueue->p5 = stop_priority_from_uid(MessageQueue->p5, uid);
    pthread_mutex_unlock(&element_free_mutex);
}

/* Determines if this messages is to be spoken
 * (returns 1) or it's parent client is paused (returns 0).
 * Note: If you are wondering why it's reversed (not to speak instead
 * of to speak), it's because we also use this function for
 * searching through the list. */
static gint
message_nto_speak(gconstpointer data, gconstpointer nothing)
{
    TFDSetElement *global_settings;
    GList *gl;
    TSpeechDMessage *message = (TSpeechDMessage *) data;

    /* Is there something in the body of the message? */
    if(message == NULL) return 0;

    /* Find global settings for this connection. */	
    global_settings = get_client_settings_by_fd(message->settings.fd);
    if (global_settings == NULL) return 0;	
 
    if (!global_settings->paused) return 0;
    else return 1;
}

static void
set_speak_thread_attributes()
{
    int ret;
    sigset_t all_signals;

    ret = sigfillset(&all_signals);
    if (ret == 0){
        ret = pthread_sigmask(SIG_BLOCK, &all_signals, NULL);
        if (ret != 0) MSG(1, "Can't set signal set, expect problems when terminating!");
    }else{
        MSG(1, "Can't fill signal set, expect problems when terminating!");
    }

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}

void 
stop_priority_except_first(int priority)
{    
    GList *queue;
    GList *gl;
    TSpeechDMessage *msg;
    GList *gl_next;
    int gid;

    queue = speaking_get_queue(priority);

    gl = g_list_last(queue); 
                
    if (gl == NULL) return;  
    if (gl->data == NULL) return;

    msg = (TSpeechDMessage*) gl->data;
    if (msg->settings.reparted <= 0){                
        queue = g_list_remove_link(queue, gl);
        speaking_set_queue(priority, queue);        

        stop_priority(priority);
        /* Fill the queue with the list containing only the first message */
        speaking_set_queue(priority, gl);
    }else{
        gid = msg->settings.reparted;

        if (highest_priority == priority && speaking_gid != gid){
            output_stop();
        }

        gl = g_list_first(queue);        
        while(gl){
            gl_next = g_list_next(gl);
            if (gl->data != NULL){
                TSpeechDMessage *msgg = gl->data;
                if (msgg->settings.reparted != gid){
                    queue = g_list_remove_link(queue, gl);
                    mem_free_message(msgg);
                }
            }
            gl = gl_next;
        }
        speaking_set_queue(priority, queue);
    }


    return;
}

static void
resolve_priorities()
{
    GList *gl;
    TSpeechDMessage *msg;

    if(g_list_length(MessageQueue->p1) != 0){
        if (highest_priority != 1) output_stop();
        stop_priority(4);
        stop_priority(5);
    }
		    
    if(g_list_length(MessageQueue->p2) != 0){
        stop_priority_except_first(2);
        stop_priority(4);
        stop_priority(5);
    }

    if(g_list_length(MessageQueue->p3) != 0){
        stop_priority(2);
        stop_priority(4);
        stop_priority(5);
    }

    if(g_list_length(MessageQueue->p4) != 0){
        stop_priority_except_first(4);
        if (is_sb_speaking()){
            if (highest_priority != 4)
                stop_priority(4);
        }
    }

    if(g_list_length(MessageQueue->p5) != 0){
        stop_priority(4);
        if (is_sb_speaking()){
            GList *gl;
            gl = g_list_last(MessageQueue->p5); 
            MessageQueue->p5 = g_list_remove_link(MessageQueue->p5, gl);
            if (gl != NULL){
                MessageQueue->p5 = empty_queue(MessageQueue->p5);
                if (gl->data != NULL){
                    MessageQueue->p5 = gl;
                }
            }
        }
    }

}

static TSpeechDMessage*
get_message_from_queues()
{
    GList *gl = NULL;

    /* We will descend through priorities to say more important
     * messages first. */
    gl = g_list_first(MessageQueue->p1); 
    if (gl != NULL){
        MSG(4,"Message in queue p1");
        MessageQueue->p1 = g_list_remove_link(MessageQueue->p1, gl);
        highest_priority = 1;
    }else{
        gl = g_list_first(MessageQueue->p2); 
        if (gl != NULL){
            MSG(4,"Message in queue p2");
            MessageQueue->p2 = g_list_remove_link(MessageQueue->p2, gl);
            highest_priority = 2;
        }else{
            gl = g_list_first(MessageQueue->p3); 
            if (gl != NULL){
                MSG(4,"Message in queue p3");
                MessageQueue->p3 = g_list_remove_link(MessageQueue->p3, gl);
                highest_priority = 3;
            }else{
                gl = g_list_first(MessageQueue->p4); 
                if (gl != NULL){
                    MSG(4,"Message in queue p4");
                    MessageQueue->p4 = g_list_remove_link(MessageQueue->p4, gl);
                    highest_priority = 4;
                }else{
                    gl = g_list_first(MessageQueue->p5);
                    if (gl != NULL){
                        MSG(4,"Message in queue p5");
                        MessageQueue->p5 = g_list_remove_link(MessageQueue->p5, gl);
                        highest_priority = 5;
                    }else{
                        return NULL;
                    }
                }
            }
        }
    } 
    assert(gl != NULL);
    return (TSpeechDMessage *) gl->data;
}

static GList*
speaking_get_queue(int priority)
{
    GList *queue = NULL;

    assert(priority > 0  &&  priority <= 5);

    switch(priority){           
    case 1: queue = MessageQueue->p1; break;
    case 2: queue = MessageQueue->p2; break;
    case 3: queue = MessageQueue->p3; break;
    case 4: queue = MessageQueue->p4; break;
    case 5: queue = MessageQueue->p5; break;
    }

    return queue;
}

static void
speaking_set_queue(int priority, GList *queue)
{
    assert(priority > 0  &&  priority <= 5);

    switch(priority){           
    case 1: MessageQueue->p1 = queue; break;
    case 2: MessageQueue->p2 = queue; break;
    case 3: MessageQueue->p3 = queue; break;
    case 4: MessageQueue->p4 = queue; break;
    case 5: MessageQueue->p5 = queue; break;
    }
}
