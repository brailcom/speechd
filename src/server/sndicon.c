
/*
 * sndicon.c - Manipulates sound icons in their different forms
 *                  (icons, spelling tables, key tables, ...)
 *
 * Copyright (C) 2001,2002,2003 Ceska organizace pro podporu free software
 * (Czech Free Software Organization), Prague 2, Vysehradska 3/255, 128 00,
 * <freesoft@freesoft.cz>
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
 * $Id: sndicon.c,v 1.2 2003-04-14 02:10:13 hanke Exp $
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

    icons = g_hash_table_lookup(snd_icon_langs, language);
    if (icons == NULL){
        icons = g_hash_table_lookup(snd_icon_langs, GlobalFDSet.language);
        if (icons == NULL) return 1;
    }

    key = (char*) spd_malloc((strlen(name) + strlen(prefix) + 3) * sizeof(char));
    sprintf(key, "%s_%s", prefix, name);
	
    text = g_hash_table_lookup(icons, key); 

    if(text == NULL) return 2;

    new = malloc(sizeof(TSpeechDMessage));
    new->bytes = strlen(text)+1;
    new->buf = malloc(new->bytes);
    strcpy(new->buf, text);
    if(queue_message(new,fd))  FATAL("Couldn't queue message\n");

    return 0;
}

char*
snd_icon_spelling_get(char *table, GHashTable *icons, char *name)
{
    char *key;
    char *text;
    char *ret;

    key = (char*) spd_malloc((strlen(name) + strlen(table) + 3) * sizeof(char));
    sprintf(key, "%s_%s", table, name);

    text = g_hash_table_lookup(icons, key); 
    if(text == NULL) return NULL;

    ret = (char*) spd_malloc((strlen(text)+1) *  sizeof(char));
    strcpy(ret, text);
    
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

    ret = sndicon_queue(fd, settings->language, "icons", name);
    return ret;
}

int
sndicon_char(int fd, char *name)
{
    int ret;
    TFDSetElement *settings;		

    settings = get_client_settings_by_fd(fd);
    if (settings == NULL)
        FATAL("Couldn't find settings for active client, internal error."); 

    ret = sndicon_queue(fd, settings->language, settings->spelling_table, name);
    return ret;
}

int
sndicon_key(int fd, char *name)
{
    int ret;
    TFDSetElement *settings;		

    settings = get_client_settings_by_fd(fd);
    if (settings == NULL)
        FATAL("Couldn't find settings for active client, internal error."); 

    ret = sndicon_queue(fd, settings->language, "keys", name);
    return ret;
}

