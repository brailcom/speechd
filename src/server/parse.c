
/*
 * parse.c - Parses commands Speech Dispatcher got from client
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
 * $Id: parse.c,v 1.34 2003-06-20 00:43:11 hanke Exp $
 */

#include "speechd.h"

/*
  Parse() receives input data and parses them. It can
  be either command or data to speak. If it's command, it
  is immadetaily executed (eg. parameters are set). If it's
  data to speak, they are queued in corresponding queues
  with corresponding parameters for synthesis.
*/

#define CHECK_SSIP_COMMAND(cmd_name, parse_function)\
 if(!strcmp(command, cmd_name)){ \
    spd_free(command); \
    return (char*) (parse_function) (buf, bytes, fd); \
 }


char* 
parse(const char *buf, const int bytes, const int fd)
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

        CHECK_SSIP_COMMAND("set", parse_set);
        CHECK_SSIP_COMMAND("history", parse_history);
        CHECK_SSIP_COMMAND("stop", parse_stop);
        CHECK_SSIP_COMMAND("cancel", parse_cancel);
        CHECK_SSIP_COMMAND("pause", parse_pause);
        CHECK_SSIP_COMMAND("resume", parse_resume);
        CHECK_SSIP_COMMAND("sound_icon", parse_snd_icon);
        CHECK_SSIP_COMMAND("char", parse_char);
        CHECK_SSIP_COMMAND("key", parse_key)
        CHECK_SSIP_COMMAND("list", parse_list);
        CHECK_SSIP_COMMAND("char", parse_char);
        CHECK_SSIP_COMMAND("help", parse_help);		

        if (!strcmp(command,"bye") || !strcmp(command,"quit")){
            MSG(4, "Bye received.");
            /* Send a reply to the socket */
            write(fd, OK_BYE, strlen(OK_BYE)+1);
            speechd_connection_destroy(fd);
            /* This is internal Speech Dispatcher message, see serve() */
            spd_free(command);
            return "999 CLIENT GONE";
        }
	
        if (!strcmp(command,"speak")){
            /* Ckeck if we have enough space in awaiting_data table for
             * this client, that can have higher file descriptor that
             * everything we got before */
            r = server_data_on(fd);
            if (r!=0){
                if(SPEECHD_DEBUG) FATAL("Can't switch to data on mode\n");
                spd_free(command);
                return "ERR INTERNAL";								 
            }
            spd_free(command);
            return OK_RECEIVE_DATA;
        }
        spd_free(command);
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
            /* Remove */
            if (o_bytes[fd] == 0) return OK_MSG_CANCELED;          
            new = (TSpeechDMessage*) spd_malloc(sizeof(TSpeechDMessage));
            new->bytes = o_bytes[fd];
            new->buf = (char*) spd_malloc(new->bytes + 1);

            assert(new->bytes >= 0); 
            assert(o_buf[fd] != NULL);

            memcpy(new->buf, o_buf[fd]->str, new->bytes);
            new->buf[new->bytes] = 0;
            MSG(4, "New buf is now: |%s|", new->buf);		
            if(queue_message(new, fd, 1, MSGTYPE_TEXT, 0) != 0){
                if(SPEECHD_DEBUG) FATAL("Can't queue message\n");
                free(new->buf);
                free(new);
                return ERR_INTERNAL;
            }			       

            /* Clear the counter of bytes in the output buffer. */
            server_data_off(fd);
            return OK_MESSAGE_QUEUED;
        }

        {
            int real_bytes;
            if(bytes>=5){
                if(pos = strstr(buf,"\r\n.\r\n")){	
                    real_bytes=pos-buf;
                    end_data=1;		
                    MSG(4,"Command in data caught");
                }else{
                    real_bytes = bytes;
                }
            }else{
                real_bytes = bytes;
            }      
            /* Get the number of bytes read before, sum it with the number of bytes read
             * now and store again in the counter */        
            o_bytes[fd] += real_bytes;       

            g_string_insert_len(o_buf[fd], -1, buf, real_bytes);
        }
    }

    if (end_data == 1) goto enddata;

    /* Don't reply on data */
    return "";

}

