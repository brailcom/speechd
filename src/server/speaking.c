
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
  * $Id: speaking.c,v 1.13 2003-05-28 23:19:18 hanke Exp $
  */

#include <glib.h>
#include <speechd.h>
#include "speaking.h"

/*
  Speak() is responsible for getting right text from right
  queue in right time and saying it loud through corresponding
  synthetiser.  This runs in a separate thread.
*/
void* 
speak(void* data)
{
    TSpeechDMessage *element = NULL;
    OutputModule *output;
    GList *gl = NULL;
    char *buffer;
    int ret;
    sigset_t all_signals;
    TSpeechDMessage *msg;

    ret = sigfillset(&all_signals);
    if (ret == 0){
        ret = pthread_sigmask(SIG_BLOCK, &all_signals, NULL);
        if (ret != 0) MSG(1, "Can't set signal set, expect problems when terminating!");
    }else{
        MSG(1, "Can't fill signal set, expect problems when terminating!");
    }

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	
    while(1){
        usleep(10);
        if (msgs_to_say>0){
            MSG(1,"Looking for messages");
            /* Look what is the highest priority of waiting
             * messages and take the desired actions on other
             * messages */
            if(g_list_length(MessageQueue->p1) != 0){
                stop_p3();	
                if (is_sb_speaking() && (highest_priority != 1))
                    stop_speaking_active_module();
            }
		    
            if(g_list_length(MessageQueue->p2) != 0){
                stop_p3();
            }

            if(g_list_length(MessageQueue->p3) != 0){
                /* Stop all other priority 3 messages but leave the first */
                gl = g_list_last(MessageQueue->p3); 
                
                assert(gl!=NULL);
                assert(gl->data != NULL);

                msg = (TSpeechDMessage*) gl->data;
                if (msg->settings.reparted <= 0){                
                    MessageQueue->p3 = g_list_remove_link(MessageQueue->p3, gl);
                    stop_p3();
                    /* Fill the queue with the list containing only the first message */
                    MessageQueue->p3 = gl;
                }

            }
            MSG(1,"Looking for messages 2");
            /* Check if sb is speaking or they are all silent. 
             * If some synthesizer is speaking, we must wait. */
            if (is_sb_speaking() == 1){
                MSG(1, "sb speaking");
                usleep(5);
                continue;
            }
            MSG(1,"before mutex");
            pthread_mutex_lock(&element_free_mutex);
            MSG(1,"after mutex");
            element = NULL;
            gl = NULL;

            if(SPEECHD_DEBUG){
                if( (g_list_length(MessageQueue->p1) == 0) &&
                    (g_list_length(MessageQueue->p2) == 0) &&
                    (g_list_length(MessageQueue->p3) == 0))
                    {
                        FATAL("All queues empty but msgs_to_say > 0\n");
                    }
            }

            MSG(1,"Looking for messages 3");
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
                        if (SPEECHD_DEBUG) FATAL("Descending through queues but all of them are empty");
                        pthread_mutex_unlock(&element_free_mutex);
                        continue;
                    }
                }
            } 
	            MSG(1,"Looking for messages 4");
            /* If we got here, we have some message to say. */
            element = (TSpeechDMessage *) gl->data;
            if (element == NULL){
                if(SPEECHD_DEBUG) FATAL("Non-NULL element containing NULL data found!\n");
                MessageQueue->p1 = g_list_delete_link(MessageQueue->p1, gl);
                MessageQueue->p2 = g_list_delete_link(MessageQueue->p2, gl);
                MessageQueue->p3 = g_list_delete_link(MessageQueue->p3, gl);
                pthread_mutex_unlock(&element_free_mutex);
                continue;
            }
     
            /* Isn't the parent client of this message paused? 
             * If it is, insert the message to the MessagePausedList. */
            assert(element != NULL);
            if (message_nto_speak(element, NULL, NULL)){
                if(element->settings.priority != 3){
                    MSG(4, "Inserting message to paused list...");
                    MessagePausedList = g_list_append(MessagePausedList, element);
                }else{
                    mem_free_message(element);
                }
                msgs_to_say--;
                pthread_mutex_unlock(&element_free_mutex);
                continue;
            }

            /* Determine which output module should be used */
            if (element->settings.type != MSGTYPE_SOUND){
                output = g_hash_table_lookup(output_modules, element->settings.output_module);
                if(output == NULL){
                    MSG(4,"Didn't find prefered output module, using default");
                    output = g_hash_table_lookup(output_modules, GlobalFDSet.output_module); 
                    if (output == NULL) MSG(2,"Can't find default output module!");
                }
            }else{
                output = sound_module;
                if (output == NULL) MSG(2,"Can't find sound module!");
            }

            if (output == NULL){
                mem_free_message(element);
                msgs_to_say--;
                pthread_mutex_unlock(&element_free_mutex);
                continue;				
            }                    

            if(element->settings.type == MSGTYPE_TEXT){
                MSG(4, "Processing message...");

                buffer = (char*) process_message(element->buf, element->bytes, &(element->settings));
                if (buffer == NULL){
                    mem_free_message(element);
                    msgs_to_say--;
                    pthread_mutex_unlock(&element_free_mutex);
                    continue;
                }

                if (buffer == NULL){
                    pthread_mutex_unlock(&element_free_mutex);
                    continue;
                }
                if (strlen(buffer) <= 0){
                    pthread_mutex_unlock(&element_free_mutex);
                    continue;
                }
            }else{
                MSG(4, "Passing message as it is...");
                MSG(5, "Message text: |%s|", element->buf);
                buffer = element->buf;
            }

            assert(buffer!=NULL);
	
            /* Set the speking_module monitor so that we knew who is speaking */
            speaking_module = output;
            speaking_uid = element->settings.uid;
            /* Write the data to the output module. (say them aloud) */
            ret = (*output->write) (buffer, strlen(buffer), &element->settings); 
            if (ret <= 0) MSG(2, "Output module failed");
            mem_free_message(element);
            msgs_to_say--;
            pthread_mutex_unlock(&element_free_mutex);
        }
    }	 
}

