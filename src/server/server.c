
 /*
  * server.c - Speech Deamon server core
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
  * $Id: server.c,v 1.32 2003-04-17 11:12:49 hanke Exp $
  */

#include "speechd.h"

int last_message_id = -1;

int server_data_on(int fd);
void server_data_off(int fd);

/* Determines if this messages is to be spoken
 * (returns 1) or it's parent client is paused (returns 0).
 * Note: If you are wondering why it's reversed (not to speak instead
 * of to speak), it's because we also use this function for
 * searching through the list. */
gint
message_nto_speak(TSpeechDMessage *elem, gpointer a, gpointer b)
{
    TFDSetElement *global_settings;
    GList *gl;

    /* Is there something in the body of the message? */
    if(elem == NULL) return 0;

    /* Find global settings for this connection. */	
    global_settings = get_client_settings_by_fd(elem->settings.fd);
    if (global_settings == NULL) return 0;	
 
    if (!global_settings->paused) return 0;
    else return 1;
}

/* Read one char  (which _pointer_ is pointing to) from an UTF-8 string
 * and store it into _character_. _character_ must have space for
 * at least  7 bytes (6 bytes character + 1 byte trailing 0). This
 * function doesn't validate if the string is valid UTF-8.
 */
int
spd_utf8_read_char(char* pointer, char* character)
{
    int bytes;
    gunichar u_char;

    u_char = g_utf8_get_char(pointer);
    bytes = g_unichar_to_utf8(u_char, character);
    character[bytes]=0;

    return bytes;
}

char*
process_message_spell(char *buf, int bytes, TFDSetElement *settings, GHashTable *icons)
{
    int i;
    char *spelled_letter;
    char *character;
    char *pos;
    GString *str;
    char *new_message;

    str = g_string_sized_new(bytes);
    character = (char*) spd_malloc(8); /* 6 bytes should be enough, plus the trailing 0 */

    assert(settings->spelling_table != NULL);

    pos = buf;                  /* Set the position cursor for UTF8 to the beginning. */
    for(i=0; i<=g_utf8_strlen(buf, -1) - 1; i++){
        spd_utf8_read_char(pos, character);
        spelled_letter = (char*) snd_icon_spelling_get(settings->spelling_table,
                                                       icons, character);
        if (spelled_letter != NULL){
            g_string_append(str, spelled_letter);
        }else{
            g_string_append(str, character);
        }
        g_string_append(str," ");
        pos = g_utf8_find_next_char(pos, NULL); /* Skip to the next UTF8 character */
    }

    new_message = str->str;
    g_string_free(str, 0);
    free(character);
    return new_message;
}

char*
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
    char *new_message;

    str = g_string_sized_new(bytes);
    character = (char*) spd_malloc(8); /* 6 bytes should be enough, plus the trailing 0 */

    assert(settings->punctuation_table!=NULL);

    pos = buf;                  /* Set the position cursor for UTF8 to the beginning. */
    for(i=0; i<=g_utf8_strlen(buf, -1) - 1; i++){
        u_char = g_utf8_get_char(pos);
        length = g_unichar_to_utf8(u_char, character);
        character[length]=0;

        if(g_unichar_ispunct(u_char)){
            if(settings->punctuation_mode == 2){
                inside = g_utf8_strchr(settings->punctuation_some,-1,u_char);
                if (inside == NULL){
                    pos = g_utf8_find_next_char(pos, NULL); /* Skip to the next UTF8 character */		                       continue;
                }
            }

            spelled_punct = (char*) snd_icon_spelling_get(settings->punctuation_table,
                                                          icons, character);
            g_string_append(str," ");
            if(spelled_punct != NULL) g_string_append(str, spelled_punct);
            g_string_append(str," ");
        }else{
            g_string_append(str, character);
        }

        pos = g_utf8_find_next_char(pos, NULL); /* Skip to the next UTF8 character */
    }

    new_message = str->str;
	
    g_string_free(str, 0);
    free(character);
	
    return new_message;
}

char*
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

