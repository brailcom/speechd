
/*
 * parse.c - Parses commands Speech Deamon got from client
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
 * $Id: parse.c,v 1.23 2003-04-18 20:41:23 hanke Exp $
 */

#include "speechd.h"

/* isanum() tests if the given string is a number,
 * returns 1 if yes, 0 otherwise. */
int
isanum(char *str){
   int i;
   if (!isdigit(str[0]) && !( (str[0]=='+') || (str[0]=='-'))) return 0;
   for(i=1;i<=strlen(str)-1;i++){
       if (!isdigit(str[i]))   return 0;
    }
    return 1;
}


/* Gets command parameter _n_ from the text buffer _buf_
 * which has _bytes_ bytes. Note that the parameter with
 * index 0 is the command itself. */
char* 
get_param(char *buf, int n, int bytes, int lower_case)
{
    char* param;
    int i, y, z;
    int quote_open = 0;

    param = (char*) malloc(bytes);
    assert(param != NULL);
	
    strcpy(param,"");
    i = 0;

    /* Read all the parameters one by one,
     * but stop after the one with index n,
     * while maintaining it's value in _param_ */
    for(y=0; y<=n; y++){
        z=0;
        for(; i<bytes; i++){
            if((buf[i] == '\"')&&(!quote_open)){
                quote_open = 1;
                continue;
            }
            if((buf[i] == '\"')&&(quote_open)){
                quote_open = 0;
                continue;
            }
            if(quote_open!=1){
                if (buf[i] == ' ') break;
            }
            param[z] = buf[i];
            z++;
        }
        i++;
    }
   
    /* Write the trailing zero */
    if (i >= bytes){
        param[z>1?z-2:0] = 0;
    }else{
        param[z] = 0;
    }

    if(lower_case){
        param = g_ascii_strdown(param, strlen(param));
    }

    return param;
}


/* Parses @history commands and calls the appropriate history_ functions. */
char*
parse_history(char *buf, int bytes, int fd)
{
    char *param;
    char *helper1, *helper2, *helper3;				
		
    param = get_param(buf,1,bytes, 1);
    MSG(4, "param 1 caught: %s\n", param);
    if (!strcmp(param,"get")){
        param = get_param(buf,2,bytes, 1);
        if (!strcmp(param,"last")){
            return (char*) history_get_last(fd);
        }
        if (!strcmp(param,"client_list")){
            return (char*) history_get_client_list();
        }  
        if (!strcmp(param,"message_list")){
            helper1 = get_param(buf,3,bytes, 0);
            if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
            helper2 = get_param(buf,4,bytes, 0);
            if (!isanum(helper2)) return ERR_NOT_A_NUMBER;
            helper3 = get_param(buf,5,bytes, 0);
            if (!isanum(helper2)) return ERR_NOT_A_NUMBER;
            return (char*) history_get_message_list( atoi(helper1), atoi(helper2), atoi (helper3));
        }  
    }
    if (!strcmp(param,"sort")){
        return ERR_NOT_IMPLEMENTED;
        // TODO: everything :)
    }
    if (!strcmp(param,"cursor")){
        param = get_param(buf,2,bytes, 1);
        MSG(4, "param 2 caught: %s\n", param);
        if (!strcmp(param,"set")){
            param = get_param(buf,4,bytes, 1);
            MSG(4, "param 4 caught: %s\n", param);
            if (!strcmp(param,"last")){
                helper1 = get_param(buf,3,bytes, 0);
                if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
                return (char*) history_cursor_set_last(fd,atoi(helper1));
            }
            if (!strcmp(param,"first")){
                helper1 = get_param(buf,3,bytes, 0);
                if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
                return (char*) history_cursor_set_first(fd, atoi(helper1));
            }
            if (!strcmp(param,"pos")){
                helper1 = get_param(buf,3,bytes, 0);
                if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
                helper2 = get_param(buf,5,bytes, 0);
                if (!isanum(helper2)) return ERR_NOT_A_NUMBER;
                return (char*) history_cursor_set_pos( fd, atoi(helper1), atoi(helper2) );
            }
        }
        if (!strcmp(param,"next")){
            return (char*) history_cursor_next(fd);
        }
        if (!strcmp(param,"prev")){
            return (char*) history_cursor_prev(fd);
        }
        if (!strcmp(param,"get")){
            return (char*) history_cursor_get(fd);
        }
    }
    if (!strcmp(param,"say")){
        param = get_param(buf,2,bytes, 1);
        if (!strcmp(param,"id")){
            helper1 = get_param(buf,3,bytes, 0);
            if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
            return (char*) history_say_id(fd, atoi(helper1));
        }
        if (!strcmp(param,"text")){
            return ERR_NOT_IMPLEMENTED;
        }
    }
 
    return ERR_INVALID_COMMAND;
}