/* TODO: Needs some refactoring */
void
speaking_stop(int uid)
{
    TSpeechDMessage* msg;
    GList *gl;
    GList *queue;
    signed int gid = -1;

    if(get_speaking_client_uid() == uid){
        stop_speaking_active_module();

        if (highest_priority == 1) queue = MessageQueue->p1;
        if (highest_priority == 2) queue = MessageQueue->p2;
        if (highest_priority == 3) queue = MessageQueue->p3;
        if (queue == NULL) return;

        gl = g_list_last(queue);
        if (gl == NULL) return;
        if (SPEECHD_DEBUG) assert(gl->data != NULL);

        msg = (TSpeechDMessage*) gl->data;
        if ((msg->settings.reparted != 0) && (msg->settings.uid == uid)){
            gid = msg->settings.reparted;           
        }

        while(1){
            gl = g_list_last(queue);
            if (gl == NULL){
                if (highest_priority == 1) MessageQueue->p1 = queue;
                if (highest_priority == 2) MessageQueue->p2 = queue;
                if (highest_priority == 3) MessageQueue->p3 = queue;
                return;
            }
            if (SPEECHD_DEBUG) assert(gl->data != NULL);

            msg = (TSpeechDMessage*) gl->data;

            if ((msg->settings.reparted == gid) && (msg->settings.uid == uid)){
                queue = g_list_remove_link(queue, gl);
                msgs_to_say--;
            }else{
                if (highest_priority == 1) MessageQueue->p1 = queue;
                if (highest_priority == 2) MessageQueue->p2 = queue;
                if (highest_priority == 3) MessageQueue->p3 = queue;
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

    stop_speaking_active_module();

    if (highest_priority == 1) queue = MessageQueue->p1;
    if (highest_priority == 2) queue = MessageQueue->p2;
    if (highest_priority == 3) queue = MessageQueue->p3;
    if (queue == NULL) return;

    gl = g_list_last(queue);
    if (gl == NULL) return;
    if (SPEECHD_DEBUG) assert(gl->data != NULL);
    msg = (TSpeechDMessage*) gl->data;

    if (msg->settings.reparted != 0){
        gid = msg->settings.reparted;
    }

    while(1){
        gl = g_list_last(queue);
        if (gl == NULL){
            if (highest_priority == 1) MessageQueue->p1 = queue;
            if (highest_priority == 2) MessageQueue->p2 = queue;
            if (highest_priority == 3) MessageQueue->p3 = queue;
            return;
        }
        if (SPEECHD_DEBUG) assert(gl->data != NULL);

        msg = (TSpeechDMessage*) gl->data;
        if (msg->settings.reparted == 1){
            queue = g_list_remove_link(queue, gl);
            msgs_to_say--;
        }else{
            if (highest_priority == 1) MessageQueue->p1 = queue;
            if (highest_priority == 2) MessageQueue->p2 = queue;
            if (highest_priority == 3) MessageQueue->p3 = queue;
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
    stop_speaking_active_module();
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

    /* Find settings for this particular client */
    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
    settings->paused = 1;     

    if (is_sb_speaking() == 0) return 0;
    if (speaking_uid != uid) return 0;    

    msg_rest = (*speaking_module->pause) ();
    if (msg_rest == NULL) return 0;

    new = (TSpeechDMessage*) spd_malloc(sizeof(TSpeechDMessage));
    new->bytes = strlen(msg_rest);
    new->buf = msg_rest;
    assert(new->bytes >= 0);
    assert(new->buf != NULL);
    new->buf[new->bytes] = 0;

    if(queue_message(new, fd, 0, MSGTYPE_TEXT, 0) != 0){
        if(SPEECHD_DEBUG) FATAL("Can't queue message\n");
        free(new->buf);
        free(new);
        return 1;
    }

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

    /* Is there any message after resume? */
    if(g_list_length(MessagePausedList) != 0){
        while(1){
            gl = g_list_find_custom(MessagePausedList, (void*) NULL, p_msg_nto_speak);
            if (gl != NULL){
                element = (TSpeechDMessage*) gl->data;
                MessageQueue->p2 = g_list_append(MessageQueue->p2, element);
                MessagePausedList = g_list_remove_link(MessagePausedList, gl);
                msgs_to_say++;
            }else{
                break;
            }
        }
    }

    return 0;
}

/* Stops speaking and cancels currently spoken message.*/
int
is_sb_speaking()
{
    int speaking = 0;

    /* Determine is the current module is still speaking */
    if(speaking_module != NULL){
        speaking = (*speaking_module->is_speaking) ();
    } 
    if (speaking == 0) speaking_module = NULL;
    return speaking;
}

int
get_speaking_client_uid()
{
    int speaking;
    if(is_sb_speaking() == 0){
        speaking_uid = 0;
        return 0;
    }
    if(speaking_uid != 0){
        speaking = speaking_uid;
    } 
    return speaking;
}

void
stop_speaking_active_module()
{
    if (speaking_module == NULL) return;
    if (is_sb_speaking()){
        (*speaking_module->stop) ();
    }
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

int
stop_priority(int priority)
{
    int num, i;
    GList *gl;
    GList *queue;

    switch(priority){
    case 1:	queue = MessageQueue->p1; break;
    case 2:	queue = MessageQueue->p2; break;
    case 3:	queue = MessageQueue->p3; break;
    default: return -1;
    }

    if (highest_priority == priority){
        stop_speaking_active_module();
    }

    num = g_list_length(queue);
    for(i=0;i<=num-1;i++){
        gl = g_list_first(queue);
        assert(gl != NULL);
        assert(gl->data != NULL);
        mem_free_message(gl->data);
        queue = g_list_delete_link(queue, gl);
        msgs_to_say--;
    }

    switch(priority){
    case 1:	MessageQueue->p1 = queue; break;
    case 2:	MessageQueue->p2 = queue; break;
    case 3:	MessageQueue->p3 = queue; break;
    default: return -1;
    }

    return 0;
}

void
stop_from_uid(int uid)
{
    GList *gl;

    pthread_mutex_lock(&element_free_mutex);
    while(gl = g_list_find_custom(MessageQueue->p1, &uid, p_msg_uid_lc)){
        if(gl->data != NULL) mem_free_message(gl->data);
        MessageQueue->p1 = g_list_delete_link(MessageQueue->p1, gl);
        msgs_to_say--;
    }
    while(gl = g_list_find_custom(MessageQueue->p2, &uid, p_msg_uid_lc)){
        if(gl->data != NULL) mem_free_message(gl->data);
        MessageQueue->p2 = g_list_delete_link(MessageQueue->p2, gl);
        msgs_to_say--;
    }	
    while(gl = g_list_find_custom(MessageQueue->p3, &uid, p_msg_uid_lc)){
        if(gl->data != NULL) mem_free_message(gl->data);
        MessageQueue->p3 = g_list_delete_link(MessageQueue->p3, gl);
        msgs_to_say--;
    }	
    pthread_mutex_unlock(&element_free_mutex);
}
