
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
  * $Id: speaking.c,v 1.18 2003-06-23 05:12:49 hanke Exp $
  */

#include <glib.h>
#include <speechd.h>
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

    /* Block all signals and sets thread states */
    set_speak_thread_attributes();
	
    while(1){
        sem_wait(sem_messages_waiting);

        if (pause_requested){
            MSG(4, "Trying to execute pause...");
            if(pause_requested == 1)
                speaking_pause_all(pause_requested_fd);
            if(pause_requested == 2)
                speaking_pause(pause_requested_fd, pause_requested_uid);
            MSG(4, "Pause execution terminated...");
            pause_requested = 0;
            continue;
        }

        /* Look what is the highest priority of waiting
         * messages and take the desired actions on other
         * messages */
        resolve_priorities();

        /* Check if sb is speaking or they are all silent. 
         * If some synthesizer is speaking, we must wait. */
        if (is_sb_speaking() == 1){
            sem_post(sem_messages_waiting);
            usleep(5);
            continue;
        }

        pthread_mutex_lock(&element_free_mutex);

        /* Extract the right message from priority queue */
        message = get_message_from_queues();
        if (message == NULL){
            if (highest_priority == 5){
                if (last_p5_message != NULL){
                    message = last_p5_message;
                    last_p5_message = NULL;
                    sem_trywait(sem_messages_waiting);
                }else{
                    pthread_mutex_unlock(&element_free_mutex);
                    continue;
                }
            }else{
                MSG(4, "Message got from queue is NULL");
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

        /* Determine which output module should be used */
        output = get_output_module(message);
        if (output == NULL){
            MSG(4, "Output module NULL...");
            mem_free_message(message);             
            pthread_mutex_unlock(&element_free_mutex);
            continue;				
        }                    

        /* Process message: spelling, punctuation, etc. */
        if(message->settings.type == MSGTYPE_TEXT){
            MSG(4, "Processing message...");

            buffer = (char*) process_message(message->buf, message->bytes, &(message->settings));
            if (buffer == NULL){
                mem_free_message(message);
                pthread_mutex_unlock(&element_free_mutex);
                continue;
            }

            if (strlen(buffer) <= 0){
                mem_free_message(message);
                pthread_mutex_unlock(&element_free_mutex);
                continue;
            }
        }else{
            MSG(4, "Passing message as it is...");
            MSG(5, "Message text: |%s|", message->buf);
            buffer = message->buf;
        }
        assert(buffer!=NULL);
	
        /* Set the speking-monitor so that we know who is speaking */
        speaking_module = output;
        speaking_uid = message->settings.uid;

        /* Write the data to the output module. (say them aloud) */
        ret = (*output->write) (buffer, strlen(buffer), &message->settings); 
        MSG(4,"Message sent to output module");
        if (ret <= 0) MSG(2, "Output module failed");

        /* Tidy up... */
        if (current_message != NULL) mem_free_message(current_message);
        current_message = message;

        pthread_mutex_unlock(&element_free_mutex);        
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
        stop_speaking_active_module();

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
                sem_trywait(sem_messages_waiting);
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

    stop_speaking_active_module();

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
            sem_trywait(sem_messages_waiting);
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
    size_t msg_pos;
    int i;

    /* Find settings for this particular client */
    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
    settings->paused = 1;     

    if (is_sb_speaking() == 0) return 0;
    if (speaking_uid != uid) return 0;    

    msg_pos = (*speaking_module->pause) ();
    if (msg_pos == 0) return 0;
    if (current_message == NULL) return 0;

    assert(msg_pos <= current_message->bytes);
    
    new = (TSpeechDMessage*) spd_malloc(sizeof(TSpeechDMessage));
    new->bytes = current_message->bytes - msg_pos;
    new->buf = (char*) spd_malloc((new->bytes + 2) * sizeof(char)); 
    MSG(1,"DBG TEMP MESSAGE: (%s) [%d]", current_message->buf, msg_pos);
    for (i=0; i<=new->bytes-1; i++){
        new->buf[i] = current_message->buf[i+msg_pos];
    }
    assert(new->bytes >= 0);
    assert(new->buf != NULL);
    new->buf[new->bytes] = 0;

    if(queue_message(new, fd, 0, MSGTYPE_TEXTP, 0) != 0){
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
            gl = g_list_find_custom(MessagePausedList, (void*) NULL, message_nto_speak);
            if (gl != NULL){
                element = (TSpeechDMessage*) gl->data;
                MessageQueue->p2 = g_list_append(MessageQueue->p2, element);
                MessagePausedList = g_list_remove_link(MessagePausedList, gl);
                sem_post(sem_messages_waiting);
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
        sem_trywait(sem_messages_waiting);
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
        stop_speaking_active_module();
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
        sem_trywait(sem_messages_waiting);
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

static char*
process_message_spell(char *buf, int bytes, TFDSetElement *settings, GHashTable *icons)
{
    int i;
    char *spelled_letter;
    char *character;
    char *pos;
    GString *str;
    char *new_message = NULL;
    int first_part = 1;
    GList *plist = NULL;
    int *sound;
    long int size;
    char *spelled;

    sound = (int*) spd_malloc(sizeof(int));

    str = g_string_sized_new(bytes);
    character = (char*) spd_malloc(8); /* 6 bytes should be enough, plus the trailing 0 */

    assert(settings->spelling_table != NULL);

    pos = buf;                  /* Set the position cursor for UTF8 to the beginning. */
    size = g_utf8_strlen(buf, -1) - 1;
    MSG(4,"Processing %d bytes", size);
    for(i=0; i <= size; i++){
        spd_utf8_read_char(pos, character);
        spelled_letter = (char*) snd_icon_spelling_get(settings->spelling_table,
                                                       icons, character, sound);
  		
        if(*sound == 1){
            /* If this is the first part of the message, save it
             * as return value. */
            if (first_part){
                first_part = 0;
                if (str->str != NULL)
                    if (strlen(str->str) > 0)
                        new_message = str->str;
            }else{              /* It's not the first part... */
                if (str->str != NULL){
                    if (strlen(str->str) > 0){
                        plist = msglist_insert(plist, str->str, MSGTYPE_TEXTP);
                    }
                }
            }
            /* Set up a new buffer */
            g_string_free(str, 0);
            str = g_string_new("");

            /* Add the icon to plist */
            if (spelled_letter != NULL){
                spelled = (char*) spd_malloc((strlen(spelled_letter) +1)  * sizeof(char)); 
                strcpy(spelled, spelled_letter);
                if (strlen(spelled_letter) > 0)
                    plist = msglist_insert(plist, spelled, MSGTYPE_SOUND);
            }else{
                assert(character != NULL);
                if (character[0] != '\r' && character[0] != '\n'){
                   MSG(4,"Using character verbatim...");
                   g_string_append(str, character);			
                }
            }

        }else{                  /* this icon is represented by a string */
            if (spelled_letter != NULL){
                g_string_append(str, spelled_letter);
            }else{
                assert(character!= NULL);
                if (character[0] != '\r' && character[0] != '\n'){
                   MSG(4,"Using character verbatim...");
                   g_string_append(str, character);
                }
            }
            g_string_append(str," ");
        }
        pos = g_utf8_find_next_char(pos, NULL); /* Skip to the next UTF8 character */
    }

    MSG(4,"Processing done...");

    /* Handle dle last part of the parsed message */
    if(first_part){	
        new_message = str->str;
    }else{   
        if (str != NULL)
            if (str->str != NULL)
                if (strlen(str->str) > 0)
                    plist = msglist_insert(plist, str->str, MSGTYPE_TEXTP);
    }
    g_string_free(str, 0);

    /* Queue the parts not returned. */
    if(plist != NULL){
        queue_messages(plist, -settings->uid, 0, ++max_gid);
        MSG(4, "Max gid set to %d", max_gid);
    }

    free(character);
    return new_message;
}

static char*
process_message_punctuation(char *buf, int bytes, TFDSetElement *settings, GHashTable *icons)
{
    int i;
    char *spelled_punct;
    char *character;
    char *pos;
    char *inside;
    gunichar u_char;
    int length;
    GString *str;
    char *new_message = NULL;
    int *sound;
    int first_part = 1;
    char *punct;
    long int size;
    GList *plist = NULL;

    sound = (int*) spd_malloc(sizeof(int));

    str = g_string_sized_new(bytes);
    character = (char*) spd_malloc(8); /* 6 bytes should be enough, plus the trailing 0 */

    assert(settings->punctuation_table != NULL);

    pos = buf;                  /* Set the position cursor for UTF8 to the beginning. */

    size = g_utf8_strlen(buf, -1);
    MSG(4,"Processing %d bytes", size);

    for(i=0; i <= size; i++){

        spd_utf8_read_char(pos, character);
        u_char = g_utf8_get_char(character);

        if (character != NULL){ 
            if (g_unichar_isprint(u_char)) g_string_append(str, character);
        }else{
            break;
        }

        if(g_unichar_ispunct(u_char)){ 
            if(settings->punctuation_mode == 2){ 
                inside = g_utf8_strchr(settings->punctuation_some,-1,u_char); 
                if (inside == NULL){
                    pos = g_utf8_find_next_char(pos, NULL); /* Skip to the next UTF8 character */
                    continue; 
                }
            } 

            spelled_punct = (char*) snd_icon_spelling_get(settings->punctuation_table, 
                                                          icons, character, sound); 

            if(*sound == 1){
                /* If this is the first part of the message, save it */
                /* as return value. */

                if (first_part){
                    first_part = 0; 
                    if (str != NULL) 
                        if (str->str != NULL) 
                            if (strlen(str->str) > 0) 
                                new_message = str->str; 
                }else{              /* It's not the first part... */ 
                    if (str->str != NULL){ 
                        if (strlen(str->str) > 0) 
                            plist = msglist_insert(plist, str->str, MSGTYPE_TEXTP);                         
                    } 
                } 
                /* Set up a new buffer */
                g_string_free(str, 0); 
                str = g_string_new(""); 

                /* Add the icon to plist */
                if (spelled_punct != NULL){
                    if (strlen(spelled_punct) > 0){
                        punct = (char*) spd_malloc((strlen(spelled_punct) +1)  * sizeof(char)); 
                        strcpy(punct, spelled_punct);
                        plist = msglist_insert(plist, punct, MSGTYPE_SOUND);           
                    }
                }
            }else{                  /* this icon is represented by a string */
                if (spelled_punct != NULL){ 
                    g_string_append_printf(str," %s, ", spelled_punct); 
                }
            } 
        }

        pos = g_utf8_find_next_char(pos, NULL); /* Skip to the next UTF8 character */		
    }

    MSG(4,"Processing done...");

    if(first_part){
        if (str != NULL)
            new_message = str->str;
        else
            new_message = NULL;
    }else{
        if (str != NULL)
            if (str->str != NULL)
                if (strlen(str->str) > 0)
                    plist = msglist_insert(plist, str->str, MSGTYPE_TEXTP);
    }
    g_string_free(str, 0);
    
    /* Queue the parts not returned. */
    if(plist != NULL){
        queue_messages(plist, -settings->uid, 0, ++max_gid);
    }

    free(character);
    return new_message;
}

static char*
process_message(char *buf, int bytes, TFDSetElement* settings)
{
    GHashTable *icons;
    char *new_message;

    if(settings->spelling || settings->punctuation_mode){
        icons = g_hash_table_lookup(snd_icon_langs, settings->language);
        if (icons == NULL){
            icons = g_hash_table_lookup(snd_icon_langs, GlobalFDSet.language);
            if (icons == NULL) return NULL;
        }
    
        if(settings->spelling){
            new_message = process_message_spell(buf, bytes, settings, icons);
        }
        else if(settings->punctuation_mode){
            new_message = process_message_punctuation(buf, bytes, settings, icons);
        }

        return new_message;
    }

    return buf;
}

/* Creates a new message from string STR and stores it
 * in a LIST of messages. This LIST is returned. This
 * is particularly useful for dividing messages
 * when processing punctuation in messages
 * (see process_message_punctuation())
 *
 * If LIST is NULL, new list is created 
 */
static GList*
msglist_insert(GList *list, char *str, EMessageType type)
{
    TSpeechDMessage *msg;
    GList *nlist;

    if (str == NULL){
        if (SPEECHD_DEBUG) FATAL("msglist_insert passed NULL in str");
        return list;
    }
    if (strlen (str) == 0){
        if (SPEECHD_DEBUG) FATAL("msglist_insert passed nothing in str");
        return list;
    }

    msg = (TSpeechDMessage*) spd_malloc(sizeof(TSpeechDMessage));
    msg->bytes = strlen(str);
    msg->buf = str;
    msg->settings.type = type;
    nlist = g_list_append(list, msg);                   

    return nlist;
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

    queue = speaking_get_queue(priority);

    /* Stop all other priority 3 messages but leave the first */
    gl = g_list_last(queue); 
                
    if (gl == NULL) return;  
    if (gl->data == NULL) return;

    msg = (TSpeechDMessage*) gl->data;
    if (msg->settings.reparted <= 0){                
        queue = g_list_remove_link(queue, gl);
        speaking_set_queue(priority, queue);        

        stop_priority(priority);
    }

    /* Fill the queue with the list containing only the first message */
    speaking_set_queue(priority, gl);

    return;
}

static void
resolve_priorities()
{
    GList *gl;
    TSpeechDMessage *msg;

    if(g_list_length(MessageQueue->p1) != 0){        
        stop_priority(2);
        stop_priority(3);
        stop_priority(4);
        stop_priority(5);
    }
		    
    if(g_list_length(MessageQueue->p2) != 0){
        stop_priority_except_first(2);
        stop_priority(3);
        stop_priority(4);
        stop_priority(5);
    }

    if(g_list_length(MessageQueue->p3) != 0){
        stop_priority(4);
        stop_priority(5);
    }

    if(g_list_length(MessageQueue->p4) != 0){
        stop_priority_except_first(4);
    }

    if(g_list_length(MessageQueue->p5) != 0){
        stop_priority(4);
        if (is_sb_speaking()){
            MessageQueue->p5 = empty_queue(MessageQueue->p5);
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

static OutputModule*
get_output_module(const TSpeechDMessage *message)
{
    OutputModule *output;

    if (message->settings.type != MSGTYPE_SOUND){
        output = g_hash_table_lookup(output_modules, message->settings.output_module);
        if(output == NULL){
            MSG(4,"Didn't find prefered output module, using default");
            output = g_hash_table_lookup(output_modules, GlobalFDSet.output_module); 
            if (output == NULL) MSG(2, "Can't find default output module!");                
        }
    }else{   /* if MSGTYPE_SOUND */
        output = sound_module;
        if (output == NULL) MSG(2,"Can't find sound module!");
    }

    return output;
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
