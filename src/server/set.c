
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
 * $Id: set.c,v 1.21 2003-05-26 16:04:50 hanke Exp $
 */

#include "set.h"

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
    if ((priority < 0) || (priority > 3)) return 1;
    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;

    set_param_int(&settings->priority, priority);
    return 0;
}

int
set_rate_self(int fd, int rate)
{
    int uid;
    int ret;
	
    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return 1;
    ret = set_rate_uid(uid, rate);

    return ret;
}

int
set_rate_uid(int uid, int rate)
{
    TFDSetElement *settings;

    if ((rate > 100) || (rate < -100)) return 1;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;

    set_param_int(&settings->rate, rate);
    return 0;
}

int
set_rate_all(int rate)
{
    int i;
    int uid;
    int err = 0;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += set_rate_uid(uid, rate);
    }

    if (err > 0) return 1;
    return 0;
}

int
set_pitch_self(int fd, int pitch)
{
    int uid;
    int ret;
	
    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return 1;
    ret = set_pitch_uid(uid, pitch);

    return ret;
}

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

int
set_pitch_all(int pitch)
{
    int i;
    int uid;
    int err = 0;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += set_pitch_uid(uid, pitch);
    }

    if (err > 0) return 1;
    return 0;
}

int
set_voice_self(int fd, char* voice)
{
    int uid;
    int ret;
	
    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return 1;
    ret = set_voice_uid(uid, voice);

    return ret;
}

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

int
set_voice_all(char* voice)
{
    int i;
    int uid;
    int err = 0;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += set_voice_uid(uid, voice);
    }

    if (err > 0) return 1;
    return 0;
}

int
set_punctuation_mode_uid(int uid, EPunctMode punctuation)
{
    TFDSetElement *settings;
	
    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;

    set_param_int((int*) &settings->punctuation_mode, punctuation);
    return 0;
}

int
set_punctuation_mode_self(int fd, EPunctMode punctuation)
{
    int uid;
    int ret;
	
    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return 1;
    ret = set_punctuation_mode_uid(uid, punctuation);

    return ret;
}

int
set_punctuation_mode_all(EPunctMode punctuation_mode)
{
    int i;
    int uid;
    int err = 0;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += set_punctuation_mode_uid(uid, punctuation_mode);
    }

    if (err > 0) return 1;
    return 0;
}

int
set_punctuation_table_uid(int uid, char* punctuation_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if(settings == NULL) return 1;
    if(punctuation_table == NULL) return 1;

    if(g_list_find_custom(tables.spelling, punctuation_table, spd_str_compare)){
        set_param_str(settings->punctuation_table, punctuation_table);
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

int
set_punctuation_table_self(int fd, char* punctuation_table)
{
    int uid;
    int ret;
	
    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return 1;
    ret = set_punctuation_table_uid(uid, punctuation_table);

    return ret;
}

int
set_punctuation_table_all(char* punctuation_table)
{
    int i;
    int uid;
    int err = 0;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += set_punctuation_table_uid(uid, punctuation_table);
    }

    if (err > 0) return 1;
    return 0;
}



int
set_capital_letter_recognition_uid(int uid, int recogn)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;

    set_param_int(&settings->cap_let_recogn, recogn);
    return 0;
}

int
set_capital_letter_recognition_self(int fd, int recogn)
{
    int uid;
    int ret;
	
    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return 1;
    ret = set_capital_letter_recognition_uid(uid, recogn);

    return ret;
}

int
set_capital_letter_recognition_all(int recogn)
{
    int i;
    int uid;
    int err = 0;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += set_capital_letter_recognition_uid(uid, recogn);
    }

    if (err > 0) return 1;
    return 0;
}

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

int
set_spelling_self(int fd, int spelling)
{
    int uid;
    int ret;
	
    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return 1;
    ret = set_spelling_uid(uid, spelling);

    return ret;
}

int
set_spelling_all(int spelling)
{
    int i;
    int uid;
    int err = 0;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += set_spelling_uid(uid, spelling);
    }

    if (err > 0) return 1;
    return 0;
}

