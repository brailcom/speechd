
/*
 * sndicon.c - Manipulates sound icons in their different forms
 *                  (icons, spelling tables, key tables, ...)
 *
 * Copyright (C) 2002, 2003 Brailcom, o.p.s.
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
 * $Id: sndicon.c,v 1.19 2003-07-18 21:39:28 hanke Exp $
*/

#include "sndicon.h"
#include "speechd.h"

int
sndicon_queue(int fd, char* language, char* prefix, char* name)
{
    GHashTable *icons;
    TSpeechDMessage *new;
    char *key;
    char *text;
    char *ret;
    int reparted;

    icons = g_hash_table_lookup(snd_icon_langs, language);
    if (icons == NULL){
        icons = g_hash_table_lookup(snd_icon_langs, GlobalFDSet.language);
        if (icons == NULL) return 1;
    }

    key = (char*) spd_malloc((strlen(name) + strlen(prefix) + 3) * sizeof(char));
    sprintf(key, "%s_%s", prefix, name);
	
    text = g_hash_table_lookup(icons, key); 
    if(text == NULL){
        icons = g_hash_table_lookup(snd_icon_langs, GlobalFDSet.language);
        if (icons == NULL) return 2;
        text = g_hash_table_lookup(icons, key); 
        if (text == NULL) return 2;
    } 

    new = (TSpeechDMessage*) spd_malloc(sizeof(TSpeechDMessage));
    ret = (char*) spd_malloc ((strlen(text) + 1 + strlen(SOUND_DATA_DIR) + 1) * sizeof(char));

    reparted = inside_block[fd];

    if(text[0] == '\"'){
        /* Strip the leading and the trailing quotes */
        strcpy(ret, &(text[1]));
        ret[strlen(ret)-1] = 0;
        new->bytes = strlen(ret)+1;
        new->buf = ret;
        if(queue_message(new, fd, 1, MSGTYPE_TEXTP, reparted))  FATAL("Couldn't queue message\n");
    }else{     
        if(text[0] != '/') sprintf(ret, "%s/%s", SOUND_DATA_DIR, text);
        else strcpy(ret, text);
        new->bytes = strlen(ret)+1;
        new->buf = ret;
        if(queue_message(new, fd, 1, MSGTYPE_SOUND, reparted))  FATAL("Couldn't queue message\n");
    }

    return 0;
}

char*
snd_icon_spelling_get(char *table, GHashTable *icons, char *name, int *sound)
{
    char *textt = NULL;
    char *ret;
    static char key[256];       /* This is not dynamic to make it faster */

    /* Make sure *sound is initialized to something */
    *sound = 0;

    sprintf(key, "%s_%s", table, name);

    textt = g_hash_table_lookup(icons, key); 

    if(textt == NULL) return NULL;
    if(strlen(textt) == 0) return NULL;

    if(textt[0] == '\"'){
        *sound = 0;
        /* Strip the leading and the trailing quotes */
        ret = (char*) spd_malloc(strlen(textt) * sizeof(char));
        strcpy(ret, &(textt[1]));
        ret[strlen(ret)-1] = 0;
    }else{       
        *sound = 1;
        if(textt[0] != '/'){
            ret = (char*) spd_malloc((strlen(SOUND_DATA_DIR) + strlen(textt) + 4) * sizeof(char));
            sprintf(ret, "%s/%s", SOUND_DATA_DIR, textt);
        }else{ 
            ret = (char*) spd_malloc((strlen(textt)+1) * sizeof(char));
            strcpy(ret, textt);
        }
    }
    
    return ret;
}

int
sndicon_icon(int fd, char *name)
{
    int ret;
    TFDSetElement *settings;		

    settings = get_client_settings_by_fd(fd);
    if (settings == NULL)
        FATAL("Couldn't find settings for active client, internal error."); 

    ret = sndicon_queue(fd, settings->language, settings->snd_icon_table, name);
    return ret;
}

int
sndicon_char(int fd, char *name)
{
    int ret;
    TFDSetElement *settings;		
    char *icon_name;

    settings = get_client_settings_by_fd(fd);
    if (settings == NULL)
        FATAL("Couldn't find settings for active client, internal error."); 

    if (!strcmp(name,"space")){
        icon_name = (char*) spd_malloc(4);
        strcpy(icon_name, " ");
    }else{
        icon_name = name;
    }

    ret = sndicon_queue(fd, settings->language, settings->char_table, icon_name);

    if(!strcmp(name,"space")) free(icon_name);

    return ret;
}

int
sndicon_key(int fd, char *name)
{
    int ret;
    TFDSetElement *settings;
    char *key_text;
    char *key_name;
    GString *sequence;
    TSpeechDMessage *message;
    GHashTable *icons;
    int sound;

    sequence = g_string_new("\0");

    settings = get_client_settings_by_fd(fd);
    if (settings == NULL) return 1;

    icons = g_hash_table_lookup(snd_icon_langs, settings->language);
    if (icons == NULL){
        icons = g_hash_table_lookup(snd_icon_langs, GlobalFDSet.language);
        if (icons == NULL) return 1;
    }

    key_name = strtok(name, "_\r\n");
    if (key_name == NULL) return 1;

    do{
        key_text = (char*) snd_icon_spelling_get(settings->key_table, icons, key_name, &sound);
        if (key_text != NULL){            
            sequence = g_string_append(sequence," ");
            sequence = g_string_append(sequence, key_text);
        }else{
            sequence = g_string_append(sequence," ");
            sequence = g_string_append(sequence, key_name);
        }
        key_name = strtok(NULL, "_\r\n");
    }while(key_name!=NULL);

    assert(sequence != NULL);
    assert(sequence->str != NULL);

    message = (TSpeechDMessage*) spd_malloc(sizeof(TSpeechDMessage));
    message->bytes = strlen(sequence->str);
    assert(message->bytes > 0);


    message->buf = sequence->str;

    if(queue_message(message, fd, 1, MSGTYPE_KEY, 0) != 0){
        if(SPEECHD_DEBUG) FATAL("Can't queue message\n");
        free(message->buf);
        free(message);
        return 1;
    }

    return 0;
}

char*
sndicon_list_spelling_tables()
{
    return (char*) sndicon_list_tables(tables.spelling);
}

char*
sndicon_list_sound_tables()
{
    return (char*) sndicon_list_tables(tables.sound_icons);
}

char*
sndicon_list_char_tables()
{
    return (char*) sndicon_list_tables(tables.characters);
}

char*
sndicon_list_key_tables()
{
    return (char*) sndicon_list_tables(tables.keys);
}

char*
sndicon_list_cap_let_recogn_tables()
{
    return (char*) sndicon_list_tables(tables.cap_let_recogn);
}

char*
sndicon_list_punctuation_tables()
{
    return (char*) sndicon_list_tables(tables.punctuation);
}

char*
sndicon_list_tables(GList *table_list)
{
    GString *response;             
    GList *cursor;
    char *ret;

    response = g_string_new("");
    cursor = g_list_first(table_list);
    if (cursor != NULL){
        do{
            g_string_append_printf(response, C_OK_TABLES"-");
            g_string_append_printf(response, "%s", (char*) cursor->data);
            g_string_append(response,"\r\n");
            cursor = g_list_next(cursor);
        }while(cursor != NULL);        
    }
    g_string_append_printf(response, OK_TABLE_LIST_SENT);

    ret = response->str;
    g_string_free(response, 0);

    return ret;
}