char*
parse_set(char *buf, int bytes, int fd)
{
    char *param;
    char *language;
    char *client_name;
    char *priority;
    char *rate;
    char *pitch;
    char *punct;
    char *punctuation_table;
    char *spelling;
    char *spelling_table;
    char *recog;
    char *voice;
    char *char_table;
    char *key_table;
    char *sound_table;
    int helper;
    int ret;

    param = get_param(buf,1,bytes, 1);
    if (!strcmp(param, "priority")){
        priority = get_param(buf,2,bytes, 0);
        if (!isanum(priority)) return ERR_NOT_A_NUMBER;
        helper = atoi(priority);
        MSG(4, "Setting priority to %d \n", helper);
        ret = set_priority(fd, helper);
        if(!ret) return ERR_COULDNT_SET_PRIORITY;	
        return OK_PRIORITY_SET;
    }
    else if (!strcmp(param,"language")){
        language = get_param(buf,2,bytes, 0);
        MSG(4, "Setting language to %s \n", language);
        ret = set_language(fd, language);
        if (!ret) return ERR_COULDNT_SET_LANGUAGE;
        return OK_LANGUAGE_SET;
    }
    else if (!strcmp(param,"spelling_table")){
        spelling_table = get_param(buf,2,bytes, 0);
        MSG(4, "Setting spelling table to %s \n", spelling_table);
        ret = set_spelling_table(fd, spelling_table);
        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (!strcmp(param,"client_name")){
        client_name = get_param(buf,2,bytes, 0);
        if (client_name == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting client name to %s. \n", client_name);
        ret = set_client_name(fd, client_name);
        if (!ret) return ERR_COULDNT_SET_LANGUAGE;
        return OK_CLIENT_NAME_SET;
    }
    else if (!strcmp(param,"rate")){
        rate = get_param(buf,2,bytes, 0);
        if (rate == NULL) return ERR_MISSING_PARAMETER;
        if (!isanum(rate)) return ERR_NOT_A_NUMBER;
        helper = atoi(rate);
        if(helper < -100) return ERR_COULDNT_SET_RATE;
        if(helper > +100) return ERR_COULDNT_SET_RATE;
        MSG(4, "Setting rate to %d \n", helper);
        ret = set_rate(fd, helper);
        if (!ret) return ERR_COULDNT_SET_RATE;
        return OK_RATE_SET;
    }
    else if (!strcmp(param,"pitch")){
        pitch = get_param(buf,2,bytes, 0);
        if (pitch == NULL) return ERR_MISSING_PARAMETER;
        if (!isanum(pitch)) return ERR_NOT_A_NUMBER;
        helper = atoi(pitch);
        if(helper < -100) return ERR_COULDNT_SET_PITCH;
        if(helper > +100) return ERR_COULDNT_SET_PITCH;
        MSG(4, "Setting pitch to %d \n", pitch);
        ret = set_pitch(fd, helper);
        if (!ret) return ERR_COULDNT_SET_PITCH;
        return OK_PITCH_SET;
    }
    else if (!strcmp(param,"voice")){
        voice = get_param(buf,2,bytes, 0);
        if (voice == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting voice to %s", voice);
        ret = set_voice(fd, voice);
        if (!ret) return ERR_COULDNT_SET_VOICE;
        return OK_VOICE_SET;
    }
    else if (!strcmp(param,"punctuation")){
        punct = get_param(buf,2,bytes, 1);
        if (punct == NULL) return ERR_MISSING_PARAMETER;

        if(!strcmp(punct,"all")) ret = set_punct_mode(fd, 1);
        else if(!strcmp(punct,"some")) ret = set_punct_mode(fd, 2);        
        else if(!strcmp(punct,"none")) ret = set_punct_mode(fd, 0);        
        else return ERR_PARAMETER_INVALID;

        MSG(4, "Setting punctuation mode to %s \n", punct);
        if (!ret) return ERR_COULDNT_SET_PUNCT_MODE;
        return OK_PUNCT_MODE_SET;
    }
    else if (!strcmp(param,"punctuation_table")){
        punctuation_table = get_param(buf,2,bytes, 0);
        if (punctuation_table == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting punctuation table to %s \n", punctuation_table);
        ret = set_punctuation_table(fd, punctuation_table);
        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (!strcmp(param,"character_table")){
        char_table = get_param(buf,2,bytes, 0);
        if (char_table == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting punctuation table to %s \n", char_table);
        ret = set_character_table(fd, char_table);
        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (!strcmp(param,"key_table")){
        key_table = get_param(buf,2,bytes, 0);
        if (key_table == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting punctuation table to %s \n", key_table);
        ret = set_key_table(fd, key_table);
        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (!strcmp(param,"sound_table")){
        sound_table = get_param(buf,2,bytes, 0);
        if (sound_table == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting punctuation table to %s \n", sound_table);
        ret = set_sound_table(fd, sound_table);
        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (!strcmp(param,"cap_let_recogn")){
        recog = get_param(buf,2,bytes, 1);
        if (recog == NULL) return ERR_MISSING_PARAMETER;

        if(!strcmp(recog,"on")) ret = set_cap_let_recog(fd, 1);
        else if(!strcmp(recog,"off")) ret = set_cap_let_recog(fd, 0);        
        else return ERR_PARAMETER_NOT_ON_OFF;

        MSG(4, "Setting capital letter recognition to %s \n", punct);
        if (!ret) return ERR_COULDNT_SET_CAP_LET_RECOG;
        return OK_CAP_LET_RECOGN_SET;
    }else if (!strcmp(param,"spelling")){
        spelling = get_param(buf,2,bytes, 1);
        if (spelling == NULL) return ERR_MISSING_PARAMETER;
        if(!strcmp(spelling,"on")) ret = set_spelling(fd, 1);
        else if(!strcmp(spelling,"off")) ret = set_spelling(fd, 0);        
        else return ERR_PARAMETER_NOT_ON_OFF;

        MSG(4, "Setting spelling to %s", spelling);

        if (!ret) return ERR_COULDNT_SET_SPELLING;
        return OK_SPELLING_SET;
    }else{
        return ERR_PARAMETER_INVALID;
    }

    return ERR_INVALID_COMMAND;
}

char*
parse_stop(char *buf, int bytes, int fd)
{
    int uid = 0;
    char *param;

    param = get_param(buf,1,bytes, 1);
    if (param == NULL) return ERR_MISSING_PARAMETER;

    if (!strcmp(param,"all")){
        speaking_stop_all();
    }
    else if (!strcmp(param, "self")){
        uid = get_client_uid_by_fd(fd);
        if(uid == 0) return ERR_INTERNAL;
        speaking_stop(uid);
    }
    else if (isanum(param)){
        uid = atoi(param);
        if (uid <= 0) return ERR_ID_NOT_EXIST;
        speaking_stop(uid);
    }else{
        return ERR_PARAMETER_INVALID;
    }

    MSG(4, "Stop received.");
    return OK_STOPPED;
}

char*
parse_cancel(char *buf, int bytes, int fd)
{
    int uid = 0;
    char *param;

    param = get_param(buf,1,bytes, 1);
    if (param == NULL) return ERR_MISSING_PARAMETER;

    if (!strcmp(param,"all")){
        speaking_cancel_all();
    }
    else if (!strcmp(param, "self")){
        uid = get_client_uid_by_fd(fd);
        if(uid == 0) return ERR_INTERNAL;
        speaking_cancel(uid);
    }
    else if (isanum(param)){
        uid = atoi(param);
        if (uid <= 0) return ERR_ID_NOT_EXIST;
        speaking_cancel(uid);
    }else{
        return ERR_PARAMETER_INVALID;
    }

    MSG(4, "Cancel received.");

    return OK_CANCELED;
}

char*
parse_pause(char *buf, int bytes, int fd)
{
    int ret;
    int uid = 0;
    char *param;

    param = get_param(buf,1,bytes, 0);
    if (param == NULL) return ERR_MISSING_PARAMETER;

    /*    if (isanum(param)) uid = atoi(param);

    */
    /* If the parameter target_uid wasn't specified,
       act on the calling client. */
    /*    if(uid == 0) uid = get_client_uid_by_fd(fd);
    if(uid == 0) return ERR_NO_SUCH_CLIENT;

    MSG(4, "Pause received.");
    ret = speaking_pause(uid);
    */

    return OK_PAUSED;
}

char*
parse_resume(char *buf, int bytes, int fd)
{
    int ret;
    int uid = 0;
    char *param;

    param = get_param(buf,1,bytes, 0);
    if (isanum(param)) uid = atoi(param);
    /*
    if (param == NULL) return ERR_MISSING_PARAMETER;
    */
    /* If the parameter target_uid wasn't specified,
       act on the calling client. */
    /*    if(uid == 0) uid = get_client_uid_by_fd(fd);
    if(uid == 0) return ERR_NO_SUCH_CLIENT;

    MSG(4, "Resume received.");
    ret = speaking_resume(uid);
    */
    return OK_RESUMED;
}

char*
parse_snd_icon(char *buf, int bytes, int fd)
{
    char *param;
    int ret;
    TFDSetElement *settings;		
    	
    param = get_param(buf,1,bytes, 0);
    if (param == NULL) return ERR_MISSING_PARAMETER;
    MSG(4,"Parameter caught: %s", param);

    ret = sndicon_icon(fd, param);
	
    if (ret!=0){
        if(ret == 1) return ERR_NO_SND_ICONS;
        if(ret == 2) return ERR_UNKNOWN_ICON;
    }

    return OK_SND_ICON_QUEUED;
}				 

char*
parse_char(char *buf, int bytes, int fd)
{
    char *param;
    TFDSetElement *settings;
    int ret;

    param = get_param(buf,1,bytes, 0);
    if (param == NULL) return ERR_MISSING_PARAMETER;
    MSG(4,"Parameter caught: %s", param);

    ret = sndicon_char(fd, param);
    if (ret!=0){
        if(ret == 1) return ERR_NO_SND_ICONS;
        if(ret == 2) return ERR_UNKNOWN_ICON;
    }
	
    return OK_SND_ICON_QUEUED;
}

char*
parse_key(char* buf, int bytes, int fd)
{
    char *param;
    TFDSetElement *settings;
    int ret;

    param = get_param(buf, 1, bytes, 0);
    if (param == NULL) return ERR_MISSING_PARAMETER;
    MSG(4,"Parameter caught: %s", param);

    ret = sndicon_key(fd, param);
    if (ret != 0){
        if(ret == 1) return ERR_NO_SND_ICONS;
        if(ret == 2) return ERR_UNKNOWN_ICON;
    }
    return OK_SND_ICON_QUEUED;
}

char*
parse_list(char* buf, int bytes, int fd)
{
    char* param;

    param = get_param(buf, 1, bytes, 1);    
    if (param == NULL) return ERR_MISSING_PARAMETER;

    if(!strcmp(param,"spelling_tables")){
        return (char*) sndicon_list_spelling_tables();
    }
    else if(!strcmp(param,"sound_tables")){
        return (char*) sndicon_list_sound_tables();
    }
    else if(!strcmp(param, "character_tables")){
        return (char*) sndicon_list_char_tables();
    }
    else if(!strcmp(param, "key_tables")){
        return (char*) sndicon_list_key_tables();
    }
    else return ERR_PARAMETER_INVALID;
}
        
