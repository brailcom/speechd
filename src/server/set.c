
/*
 * set.c - Settings related functions for Speech Deamon
 *
 * Copyright (C) 2001, 2002, 2003 Brailcom, o.p.s.
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
 * $Id: set.c,v 1.17 2003-04-18 20:38:28 hanke Exp $
 */


#include "set.h"

gint
spd_str_compare(gconstpointer a, gconstpointer b)
{
  return strcmp((char*) a, (char*) b);
}

int
set_priority(int fd, int priority)
{
    TFDSetElement *settings;
	
	settings = get_client_settings_by_fd(fd);
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings->priority = priority;
	return 1;
}

int
set_rate(int fd, int rate)
{
    TFDSetElement *settings;
	
	settings = get_client_settings_by_fd(fd);
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings->speed = rate;
	return 1;
}

int
set_pitch(int fd, int pitch)
{
    TFDSetElement *settings;

	settings = get_client_settings_by_fd(fd);
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings->pitch = pitch;
	return 1;
}

int
set_voice(int fd, char *voice)
{
	TFDSetElement *settings;

	settings = get_client_settings_by_fd(fd);
	if (!strcmp(voice, "male1")) settings->voice_type = MALE1;
	else if (!strcmp(voice, "male2")) settings->voice_type = MALE2;
	else if (!strcmp(voice, "male3")) settings->voice_type = MALE3;
	else if (!strcmp(voice, "female1")) settings->voice_type = FEMALE1;
	else if (!strcmp(voice, "female2")) settings->voice_type = FEMALE2;
	else if (!strcmp(voice, "female3")) settings->voice_type = FEMALE3;
	else if (!strcmp(voice, "child_male")) settings->voice_type = CHILD_MALE;
	else if (!strcmp(voice, "child_female")) settings->voice_type = CHILD_FEMALE;
	else return 0;

	return 1;
}

int
set_punct_mode(int fd, int punct)
{
    TFDSetElement *settings;
	
	settings = get_client_settings_by_fd(fd);
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings->punctuation_mode = punct;
	return 1;
}

int
set_punctuation_table(int fd, char* punctuation_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_fd(fd);
    if (settings == NULL) FATAL("Couldn't find settings for active client, internal error.");
    if(punctuation_table == NULL) return 1;

    if(g_list_find_custom(tables.spelling, punctuation_table, spd_str_compare)){
        realloc(settings->punctuation_table, (strlen(punctuation_table) + 1)*sizeof(char));
        strcpy(settings->punctuation_table, punctuation_table);
    }else{
        if(!strcmp(punctuation_table, "punctuation_basic")){
            MSG(3,"Couldn't find requested table, using the previous");
            return 0;
        }else{
            return 1;
        }        
    }
    return 0;
}

int
set_cap_let_recog(int fd, int recog)
{
    TFDSetElement *settings;

	settings = get_client_settings_by_fd(fd);
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings->cap_let_recogn = recog;
	return 1;
}

int
set_spelling(int fd, int spelling)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_fd(fd);
    if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
    settings->spelling = spelling;
    return 1;
}

int
set_spelling_table(int fd, char* spelling_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_fd(fd);
    if (settings == NULL) FATAL("Couldn't find settings for active client, internal error.");
    if (spelling_table == NULL) return 1;

    if(g_list_find_custom(tables.spelling, spelling_table, spd_str_compare)){
        realloc(settings->spelling_table, (strlen(spelling_table) + 1)*sizeof(char));
        strcpy(settings->spelling_table, spelling_table);
    }else{
        if(!strcmp(spelling_table, "spelling_short")
           || !strcmp(spelling_table, "spelling_long")){
            MSG(4,"Couldn't find requested table, using the previous");
            return 0;
        }else{
            return 1;
        }        
    }

    return 0;
}

int
set_sound_table(int fd, char* sound_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_fd(fd);
    if (settings == NULL) FATAL("Couldn't find settings for active client, internal error.");
    if (sound_table == NULL) return 1;

    if(g_list_find_custom(tables.sound_icons, sound_table, spd_str_compare)){
        realloc(settings->snd_icon_table, (strlen(sound_table) + 1)*sizeof(char));
        strcpy(settings->snd_icon_table, sound_table);
    }else{
        if(!strcmp(sound_table, "sound_icons_default")){
            MSG(4,"Couldn't find requested table, using the previous");
            return 0;
        }else{
            return 1;
        }        
    }

    return 0;
}

