
 /*
  * server.c - Speech Deamon server core
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
  * $Id: server.c,v 1.34 2003-04-24 19:30:20 hanke Exp $
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

int
server_data_on(int fd)
{
    int v;

    /* Mark this client as ,,sending data'' */
    v = 1;
    awaiting_data[fd]=1;
    /* Create new output buffer */
    o_buf[fd] = g_string_new("\0");
    assert(o_buf[fd] != NULL);
    assert(o_buf[fd]->str != NULL);
    MSG(4, "Switching to data mode...");
    return 0;
}

void
server_data_off(int fd)
{
    int v;
    o_bytes[fd]=0;
    if(o_buf[fd]!=NULL){
	g_string_free(o_buf[fd],1);
        o_buf[fd] = NULL;
    }
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
