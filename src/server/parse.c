
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
 * $Id: parse.c,v 1.28 2003-05-07 20:58:37 hanke Exp $
 */

#include "speechd.h"

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
    int r, i;
    int v;
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
    if (awaiting_data[fd] == 0){
        /* Read the command */
        command = get_param(buf, 0, bytes, 1);

        MSG(5, "Command caught: \"%s\"", command);

        /* Here we will check which command we got and process
         * it with its parameters. */

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
        if (!strcmp(command,"list")){				
            return (char *) parse_list(buf, bytes, fd);
        }
        if (!strcmp(command,"help")){
            return (char *) parse_help(buf, bytes, fd);
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
        if(((bytes >= 5)&&((!strncmp(buf, "\r\n.\r\n", bytes))))||(end_data == 1)
           ||((bytes == 3)&&(!strncmp(buf, ".\r\n", bytes)))){
            MSG(4,"Finishing data");
            end_data = 0;
            /* Set the flag to command mode */
            MSG(4, "Switching back to command mode...");
            awaiting_data[fd] = 0;

            /* Prepare element (text+settings commands) to be queued. */
            if (o_bytes[fd] == 0) return OK_MSG_CANCELED;          
            new = (TSpeechDMessage*) spd_malloc(sizeof(TSpeechDMessage));
            new->bytes = o_bytes[fd];
            new->buf = (char*) spd_malloc(new->bytes + 1);

            assert(new->bytes >= 0); 
            assert(o_buf[fd] != NULL);

            memcpy(new->buf, o_buf[fd]->str, new->bytes);
            new->buf[new->bytes] = 0;
				
            if(queue_message(new, fd, 1, MSGTYPE_TEXT) != 0){
                if(SPEECHD_DEBUG) FATAL("Can't queue message\n");
                free(new->buf);
                free(new);
                return ERR_INTERNAL;
            }
				
            //            MSG(4, "%d bytes put in queue and history", o_bytes[fd]);
            MSG(4, "messages_to_say set to: %d", msgs_to_say);

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
        
        o_bytes[fd] += bytes;

        buf[bytes] = 0;
        g_string_append(o_buf[fd],buf);
    }

    if (end_data == 1) goto enddata;

    /* Don't reply on data */
    return "";

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
        if (!strcmp(param,"client_id")){
            return (char*) history_get_client_id(fd);
        }  
        if (!strcmp(param,"message")){
            helper1 = get_param(buf,3,bytes, 0);
            if (!isanum(helper1)) return ERR_NOT_A_NUMBER;         
            return (char*) history_get_message(atoi(helper1));
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
        if (!strcmp(param,"forward")){
            return (char*) history_cursor_forward(fd);
        }
        if (!strcmp(param,"backward")){
            return (char*) history_cursor_backward(fd);
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
    int who;                    /* 0 - self, 1 - uid specified, 2 - all */
    int uid;                    /* uid of the client (only if who == 1) */
    char *language;
    char *client_name;
    char *priority;
    char *rate;
    char *pitch;
    char *punct;
    EPunctMode punct_mode;
    char *punctuation_table;
    char *spelling;
    char *spelling_table;
    char *recog;
    char *voice;
    char *char_table;
    char *key_table;
    char *sound_table;
    int onoff;
    int helper;
    int ret;

    param = get_param(buf,1,bytes, 1);
    if (param == NULL) return ERR_MISSING_PARAMETER;

    if (!strcmp(param,"self")) who = 0;
    else if (!strcmp(param,"all")) who = 2;
    else if (isanum(param)){
        who = 1;
        uid = atoi(param);
    }else return ERR_PARAMETER_INVALID;


    param = get_param(buf, 2, bytes, 1);
    if (param == NULL) return ERR_MISSING_PARAMETER;

    if (!strcmp(param, "priority")){
        if (who != 0) return ERR_COULDNT_SET_PRIORITY; /* Setting priority only allowed for "self" */
        priority = get_param(buf,3,bytes, 0);
        if (priority == NULL) return ERR_MISSING_PARAMETER;
        if (!isanum(priority)) return ERR_NOT_A_NUMBER;
        helper = atoi(priority);
        MSG(4, "Setting priority to %d \n", helper);
        ret = set_priority_self(fd, helper);
        if (ret) return ERR_COULDNT_SET_PRIORITY;	
        return OK_PRIORITY_SET;
    }
    else if (!strcmp(param,"language")){
        language = get_param(buf, 3, bytes, 0);
        if (language == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting language to %s \n", language);
        if (who == 0) ret = set_language_self(fd, language);
        else if (who == 1) ret = set_language_uid(uid, language);
        else if (who == 2) ret = set_language_all(language);
        if (ret) return ERR_COULDNT_SET_LANGUAGE;
        return OK_LANGUAGE_SET;
    }
    else if (!strcmp(param,"spelling_table")){
        spelling_table = get_param(buf, 3, bytes, 0);
        if (spelling_table == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting spelling table to %s \n", spelling_table);
        if (who == 0) ret = set_spelling_table_self(fd, spelling_table);
        else if (who == 1) ret = set_spelling_table_uid(uid, spelling_table);
        else if (who == 2) ret = set_spelling_table_all(spelling_table);
        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (!strcmp(param,"client_name")){
        client_name = get_param(buf, 3, bytes, 0);
        if (client_name == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting client name to %s. \n", client_name);
        if (who != 0) return ERR_COULDNT_SET_CLIENT_NAME; /* only "self" allowed */
        ret = set_client_name_self(fd, client_name);       
        if (ret) return ERR_COULDNT_SET_CLIENT_NAME;
        return OK_CLIENT_NAME_SET;
    }
    else if (!strcmp(param,"rate")){
        rate = get_param(buf, 3, bytes, 0);
        if (rate == NULL) return ERR_MISSING_PARAMETER;
        if (!isanum(rate)) return ERR_NOT_A_NUMBER;
        helper = atoi(rate);
        if(helper < -100) return ERR_COULDNT_SET_RATE;
        if(helper > +100) return ERR_COULDNT_SET_RATE;
        MSG(4, "Setting rate to %d \n", helper);
        if (who == 0) ret = set_rate_self(fd, helper);        
        else if (who == 1) ret = set_rate_uid(uid, helper);
        else if (who == 2) ret = set_rate_all(helper);
        if (ret) return ERR_COULDNT_SET_RATE;
        return OK_RATE_SET;
    }
    else if (!strcmp(param,"pitch")){
        pitch = get_param(buf, 3, bytes, 0);
        if (pitch == NULL) return ERR_MISSING_PARAMETER;
        if (!isanum(pitch)) return ERR_NOT_A_NUMBER;
        helper = atoi(pitch);
        if(helper < -100) return ERR_COULDNT_SET_PITCH;
        if(helper > +100) return ERR_COULDNT_SET_PITCH;
        MSG(4, "Setting pitch to %d \n", pitch);
        if (who == 0) ret = set_pitch_self(fd, helper);        
        else if (who == 1) ret = set_pitch_uid(uid, helper);
        else if (who == 2) ret = set_pitch_all(helper);
        if (ret) return ERR_COULDNT_SET_PITCH;
        return OK_PITCH_SET;
    }
    else if (!strcmp(param,"voice")){
        voice = get_param(buf, 3, bytes, 1);
        if (voice == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting voice to %s", voice);
        if (who == 0) ret = set_voice_self(fd, voice);        
        else if (who == 1) ret = set_voice_uid(uid, voice);
        else if (who == 2) ret = set_voice_all(voice);
        if (ret) return ERR_COULDNT_SET_VOICE;
        return OK_VOICE_SET;
    }
    else if (!strcmp(param,"punctuation")){
        punct = get_param(buf, 3, bytes, 1);
        if (punct == NULL) return ERR_MISSING_PARAMETER;
        if(!strcmp(punct,"all")) punct_mode = PUNCT_ALL;
        else if(!strcmp(punct,"some")) punct_mode = PUNCT_SOME;        
        else if(!strcmp(punct,"none")) punct_mode = PUNCT_NONE;        
        else return ERR_PARAMETER_INVALID;

        if (who == 0) ret = set_punctuation_mode_self(fd, punct_mode);        
        else if (who == 1) ret = set_punctuation_mode_uid(uid, punct_mode);
        else if (who == 2) ret = set_punctuation_mode_all(punct_mode);

        MSG(4, "Setting punctuation mode to %s \n", punct);
        if (ret) return ERR_COULDNT_SET_PUNCT_MODE;
        return OK_PUNCT_MODE_SET;
    }
    else if (!strcmp(param,"punctuation_table")){
        punctuation_table = get_param(buf, 3, bytes, 0);
        if (punctuation_table == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting punctuation table to %s \n", punctuation_table);

        if (who == 0) ret = set_punctuation_table_self(fd, punctuation_table);        
        else if (who == 1) ret = set_punctuation_table_uid(uid, punctuation_table);
        else if (who == 2) ret = set_punctuation_table_all(punctuation_table);

        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (!strcmp(param,"character_table")){
        char_table = get_param(buf, 3, bytes, 0);
        if (char_table == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting punctuation table to %s \n", char_table);
        if (who == 0) ret = set_character_table_self(fd, char_table);
        else if (who == 1) ret = set_character_table_uid(uid, char_table);
        else if (who == 2) ret = set_character_table_all(char_table);
        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (!strcmp(param,"key_table")){
        key_table = get_param(buf, 3, bytes, 0);
        if (key_table == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting punctuation table to %s \n", key_table);

        if (who == 0) ret = set_key_table_self(fd, key_table);        
        else if (who == 1) ret = set_key_table_uid(uid, key_table);
        else if (who == 2) ret = set_key_table_all(key_table);

        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (!strcmp(param,"text_table")){
        return OK_NOT_IMPLEMENTED;
    }
    else if (!strcmp(param,"sound_table")){
        sound_table = get_param(buf, 3, bytes, 0);
        if (sound_table == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting punctuation table to %s \n", sound_table);

        if (who == 0) ret = set_sound_table_self(fd, sound_table);        
        else if (who == 1) ret = set_sound_table_uid(uid, sound_table);
        else if (who == 2) ret = set_sound_table_all(sound_table);

        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (!strcmp(param,"cap_let_recogn")){
        recog = get_param(buf, 3, bytes, 1);
        if (recog == NULL) return ERR_MISSING_PARAMETER;

        if(!strcmp(recog,"on")) onoff = 1;
        else if(!strcmp(recog,"off")) onoff = 0;        
        else return ERR_PARAMETER_NOT_ON_OFF;

        if (who == 0) ret = set_capital_letter_recognition_self(fd, onoff);        
        else if (who == 1) ret = set_capital_letter_recognition_uid(uid, onoff);
        else if (who == 2) ret = set_capital_letter_recognition_all(onoff);

        MSG(4, "Setting capital letter recognition to %s \n", punct);
        if (ret) return ERR_COULDNT_SET_CAP_LET_RECOG;
        return OK_CAP_LET_RECOGN_SET;
    }
    else if (!strcmp(param,"spelling")){
        spelling = get_param(buf, 3, bytes, 1);
        if (spelling == NULL) return ERR_MISSING_PARAMETER;
        if(!strcmp(spelling,"on")) onoff = 1;
        else if(!strcmp(spelling,"off")) onoff = 0;        
        else return ERR_PARAMETER_NOT_ON_OFF;

        if (who == 0) ret = set_spelling_self(fd, onoff);        
        else if (who == 1) ret = set_spelling_uid(uid, onoff);
        else if (who == 2) ret = set_spelling_all(onoff);

        MSG(4, "Setting spelling to %s", spelling);

        if (ret) return ERR_COULDNT_SET_SPELLING;
        return OK_SPELLING_SET;
    }
    else{
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

    param = get_param(buf, 1, bytes, 1);
    if (param == NULL) return ERR_MISSING_PARAMETER;

    if (!strcmp(param,"all")){
        speaking_pause_all(fd);
    }
    else if (!strcmp(param, "self")){
        uid = get_client_uid_by_fd(fd);
        if(uid == 0) return ERR_INTERNAL;
        speaking_pause(fd, uid);
    }
    else if (isanum(param)){
        uid = atoi(param);
        if (uid <= 0) return ERR_ID_NOT_EXIST;
        speaking_pause(fd, uid);
    }else{
        return ERR_PARAMETER_INVALID;
    }

    MSG(4, "Pause received.");

    return OK_PAUSED;
}

char*
parse_resume(char *buf, int bytes, int fd)
{
    int ret;
    int uid = 0;
    char *param;

    param = get_param(buf,1,bytes, 1);
    if (param == NULL) return ERR_MISSING_PARAMETER;

    if (!strcmp(param,"all")){
        speaking_resume_all();
    }
    else if (!strcmp(param, "self")){
        uid = get_client_uid_by_fd(fd);
        if(uid == 0) return ERR_INTERNAL;
        speaking_resume(uid);
    }
    else if (isanum(param)){
        uid = atoi(param);
        if (uid <= 0) return ERR_ID_NOT_EXIST;
        speaking_resume(uid);
    }else{
        return ERR_PARAMETER_INVALID;
    }

    MSG(4, "Resume received.");
    
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
    else if(!strcmp(param, "punctuation_tables")){
        return (char*) sndicon_list_punctuation_tables();
    }
	else if(!strcmp(param, "key_tables")){
        return (char*) sndicon_list_key_tables();
    }
    else if(!strcmp(param, "text_tables")){
        //        return (char*) sndicon_list_text_tables();
        return OK_NOT_IMPLEMENTED;
    }
    else return ERR_PARAMETER_INVALID;
}

char*
parse_help(char* buf, int bytes, int fd)
{
    char *help;

    help = (char*) spd_malloc(1024 * sizeof(char));

    sprintf(help, 
            C_OK_HELP"-  SPEAK           -- say text \r\n"
            C_OK_HELP"-  KEY             -- say a combination of keys \r\n"
            C_OK_HELP"-  CHAR            -- say a char \r\n"
            C_OK_HELP"-  SOUND_ICON      -- execute a sound icon \r\n"
            C_OK_HELP"-  SET             -- set a parameter \r\n"
            C_OK_HELP"-  QUIT            -- close the connection \r\n"
            C_OK_HELP"-  LIST            -- list available tables \r\n"
            C_OK_HELP"-  HISTORY         -- commands related to history \r\n"
            OK_HELP_SENT);

    return help;
}
        
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

    if(z<=0) return NULL;   

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