int
set_language(int fd, char *language){
    TFDSetElement *settings;

	settings = get_client_settings_by_fd(fd);
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
	free(settings->language);
	settings->language = (char*) malloc(strlen(language) * sizeof(char) + 1);
	if (settings->language == NULL) FATAL ("Not enough memmory.");
	strcpy(settings->language, language);
	return 1;
}

int
set_client_name(int fd, char *client_name)
{
    TFDSetElement *settings;
	GList *gl;
	
	settings = get_client_settings_by_fd(fd);
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
	free(settings->client_name);
	settings->client_name = (char*) malloc(strlen(client_name) * sizeof(char) + 1);
	strcpy(settings->client_name, client_name);

	return 1;
}

int
set_key_table(int fd, char* key_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_fd(fd);
    if (settings == NULL) FATAL("Couldn't find settings for active client, internal error.");
    if (key_table == NULL) return 1;

    if(g_list_find_custom(tables.keys, key_table, spd_str_compare)){
        realloc(settings->key_table, (strlen(key_table) + 1)*sizeof(char));
        strcpy(settings->key_table, key_table);
    }else{
        if(!strcmp(key_table, "keys_basic")){
            MSG(4,"Couldn't find requested table, using the previous");
            return 0;
        }else{
            return 1;
        }        
    }
    return 0;
}

int
set_character_table(int fd, char* char_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_fd(fd);
    if (settings == NULL) FATAL("Couldn't find settings for active client, internal error.");
    if (char_table == NULL) return 1;

    if(g_list_find_custom(tables.characters, char_table, spd_str_compare)){
        realloc(settings->char_table, (strlen(char_table) + 1)*sizeof(char));
        strcpy(settings->char_table, char_table);
    }else{
        if(!strcmp(char_table, "characters_basic")){
            MSG(4,"Couldn't find requested table, using the previous");
            return 0;
        }else{
            return 1;
        }        
    }

    return 0;
}
		 
TFDSetElement*
default_fd_set(void)
{
	TFDSetElement *new;

	new = (TFDSetElement*) spd_malloc(sizeof(TFDSetElement));
	new->language = (char*) spd_malloc(64);			/* max 63 characters */
	new->output_module = (char*) spd_malloc(64);
	new->client_name = (char*) spd_malloc(128);		/* max 127 characters */
	new->spelling_table = (char*) spd_malloc(128);		/* max 127 characters */
        new->punctuation_some = (char*) spd_malloc(128);
	new->punctuation_table = (char*) spd_malloc(128);       /* max 127 characters */
	new->key_table = (char*) spd_malloc(128);       /* max 127 characters */
	new->char_table = (char*) spd_malloc(128);       /* max 127 characters */
	new->snd_icon_table = (char*) spd_malloc(128);       /* max 127 characters */

   
	new->paused = 0;

	/* Fill with the global settings values */
	new->priority = GlobalFDSet.priority;
	new->punctuation_mode = GlobalFDSet.punctuation_mode;
	new->speed = GlobalFDSet.speed;
	new->pitch = GlobalFDSet.pitch;
	strcpy(new->language, GlobalFDSet.language);
	strcpy(new->output_module, GlobalFDSet.output_module);
	strcpy(new->client_name, GlobalFDSet.client_name); 
	strcpy(new->spelling_table, GlobalFDSet.spelling_table); 
        strcpy(new->punctuation_some, GlobalFDSet.punctuation_some);
        strcpy(new->punctuation_table, GlobalFDSet.punctuation_table);
        strcpy(new->key_table, GlobalFDSet.key_table);
        strcpy(new->char_table, GlobalFDSet.char_table);
        strcpy(new->snd_icon_table, GlobalFDSet.snd_icon_table);
	new->voice_type = GlobalFDSet.voice_type;
	new->spelling = GlobalFDSet.spelling;         
	new->cap_let_recogn = GlobalFDSet.cap_let_recogn;
	new->active = 1;

	new->hist_cur_uid = -1;
	new->hist_cur_pos = -1;
	new->hist_sorted = 0;

	return(new);
}

int
get_client_uid_by_fd(int fd)
{
    int *uid;
    if(fd <= 0) return 0;
    uid = g_hash_table_lookup(fd_uid, &fd);
    if(uid == NULL) return 0;
    return *uid;
}

TFDSetElement*
get_client_settings_by_fd(int fd)
{
    TFDSetElement* element;
    int uid;

    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return NULL;

    element = g_hash_table_lookup(fd_settings, &uid);
    return element;	
}

TFDSetElement*
get_client_settings_by_uid(int uid){
	TFDSetElement* element;
	
	if (uid < 0) return NULL;
	
	element = g_hash_table_lookup(fd_settings, &uid);
	return element;	
}