/* Parses @history commands and calls the appropriate history_ functions. */
char*
parse_history(const char *buf, const int bytes, const int fd)
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
parse_set(const char *buf, const int bytes, const int fd)
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
    char *output_module;
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
        priority = get_param(buf,3,bytes, 1);
        if (priority == NULL) return ERR_MISSING_PARAMETER;
        if (!strcmp(priority, "important")) helper = 1;
        else if (!strcmp(priority, "text")) helper = 2;
        else if (!strcmp(priority, "message")) helper = 3;
        else if (!strcmp(priority, "notification")) helper = 4;
        else if (!strcmp(priority, "progress")) helper = 5;
        else return ERR_UNKNOWN_PRIORITY;
        MSG(4, "Setting priority to %s = %d \n", priority, helper);
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
    else if (!strcmp(param,"output_module")){
        output_module = get_param(buf, 3, bytes, 0);
        if (output_module == NULL) return ERR_MISSING_PARAMETER;
        MSG(4, "Setting output module to %s \n", output_module);
        if (who == 0) ret = set_output_module_self(fd, output_module);
        else if (who == 1) ret = set_output_module_uid(uid, output_module);
        else if (who == 2) ret = set_output_module_all(output_module);
        if (ret) return ERR_COULDNT_SET_OUTPUT_MODULE;
        return OK_OUTPUT_MODULE_SET;
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
parse_stop(const char *buf, const int bytes, const int fd)
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
parse_cancel(const char *buf, const int bytes, const int fd)
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
parse_pause(const char *buf, const int bytes, const int fd)
{
    int ret;
    int uid = 0;
    char *param;

    param = get_param(buf, 1, bytes, 1);
    if (param == NULL) return ERR_MISSING_PARAMETER;

    if (!strcmp(param,"all")){
        pause_requested = 1;
        pause_requested_fd = fd;
        sem_post(sem_messages_waiting);
    }
    else if (!strcmp(param, "self")){
        uid = get_client_uid_by_fd(fd);
        if(uid == 0) return ERR_INTERNAL;
        pause_requested = 2;
        pause_requested_fd = fd;
        pause_requested_uid = uid;
        sem_post(sem_messages_waiting);
    }
    else if (isanum(param)){
        uid = atoi(param);
        if (uid <= 0) return ERR_ID_NOT_EXIST;
        pause_requested = 2;
        pause_requested_fd = fd;
        pause_requested_uid = uid;
        sem_post(sem_messages_waiting);
    }else{
        return ERR_PARAMETER_INVALID;
    }

    MSG(4, "Pause received.");

    return OK_PAUSED;
}

char*
parse_resume(const char *buf, const int bytes, const int fd)
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
parse_snd_icon(const char *buf, const int bytes, const int fd)
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
parse_char(const char *buf, const int bytes, const int fd)
{
    char *param;
    TFDSetElement *settings;
    int ret;

    param = get_param(buf,1,bytes, 0);
    if (param == NULL) return ERR_MISSING_PARAMETER;
    MSG(4,"Parameter cught: %s", param);

    ret = sndicon_char(fd, param);
    if (ret!=0){
        if(ret == 1) return ERR_NO_SND_ICONS;
        if(ret == 2) return ERR_UNKNOWN_ICON;
    }
	
    return OK_SND_ICON_QUEUED;
}

char*
parse_key(const char* buf, const int bytes, const int fd)
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
parse_list(const char* buf, const int bytes, const int fd)
{
    char *param;
    char *voice_list;

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
    else if(!strcmp(param, "voices")){
        voice_list = (char*) spd_malloc(1024);
        sprintf(voice_list,
                C_OK_VOICES"-MALE1\r\n"
                C_OK_VOICES"-MALE2\r\n"
                C_OK_VOICES"-MALE3\r\n"
                C_OK_VOICES"-FEMALE1\r\n"
                C_OK_VOICES"-FEMALE2\r\n"
                C_OK_VOICES"-FEMALE3\r\n"
                C_OK_VOICES"-CHILD_MALE\r\n"
                C_OK_VOICES"-CHILD_FEMALE\r\n"
                OK_VOICE_LIST_SENT);
        return voice_list;
    }
    else return ERR_PARAMETER_INVALID;
}

char*
parse_help(const char* buf, const int bytes, const int fd)
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
isanum(const char *str){
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
get_param(const char *buf, const int n, const int bytes, const int lower_case)
{
    char* param;
    int i, y, z;
    int quote_open = 0;

    param = (char*) spd_malloc(bytes);
    assert(param != NULL);
	
    strcpy(param,"");
    i = 0;
        
    /* Read all the parameters one by one,
     * but stop after the one with index n,
     * while maintaining it's value in _param_ */
    for(y=0; y<=n; y++){
        z=0;
        for(; i<bytes; i++){
            /*            if((buf[i] == '\"')&&(!quote_open)){
                quote_open = 1;
                continue;
            }
            if((buf[i] == '\"')&&(quote_open)){
                quote_open = 0;
                continue;
                }
                if(quote_open!=1){ */
            if (buf[i] == ' ') break;
                //            }
            param[z] = buf[i];
            z++;
        }
        i++;
    }

    if(z <= 0){
        free(param);
        return NULL;   
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
