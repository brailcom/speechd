
/*
 * set.c - Settings related functions for Speech Dispatcher
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
 * $Id: set.c,v 1.25 2003-07-16 19:19:55 hanke Exp $
 */

#include "set.h"
#include "alloc.h"

extern char *spd_strdup(char*);

gint
spd_str_compare(gconstpointer a, gconstpointer b)
{
  return strcmp((char*) a, (char*) b);
}

int
set_priority_self(int fd, int priority)
{
    int uid;
    int ret;
    
    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return 1;
    ret = set_priority_uid(uid, priority);

    return ret;
}

int
set_priority_uid(int uid, int priority)
{
    TFDSetElement *settings;
    if ((priority < 0) || (priority > 5)) return 1;
    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;

    set_param_int(&settings->priority, priority);
    return 0;
}

#define SET_SELF_ALL(type, param) \
   int \
   set_ ## param ## _self(int fd, type param) \
   { \
      int uid; \
      uid = get_client_uid_by_fd(fd); \
      if (uid == 0) return 1; \
      return set_ ## param ## _uid(uid, param); \
   } \
   int \
   set_ ## param ## _all(type param) \
   { \
      int i; \
      int uid; \
      int err = 0; \
      for(i=1;i<=fdmax;i++){ \
        uid = get_client_uid_by_fd(i); \
        if (uid == 0) continue; \
        err += set_ ## param ## _uid(uid, param); \
      } \
      if (err > 0) return 1; \
      return 0; \
   }

SET_SELF_ALL(int, rate);

set_rate_uid(int uid, int rate)
{
    TFDSetElement *settings;

    if ((rate > 100) || (rate < -100)) return 1;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;

    set_param_int(&settings->rate, rate);
    return 0;
}

SET_SELF_ALL(int, pitch);

int
set_pitch_uid(int uid, int pitch)
{
    TFDSetElement *settings;

    if ((pitch > 100) || (pitch < -100)) return 1;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;

    set_param_int(&settings->pitch, pitch);
    return 0;
}

SET_SELF_ALL(char*, voice)

int
set_voice_uid(int uid, char *voice)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;

    if (!strcmp(voice, "male1")) settings->voice_type = MALE1;
    else if (!strcmp(voice, "male2")) settings->voice_type = MALE2;
    else if (!strcmp(voice, "male3")) settings->voice_type = MALE3;
    else if (!strcmp(voice, "female1")) settings->voice_type = FEMALE1;
    else if (!strcmp(voice, "female2")) settings->voice_type = FEMALE2;
    else if (!strcmp(voice, "female3")) settings->voice_type = FEMALE3;
    else if (!strcmp(voice, "child_male")) settings->voice_type = CHILD_MALE;
    else if (!strcmp(voice, "child_female")) settings->voice_type = CHILD_FEMALE;
    else return 1;

    return 0;
}

SET_SELF_ALL(EPunctMode, punctuation_mode)

int
set_punctuation_mode_uid(int uid, EPunctMode punctuation)
{
    TFDSetElement *settings;
	
    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;

    set_param_int((int*) &settings->punctuation_mode, punctuation);
    return 0;
}

SET_SELF_ALL(char*, punctuation_table)

int
set_punctuation_table_uid(int uid, char* punctuation_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if(settings == NULL) return 1;
    if(punctuation_table == NULL) return 1;

    if(g_list_find_custom(tables.spelling, punctuation_table, spd_str_compare)){
        set_param_str(&(settings->punctuation_table), punctuation_table);
    }else{
        if(!strcmp(punctuation_table, "punctuation_basic")){
            MSG(3,"Couldn't find requested table, using the previous");
            return 0;
        }else{
            /* We don't have the table between known or standard tables */
            return 1;
        }        
    }
    return 0;
}

SET_SELF_ALL(ECapLetRecogn, capital_letter_recognition)

int
set_capital_letter_recognition_uid(int uid, ECapLetRecogn recogn)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;

    set_param_int((int*) &settings->cap_let_recogn, (int) recogn);
    return 0;
}


SET_SELF_ALL(int, spelling)

int
set_spelling_uid(int uid, int spelling)
{
    TFDSetElement *settings;

    assert((spelling == 0) || (spelling == 1));

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;

    set_param_int(&settings->spelling, spelling);
    return 0;
}

SET_SELF_ALL(char*, spelling_table)