int
set_spelling_table_uid(int uid, char* spelling_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
    if (spelling_table == NULL) return 1;

    if(g_list_find_custom(tables.spelling, spelling_table, spd_str_compare)){
        set_param_str(settings->spelling_table, spelling_table);
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
set_spelling_table_self(int fd, char* spelling_table)
{
    int uid;
    int ret;
	
    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return 1;
    ret = set_spelling_table_uid(uid, spelling_table);

    return ret;
}

int
set_spelling_table_all(char* spelling_table)
{
    int i;
    int uid;
    int err = 0;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += set_spelling_table_uid(uid, spelling_table);
    }

    if (err > 0) return 1;
    return 0;
}

int
set_sound_table_uid(int uid, char* sound_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
    if (sound_table == NULL) return 1;

    if(g_list_find_custom(tables.sound_icons, sound_table, spd_str_compare)){
        set_param_str(settings->snd_icon_table, sound_table);
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
set_sound_table_self(int fd, char* sound_table)
{
    int uid;
    int ret;
	
    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return 1;
    ret = set_sound_table_uid(uid, sound_table);

    return ret;
}

int
set_sound_table_all(char* sound_table)
{
    int i;
    int uid;
    int err = 0;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += set_sound_table_uid(uid, sound_table);
    }

    if (err > 0) return 1;
    return 0;
}


int
set_language_uid(int uid, char *language)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
	
    set_param_str(settings->language, language);        

    return 0;
}

int
set_language_self(int fd, char* language)
{
    int uid;
    int ret;
	
    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return 1;
    ret = set_language_uid(uid, language);

    return ret;
}

int
set_language_all(char* language)
{
    int i;
    int uid;
    int err = 0;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += set_language_uid(uid, language);
    }

    if (err > 0) return 1;
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
    for(i=0; i <= strlen(client_name)+1; i++)
        if(client_name[i]==':') dividers++;
    if (dividers != 2) return 1;
    
    set_param_str(settings->client_name, client_name);

    return 0;
}

int
set_key_table_uid(int uid, char* key_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL)
        FATAL("Couldn't find settings for active client, internal error.");
    if (key_table == NULL) return 1;

    if(g_list_find_custom(tables.keys, key_table, spd_str_compare)){
        set_param_str(settings->key_table, key_table);
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
set_key_table_self(int fd, char* key_table)
{
    int uid;
    int ret;
	
    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return 1;
    ret = set_key_table_uid(uid, key_table);

    return ret;
}

int
set_key_table_all(char* key_table)
{
    int i;
    int uid;
    int err = 0;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += set_key_table_uid(uid, key_table);
    }

    if (err > 0) return 1;
    return 0;
}

int
set_character_table_uid(int uid, char* char_table)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
    if (char_table == NULL) return 1;

    if(g_list_find_custom(tables.characters, char_table, spd_str_compare)){
        set_param_str(settings->char_table, char_table);
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

int
set_character_table_self(int fd, char* char_table)
{
    int uid;
    int ret;
	
    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return 1;
    ret = set_character_table_uid(uid, char_table);

    return ret;
}

int
set_character_table_all(char* char_table)
{
    int i;
    int uid;
    int err = 0;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += set_character_table_uid(uid, char_table);
    }

    if (err > 0) return 1;
    return 0;
}

int
set_output_module_uid(int uid, char* output_module)
{
    TFDSetElement *settings;

    settings = get_client_settings_by_uid(uid);
    if (settings == NULL) return 1;
    if (output_module == NULL) return 1;

    set_param_str(settings->output_module, output_module);

    return 0;
}

int
set_output_module_all(char* output_module)
{
    int i;
    int uid;
    int err = 0;

    for(i=1;i<=fdmax;i++){
        uid = get_client_uid_by_fd(i);
        if (uid == 0) continue;
        err += set_output_module_uid(uid, output_module);
    }

    if (err > 0) return 1;
    return 0;
}	

int
set_output_module_self(int fd, char* output_module)
{
    int uid;
    int ret;
	
    uid = get_client_uid_by_fd(fd);
    if (uid == 0) return 1;
    ret = set_output_module_uid(uid, output_module);

    return ret;
}	

TFDSetElement*
default_fd_set(void)
{
	TFDSetElement *new;

	new = (TFDSetElement*) spd_malloc(sizeof(TFDSetElement));
	new->language = (char*) spd_malloc(64);			/* max 63 characters */
	new->output_module = (char*) spd_malloc(128);
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
	new->rate = GlobalFDSet.rate;
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

void
set_param_int(int* parameter, int value)
{
    *parameter = value;
}

void
set_param_str(char* parameter, char* value)
{
    assert(parameter != NULL);

    if(value == NULL){
        parameter = NULL;
    }
    realloc(parameter, (strlen(value) + 1) * sizeof(char));
    strcpy(parameter, value);
}