int
queue_message(TSpeechDMessage *new, int fd, int history_flag, EMessageType type)
{
    GList *gl;
    TFDSetElement *settings;
    TSpeechDMessage *newgl;
		
    if (new == NULL) return -1;

    /* Find settings for this particular client */
    settings = get_client_settings_by_fd(fd);
    if (settings == NULL)
        FATAL("Couldn't find settings for active client, internal error.");

    /* Copy the settings to the new to-be-queued element */
    new->settings = *settings;
    new->settings.type = type;
    new->settings.output_module = (char*) spd_malloc(strlen(settings->output_module) + 1);
    new->settings.language = (char*) spd_malloc( strlen(settings->language) + 1);
    new->settings.client_name = (char*) spd_malloc( strlen(settings->client_name) + 1);
    strcpy(new->settings.output_module, settings->output_module);
    strcpy(new->settings.language, settings->language);
    strcpy(new->settings.client_name, settings->client_name);

    /* And we set the global id (note that this is really global, not
     * depending on the particular client, but unique) */
    last_message_id++;				
    new->id = last_message_id;

    /* Put the element new to queue according to it's priority. */
    switch(settings->priority){
    case 1: MessageQueue->p1 = g_list_append(MessageQueue->p1, new); 
        break;
    case 2: MessageQueue->p2 = g_list_append(MessageQueue->p2, new);
        break;
    case 3: MessageQueue->p3 = g_list_append(MessageQueue->p3, new);
        break;
    default: FATAL("Nonexistent priority given");
    }

    msgs_to_say++;
	
    if (history_flag){
        /* Put the element _new_ to history also. */
        /* We will make an exact copy of the message for inclusion into history. */
        newgl = (TSpeechDMessage*) history_list_new_message(new); 
        if(newgl != NULL){
            message_history = g_list_append(message_history, newgl);
        }else{
            if(SPEECHD_DEBUG) FATAL("Can't include message into history\n");
        }
    }

    return 0;
}

/*
  Parse() receives input data and parses them. It can
  be either command or data to speak. If it's command, it
  is immadetaily executed (eg. parameters are set). If it's
  data to speak, they are queued in corresponding queues
  with corresponding parameters for synthesis.
*/