int
set_spelling_table_uid(int uid, char* spelling_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
    if (spelling_table == NULL) return 1;

    if(g_list_find_custom(tables.spelling, spelling_table, spd_str_compare)){
        set_param_str(&(settings->spelling_table), spelling_table);
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

SET_SELF_ALL(char*, sound_table)

int
set_sound_table_uid(int uid, char* sound_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
    if (sound_table == NULL) return 1;

    if(g_list_find_custom(tables.sound_icons, sound_table, spd_str_compare)){
        set_param_str(&(settings->snd_icon_table), sound_table);
    }else{
        if(!strcmp(sound_table, "sound_icons")){
            MSG(4,"Couldn't find requested table, using the previous");
            return 0;
        }else{
            return 1;
        }        
    }

    return 0;
}

SET_SELF_ALL(char*, cap_let_recogn_table)

int
set_cap_let_recogn_table_uid(int uid, char* cap_let_recogn_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
    if (cap_let_recogn_table == NULL) return 1;

    if(g_list_find_custom(tables.cap_let_recogn, cap_let_recogn_table, spd_str_compare)){
        set_param_str(&(settings->cap_let_recogn_table), cap_let_recogn_table);
    }else{
        MSG(4,"Couldn't find requested table, using the previous");
        return 0;
    }

    return 0;
}

SET_SELF_ALL(char*, language);

int
set_language_uid(int uid, char *language)
{
    TFDSetElement *settings;
    char *output_module;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
	
    set_param_str(&(settings->language), language);        

    /* Check if it is not desired to change output module */
    output_module = g_hash_table_lookup(language_default_modules, language);
    if (output_module != NULL)
        set_output_module_uid(uid, output_module);

    return 0;
}

int
set_client_name_self(int fd, char *client_name)
{
    TFDSetElement *settings;
    int dividers = 0;
    int i;

    assert(client_name != NULL);

    settings = get_client_settings_by_fd(fd);
    if (settings == NULL) return 1;

    /* Is the parameter a valid client name? */
    for(i=0; i <= strlen(client_name)-1; i++)
        if(client_name[i]==':') dividers++;
    if (dividers != 2) return 1;
    
    set_param_str(&(settings->client_name), client_name);

    return 0;
}

SET_SELF_ALL(char*, key_table);

int
set_key_table_uid(int uid, char* key_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL)
        FATAL("Couldn't find settings for active client, internal error.");
    if (key_table == NULL) return 1;

    if(g_list_find_custom(tables.keys, key_table, spd_str_compare)){
        set_param_str(&(settings->key_table), key_table);
    }else{
        if(!strcmp(key_table, "keys")){
            MSG(4,"Couldn't find requested table, using the previous");
            return 0;
        }else{
            return 1;
        }        
    }
    return 0;
}

SET_SELF_ALL(char*, character_table);

int
set_character_table_uid(int uid, char* char_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
    if (char_table == NULL) return 1;

    if(g_list_find_custom(tables.characters, char_table, spd_str_compare)){
        set_param_str(&(settings->char_table), char_table);
    }else{
        if(!strcmp(char_table, "characters")){
            MSG(4,"Couldn't find requested table, using the previous");
            return 0;
        }else{
            return 1;
        }        
    }

    return 0;
}


SET_SELF_ALL(char*, output_module);

int
set_output_module_uid(int uid, char* output_module)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
    if (output_module == NULL) return 1;

    MSG(5, "Setting output module to %s", output_module);

    set_param_str(&(settings->output_module), output_module);

    return 0;
}

TFDSetElement*
default_fd_set(void)
{
	TFDSetElement *new;

	new = (TFDSetElement*) spd_malloc(sizeof(TFDSetElement));
   
	new->paused = 0;

	/* Fill with the global settings values */
	new->priority = GlobalFDSet.priority;
	new->punctuation_mode = GlobalFDSet.punctuation_mode;
	new->rate = GlobalFDSet.rate;
	new->pitch = GlobalFDSet.pitch;
	new->language = spd_strdup(GlobalFDSet.language);
	new->output_module = spd_strdup(GlobalFDSet.output_module);
	new->client_name = spd_strdup(GlobalFDSet.client_name); 
	new->spelling_table = spd_strdup(GlobalFDSet.spelling_table); 
        new->cap_let_recogn_table = spd_strdup(GlobalFDSet.cap_let_recogn_table);
        new->cap_let_recogn_sound = spd_strdup(GlobalFDSet.cap_let_recogn_sound);
        new->punctuation_some = spd_strdup(GlobalFDSet.punctuation_some);
        new->punctuation_table = spd_strdup(GlobalFDSet.punctuation_table);
        new->key_table = spd_strdup(GlobalFDSet.key_table);
        new->char_table = spd_strdup(GlobalFDSet.char_table);
        new->snd_icon_table = spd_strdup(GlobalFDSet.snd_icon_table);
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

void
set_param_int(int* parameter, int value)
{
    *parameter = value;
}

void
set_param_str(char** parameter, char* value)
{   
    if(value == NULL){
        *parameter = NULL;
    }

    *parameter = realloc(*parameter, (strlen(value) + 1) * sizeof(char));
    strcpy(*parameter, value);
}