char* 
parse(char *buf, int bytes, int fd)
{
    TSpeechDMessage *new;
    TFDSetElement *settings;
    GList *gl;
    char *command;
    char *param;
    int helper;
    char *ret;
    int r, v, i;
    int end_data;
    char *pos;

    end_data = 0;
	
    assert(fd > 0);
    if ((buf == NULL) || (bytes == 0)){
        if(SPEECHD_DEBUG) FATAL("invalid buffer for parse()\n");
        return "ERR INTERNAL";
    }	
   	
    /* First the condition that we are not in data mode and we
     * are awaiting commands */
    if (g_array_index(awaiting_data,int,fd) == 0){
        /* Read the command */
        command = get_param(buf, 0, bytes, 1);

        MSG(5, "Command caught: \"%s\"", command);

        /* Here we will check which command we got and process
         * it with it's parameters. */

        if (command == NULL){
            if(SPEECHD_DEBUG) FATAL("Invalid buffer for parse()\n");
            return ERR_INTERNAL; 
        }		
		
        if (!strcmp(command,"set")){				
            return (char *) parse_set(buf, bytes, fd);
        }
        if (!strcmp(command,"history")){				
            return (char *) parse_history(buf, bytes, fd);
        }
        if (!strcmp(command,"stop")){
            return (char *) parse_stop(buf, bytes, fd);
        }
        if (!strcmp(command,"pause")){
            return (char *) parse_pause(buf, bytes, fd);
        }
        if (!strcmp(command,"resume")){
            return (char *) parse_resume(buf, bytes, fd);
        }
        if (!strcmp(command,"cancel")){
            return (char *) parse_cancel(buf, bytes, fd);
        }
        if (!strcmp(command,"sound_icon")){				
            return (char *) parse_snd_icon(buf, bytes, fd);
        }
        if (!strcmp(command,"char")){				
            return (char *) parse_char(buf, bytes, fd);
        }
        if (!strcmp(command,"key")){				
            return (char *) parse_key(buf, bytes, fd);
        }

        /* Check if the client didn't end the session */
        if (!strcmp(command,"bye") || !strcmp(command,"quit")){
            MSG(4, "Bye received.");
            /* Send a reply to the socket */
            write(fd, OK_BYE, strlen(OK_BYE)+1);
            speechd_connection_destroy(fd);
            /* This is internal Speech Deamon message, see serve() */
            return "999 CLIENT GONE";
        }
	
        if (!strcmp(command,"speak")){
            /* Ckeck if we have enough space in awaiting_data table for
             * this client, that can have higher file descriptor that
             * everything we got before */
            r = server_data_on(fd);
            if (r!=0){
                if(SPEECHD_DEBUG) FATAL("Can't switch to data on mode\n");
                return "ERR INTERNAL";								 
            }
            return OK_RECEIVE_DATA;
        }
        return ERR_INVALID_COMMAND;

        /* The other case is that we are in awaiting_data mode and
         * we are waiting for text that is comming through the chanel */
    }else{
    enddata:
        /* In the end of the data flow we got a "@data off" command. */
        MSG(5,"Buffer: |%s| bytes:", buf, bytes);
        if(((bytes>=5)&&((!strncmp(buf,"\r\n.\r\n", bytes))))||(end_data==1)
           ||((bytes==3)&&(!strncmp(buf,".\r\n", bytes)))){
            MSG(4,"Finishing data");
            end_data=0;
            /* Set the flag to command mode */
            MSG(4, "Switching back to command mode...");
            v = 0;
            g_array_insert_val(awaiting_data, fd, v);

            /* Prepare element (text+settings commands) to be queued. */
            new = (TSpeechDMessage*) spd_malloc(sizeof(TSpeechDMessage));
            new->bytes = g_array_index(o_bytes,int,fd);
            new->buf = (char*) spd_malloc(new->bytes + 1);
            memcpy(new->buf, o_buf[fd]->str, new->bytes);
            assert(new->bytes>=0);
            new->buf[new->bytes] = 0;

            if (new->bytes==0) return OK_MSG_CANCELED;
				
            if(queue_message(new, fd, 1, TEXT) != 0){
                if(SPEECHD_DEBUG) FATAL("Can't queue message\n");
                free(new->buf);
                free(new);
                return ERR_INTERNAL;
            }
				
            MSG(4, "%d bytes put in queue and history", g_array_index(o_bytes,int,fd));
            MSG(4, "messages_to_say set to: %d",msgs_to_say);

            /* Clear the counter of bytes in the output buffer. */
            server_data_off(fd);
            return OK_MESSAGE_QUEUED;
        }
	
        if(bytes>=5){
            if(pos = strstr(buf,"\r\n.\r\n")){	
                bytes=pos-buf;
                end_data=1;		
                MSG(4,"Command in data caught");
            }
        }

        /* Get the number of bytes read before, sum it with the number of bytes read
         * now and store again in the counter */
        v = g_array_index(o_bytes,int,fd);
        v += bytes;
        g_array_set_size(o_bytes,fd);	 
        g_array_insert_val(o_bytes, fd, v);
    
        buf[bytes] = 0;
        g_string_append(o_buf[fd],buf);
    }

    if (end_data == 1) goto enddata;

    /* Don't reply on data */
    return "";
}

int
server_data_on(int fd)
{
    int v;

    /* Mark this client as ,,sending data'' */
    v = 1;
    g_array_insert_val(awaiting_data, fd, v);
    /* Create new output buffer */
    o_buf[fd] = g_string_new("\0");
    assert(o_buf[fd] != NULL);
    MSG(4, "Switching to data mode...");
    return 0;
}

void
server_data_off(int fd)
{
    int v;
    v = 0;
    g_array_insert_val(o_bytes, fd, v);
    if(o_buf[fd]!=NULL)	g_string_free(o_buf[fd],1);
    else if(SPEECHD_DEBUG) FATAL("o_buf[fd] == NULL while data on\n");
}

/* Serve the client on _fd_ if we got some activity. */
int
serve(int fd)
{
    int bytes;              /* Number of bytes we got */
    char buf[BUF_SIZE];     /* The content (commands or data) we got */
    char *reply;
    int ret;                /* Return value of write() */
 
    /* Read data from socket */
    MSG(4, "Reading data");
    bytes = read(fd, buf, BUF_SIZE);
    if(bytes == -1) return -1;
    buf[bytes]=0;
    MSG(4,"Read %d bytes from client on fd %d", bytes, fd);

    /* Parse the data and read the reply*/
    reply = parse(buf, bytes, fd);
	
    /* Send the reply to the socket */
    if (strlen(reply) == 0) return 0;

    if(reply[0] != '9'){
        ret = write(fd, reply, strlen(reply));
        if (ret == -1) return -1;	
    }

    return 0;
}
