
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
 * $Id: parse.c,v 1.40 2003-07-16 19:18:24 hanke Exp $
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
                return ERR_INTERNAL;								 
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
#undef CHECK_SSIP_COMMAND

#define CHECK_PARAM(param) \
    if (param == NULL){ \
       MSG(3, "Missing parameter from client"); \
       return ERR_MISSING_PARAMETER; \
    } 

#define GET_PARAM_INT(name, pos) \
   { \
       char *helper; \
       helper = get_param(buf, pos, bytes, 0); \
       CHECK_PARAM(helper); \
       if (!isanum(helper)) return ERR_NOT_A_NUMBER; \
       name = atoi(helper); \
   }

#define CONV_DOWN 1
#define NO_CONV 0

#define GET_PARAM_STR(name, pos, up_lo_case) \
       name = get_param(buf, pos, bytes, up_lo_case); \
       CHECK_PARAM(name);

/* Tests if cmd is the same as str AND deallocates cmd if
   the test is succesful */
#define TEST_CMD(cmd, str) \
    (!strcmp(cmd, str) ? spd_free(cmd), 1 : 0 )

/* Parses @history commands and calls the appropriate history_ functions. */
char*
parse_history(const char *buf, const int bytes, const int fd)
{
    char *cmd_main;
    GET_PARAM_STR(cmd_main, 1, CONV_DOWN);

    if (TEST_CMD(cmd_main, "get")){
        char *hist_get_sub;
        GET_PARAM_STR(hist_get_sub, 2, CONV_DOWN);

        if (TEST_CMD(hist_get_sub, "client_list")){
            return (char*) history_get_client_list();
        }  
        else if (TEST_CMD(hist_get_sub, "client_id")){
            return (char*) history_get_client_id(fd);
        }  
        else if (TEST_CMD(hist_get_sub, "client_messages")){
            int start, num;
            char *who;

            /* TODO: This needs to be (sim || am)-plified */
            who = get_param(buf,3,bytes, 1);
            CHECK_PARAM(who);
            if (!strcmp(who, "self")) return ERR_NOT_IMPLEMENTED;
            if (!strcmp(who, "all")) return ERR_NOT_IMPLEMENTED;                          
            if (!isanum(who)) return ERR_NOT_A_NUMBER;
            
            GET_PARAM_INT(start, 4);
            GET_PARAM_INT(num, 5);

            return (char*) history_get_message_list(atoi(who), start, num);
        }  
        else if (TEST_CMD(hist_get_sub, "last")){
            return (char*) history_get_last(fd);
        }
        else if (TEST_CMD(hist_get_sub, "message")){
            int msg_id;
            GET_PARAM_INT(msg_id, 3);
            return (char*) history_get_message(msg_id);
        }else{
            return ERR_MISSING_PARAMETER;
        }
    }
    else if (TEST_CMD(cmd_main, "cursor")){       
        char *hist_cur_sub;
        GET_PARAM_STR(hist_cur_sub, 2, CONV_DOWN);

        if (TEST_CMD(hist_cur_sub, "set")){
            int who;
            char *location;

            GET_PARAM_INT(who, 3);
            GET_PARAM_STR(location, 4, CONV_DOWN);            

            if (TEST_CMD(location,"last")){
                return (char*) history_cursor_set_last(fd, who);
            }
            else if (TEST_CMD(location,"first")){
                return (char*) history_cursor_set_first(fd, who);
            }
            else if (TEST_CMD(location,"pos")){
                int pos;
                GET_PARAM_INT(pos, 5);
                return (char*) history_cursor_set_pos(fd, who, pos);
            }
            else{
                spd_free(location);
                return ERR_MISSING_PARAMETER;
            }
        }
        else if (TEST_CMD(hist_cur_sub, "forward")){
            return (char*) history_cursor_forward(fd);
        }
        else if (TEST_CMD(hist_cur_sub,"backward")){
            return (char*) history_cursor_backward(fd);
        }
        else if (TEST_CMD(hist_cur_sub,"get")){
            return (char*) history_cursor_get(fd);
        }else{
            spd_free(hist_cur_sub);
            return ERR_MISSING_PARAMETER;
        }
            
    }
    else if (TEST_CMD(cmd_main,"say")){
        int msg_id;
        GET_PARAM_INT(msg_id, 2);
        return (char*) history_say_id(fd, msg_id);
    }
    else if (TEST_CMD(cmd_main,"sort")){
        // TODO: everything :)
        return ERR_NOT_IMPLEMENTED;
    }
    else{
        spd_free(cmd_main);
        return ERR_MISSING_PARAMETER;
    }

 
    return ERR_INVALID_COMMAND;
}

#define SSIP_SET_COMMAND(param) \
        if (who == 0) ret = set_ ## param ## _self(fd, param); \
        else if (who == 1) ret = set_ ## param ## _uid(uid, param); \
        else if (who == 2) ret = set_ ## param ## _all(param); \

char*
parse_set(const char *buf, const int bytes, const int fd)
{
    int who;                    /* 0 - self, 1 - uid specified, 2 - all */
    int uid;                    /* uid of the client (only if who == 1) */
    int ret;  
    char *set_sub;

    char *who_s;

    GET_PARAM_STR(who_s, 1, CONV_DOWN);

    if (TEST_CMD(who_s, "self")) who = 0;
    else if (TEST_CMD(who_s, "all")) who = 2;
    else if (isanum(who_s)){
        who = 1;
        uid = atoi(who_s);
        spd_free(who_s);
    }else{
        spd_free(who_s);
        return ERR_PARAMETER_INVALID;
    }

    GET_PARAM_STR(set_sub, 2, CONV_DOWN);

    if (TEST_CMD(set_sub, "priority")){
        char *priority_s;
        int priority;

        /* Setting priority only allowed for "self" */
        if (who != 0) return ERR_COULDNT_SET_PRIORITY; 
        GET_PARAM_STR(priority_s, 3, CONV_DOWN);

        if (TEST_CMD(priority_s, "important")) priority = 1;
        else if (TEST_CMD(priority_s, "text")) priority = 2;
        else if (TEST_CMD(priority_s, "message")) priority = 3;
        else if (TEST_CMD(priority_s, "notification")) priority = 4;
        else if (TEST_CMD(priority_s, "progress")) priority = 5;
        else{
            spd_free(priority_s);
            return ERR_UNKNOWN_PRIORITY;
        }

        ret = set_priority_self(fd, priority);
        if (ret) return ERR_COULDNT_SET_PRIORITY;	
        return OK_PRIORITY_SET;
    }
    else if (TEST_CMD(set_sub, "language")){
        char *language;
        GET_PARAM_STR(language, 3, CONV_DOWN);

        SSIP_SET_COMMAND(language);
        if (ret) return ERR_COULDNT_SET_LANGUAGE;
        return OK_LANGUAGE_SET;
    }
    else if (TEST_CMD(set_sub, "spelling_table")){
        char *spelling_table;
        GET_PARAM_STR(spelling_table, 3, CONV_DOWN);

        SSIP_SET_COMMAND(spelling_table);
        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (TEST_CMD(set_sub, "client_name")){
        char *client_name;

        /* Setting client name only allowed for "self" */
        if (who != 0) return ERR_COULDNT_SET_CLIENT_NAME;
      
        GET_PARAM_STR(client_name, 3, CONV_DOWN);

        ret = set_client_name_self(fd, client_name);       
        spd_free(client_name);

        if (ret) return ERR_COULDNT_SET_CLIENT_NAME;
        return OK_CLIENT_NAME_SET;
    }
    else if (!strcmp(set_sub, "rate")){
        signed int rate;
        GET_PARAM_INT(rate, 3);

        if(rate < -100) return ERR_RATE_TOO_LOW;
        if(rate > +100) return ERR_RATE_TOO_HIGH;

        SSIP_SET_COMMAND(rate);
        if (ret) return ERR_COULDNT_SET_RATE;
        return OK_RATE_SET;
    }
    else if (TEST_CMD(set_sub, "pitch")){
        signed int pitch;
        GET_PARAM_INT(pitch, 3);

        if(pitch < -100) return ERR_PITCH_TOO_LOW;
        if(pitch > +100) return ERR_PITCH_TOO_HIGH;

        SSIP_SET_COMMAND(pitch);

        if (ret) return ERR_COULDNT_SET_PITCH;
        return OK_PITCH_SET;
    }
    else if (TEST_CMD(set_sub, "voice")){
        char *voice;
        GET_PARAM_STR(voice, 3, CONV_DOWN);

        SSIP_SET_COMMAND(voice);
        spd_free(voice);

        if (ret) return ERR_COULDNT_SET_VOICE;
        return OK_VOICE_SET;
    }
    else if (TEST_CMD(set_sub, "punctuation")){
        char *punct_s;
        EPunctMode punctuation_mode;
        GET_PARAM_STR(punct_s, 3, CONV_DOWN);

        if(TEST_CMD(punct_s,"all")) punctuation_mode = PUNCT_ALL;
        else if(TEST_CMD(punct_s,"some")) punctuation_mode = PUNCT_SOME;        
        else if(TEST_CMD(punct_s,"none")) punctuation_mode = PUNCT_NONE;        
        else{
            spd_free(punct_s);
            return ERR_PARAMETER_INVALID;
        }

        SSIP_SET_COMMAND(punctuation_mode);

        if (ret) return ERR_COULDNT_SET_PUNCT_MODE;
        return OK_PUNCT_MODE_SET;
    }
    else if (TEST_CMD(set_sub, "punctuation_table")){
        char *punctuation_table;
        GET_PARAM_STR(punctuation_table, 3, CONV_DOWN);

        SSIP_SET_COMMAND(punctuation_table);
        spd_free(punctuation_table);

        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (TEST_CMD(set_sub, "character_table")){
        char *character_table;
        GET_PARAM_STR(character_table, 3, CONV_DOWN);

        SSIP_SET_COMMAND(character_table);
        spd_free(character_table);

        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (TEST_CMD(set_sub, "output_module")){
        char *output_module;
        GET_PARAM_STR(output_module, 3, CONV_DOWN);

        SSIP_SET_COMMAND(output_module);
        spd_free(output_module);

        if (ret) return ERR_COULDNT_SET_OUTPUT_MODULE;
        return OK_OUTPUT_MODULE_SET;
    }
    else if (TEST_CMD(set_sub, "key_table")){
        char *key_table;
        GET_PARAM_STR(key_table, 3, CONV_DOWN);
        
        SSIP_SET_COMMAND(key_table);
        spd_free(key_table);

        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (TEST_CMD(set_sub, "text_table")){
        return OK_NOT_IMPLEMENTED;
    }
    else if (TEST_CMD(set_sub,"sound_table")){
        char *sound_table;
        GET_PARAM_STR(sound_table, 3, CONV_DOWN);

        SSIP_SET_COMMAND(sound_table);
        spd_free(sound_table);

        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (TEST_CMD(set_sub,"cap_let_recogn_table")){
        char *cap_let_recogn_table;
        GET_PARAM_STR(cap_let_recogn_table, 3, CONV_DOWN);

        SSIP_SET_COMMAND(cap_let_recogn_table);
        spd_free(cap_let_recogn_table);

        if (ret) return ERR_COULDNT_SET_TABLE;
        return OK_TABLE_SET;
    }
    else if (TEST_CMD(set_sub, "cap_let_recogn")){
        int capital_letter_recognition;
        char *recognition;

        GET_PARAM_STR(recognition, 3, CONV_DOWN);

        if(TEST_CMD(recognition, "none")) capital_letter_recognition = RECOGN_NONE;
        else if(TEST_CMD(recognition, "spell")) capital_letter_recognition = RECOGN_SPELL;        
        else if(TEST_CMD(recognition, "icon")) capital_letter_recognition = RECOGN_ICON;        
        else{
            spd_free(recognition);
            return ERR_PARAMETER_INVALID;
        }

        SSIP_SET_COMMAND(capital_letter_recognition);

        if (ret) return ERR_COULDNT_SET_CAP_LET_RECOG;
        return OK_CAP_LET_RECOGN_SET;
    }
    else if (TEST_CMD(set_sub,"spelling")){
        char *spelling_s;
        int spelling;
        GET_PARAM_STR(spelling_s, 3, CONV_DOWN);

        if(TEST_CMD(spelling_s, "on")) spelling = 1;
        else if(TEST_CMD(spelling_s, "off")) spelling = 0;        
        else{
            spd_free(spelling_s);
            return ERR_PARAMETER_NOT_ON_OFF;
        }

        SSIP_SET_COMMAND(spelling);

        if (ret) return ERR_COULDNT_SET_SPELLING;
        return OK_SPELLING_SET;
    }
    else{
        return ERR_PARAMETER_INVALID;
    }

    return ERR_INVALID_COMMAND;
}
#undef SSIP_SET_COMMAND

char*
parse_stop(const char *buf, const int bytes, const int fd)
{
    int uid = 0;
    char *who_s;

    MSG(4, "Stop received from fd %d.", fd);

    GET_PARAM_STR(who_s, 1, CONV_DOWN);

    if (TEST_CMD(who_s, "all")){
        speaking_stop_all();
    }
    else if (TEST_CMD(who_s, "self")){
        uid = get_client_uid_by_fd(fd);
        if(uid == 0) return ERR_INTERNAL;
        speaking_stop(uid);
    }
    else if (isanum(who_s)){
        uid = atoi(who_s);
        spd_free(who_s);

        if (uid <= 0) return ERR_ID_NOT_EXIST;
        speaking_stop(uid);
    }else{
        spd_free(who_s);
        return ERR_PARAMETER_INVALID;
    }

    return OK_STOPPED;
}

char*
parse_cancel(const char *buf, const int bytes, const int fd)
{
    int uid = 0;
    char *who_s;

    MSG(4, "Cancel received from fd %d.", fd);

    GET_PARAM_STR(who_s, 1, CONV_DOWN);

    if (TEST_CMD(who_s,"all")){
        speaking_cancel_all();
    }
    else if (TEST_CMD(who_s, "self")){
        uid = get_client_uid_by_fd(fd);
        if(uid == 0) return ERR_INTERNAL;
        speaking_cancel(uid);
    }
    else if (isanum(who_s)){
        uid = atoi(who_s);
        spd_free(who_s);

        if (uid <= 0) return ERR_ID_NOT_EXIST;
        speaking_cancel(uid);
    }else{
        spd_free(who_s);
        return ERR_PARAMETER_INVALID;
    }

    return OK_CANCELED;
}

char*
parse_pause(const char *buf, const int bytes, const int fd)
{
    int ret;
    int uid = 0;
    char *who_s;

    MSG(4, "Pause received from fd %d.", fd);

    GET_PARAM_STR(who_s, 1, CONV_DOWN);

    /* Note: In this case, the semaphore has a special meaning
       to allow the speaking loop detect the request for pause */

    if (TEST_CMD(who_s, "all")){
        pause_requested = 1;
        pause_requested_fd = fd;
        sem_post(sem_messages_waiting);
    }
    else if (TEST_CMD(who_s, "self")){
        uid = get_client_uid_by_fd(fd);
        if(uid == 0) return ERR_INTERNAL;
        pause_requested = 2;
        pause_requested_fd = fd;
        pause_requested_uid = uid;
        sem_post(sem_messages_waiting);
    }
    else if (isanum(who_s)){
        uid = atoi(who_s);
        spd_free(who_s);
        if (uid <= 0) return ERR_ID_NOT_EXIST;
        pause_requested = 2;
        pause_requested_fd = fd;
        pause_requested_uid = uid;
        sem_post(sem_messages_waiting);
    }else{
        spd_free(who_s);
        return ERR_PARAMETER_INVALID;
    }

    return OK_PAUSED;
}

char*
parse_resume(const char *buf, const int bytes, const int fd)
{
    int ret;
    int uid = 0;
    char *who_s;

    MSG(4, "Resume received from fd %d.", fd);

    GET_PARAM_STR(who_s, 1, CONV_DOWN);

    if (TEST_CMD(who_s, "all")){
        speaking_resume_all();
    }
    else if (TEST_CMD(who_s, "self")){
        uid = get_client_uid_by_fd(fd);
        if(uid == 0) return ERR_INTERNAL;
        speaking_resume(uid);
    }
    else if (isanum(who_s)){
        uid = atoi(who_s);
        spd_free(who_s);
        if (uid <= 0) return ERR_ID_NOT_EXIST;
        speaking_resume(uid);
    }else{
        spd_free(who_s);
        return ERR_PARAMETER_INVALID;
    }
    
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
    char *character;
    int ret;

    GET_PARAM_STR(character, 1, NO_CONV);

    if (character == NULL) return ERR_MISSING_PARAMETER;
    if (character[0] == 0) return ERR_MISSING_PARAMETER;

    ret = sndicon_char(fd, character);
    if (ret != 0){
        TSpeechDMessage *verbat;
        /* Use the character verbatim */
        verbat = (TSpeechDMessage*) spd_malloc(sizeof(TSpeechDMessage));
        verbat->bytes = strlen(character);
        verbat->buf = strdup(character);
        if(queue_message(verbat, fd, 1, MSGTYPE_TEXTP, 0)){
            if (SPEECHD_DEBUG) FATAL("Couldn't queue message\n");
            MSG(1, "Couldn't queue message\n");            
        }
    }

    spd_free(character);

    return OK_SND_ICON_QUEUED;
}

char*
parse_key(const char* buf, const int bytes, const int fd)
{
    char *key;
    int ret;

    GET_PARAM_STR(key, 1, NO_CONV);

    ret = sndicon_key(fd, key);

    if (ret != 0){
        if(g_unichar_isalpha(g_utf8_get_char(key))){
            TSpeechDMessage *verbat;
            /* Use the key verbatim */
            verbat = (TSpeechDMessage*) spd_malloc(sizeof(TSpeechDMessage));
            verbat->bytes = strlen(key);
            verbat->buf = strdup(key);
            if(queue_message(verbat, fd, 1, MSGTYPE_TEXTP, 0))  FATAL("Couldn't queue message\n");
        }else{
            spd_free(key);
            return ERR_UNKNOWN_ICON;
        }
    }

    spd_free(key);
    return OK_SND_ICON_QUEUED;
}

char*
parse_list(const char* buf, const int bytes, const int fd)
{
    char *list_type;
    char *voice_list;

    GET_PARAM_STR(list_type, 1, CONV_DOWN);

    if(TEST_CMD(list_type,"spelling_tables")){
        return (char*) sndicon_list_spelling_tables();
    }
    else if(TEST_CMD(list_type,"sound_tables")){
        return (char*) sndicon_list_sound_tables();
    }
    else if(TEST_CMD(list_type, "character_tables")){
        return (char*) sndicon_list_char_tables();
    }
    else if(TEST_CMD(list_type, "punctuation_tables")){
        return (char*) sndicon_list_punctuation_tables();
    }
    else if(TEST_CMD(list_type, "key_tables")){
        return (char*) sndicon_list_key_tables();
    }
    else if(TEST_CMD(list_type, "cap_let_recogn_tables")){
        return (char*) sndicon_list_cap_let_recogn_tables();
    }
    else if(TEST_CMD(list_type, "text_tables")){
        //        return (char*) sndicon_list_text_tables();
        return OK_NOT_IMPLEMENTED;
    }
    else if(TEST_CMD(list_type, "voices")){
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
    else{
        spd_free(list_type);
        return ERR_PARAMETER_INVALID;
    }
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
            C_OK_HELP"-  LIST            -- list available tables \r\n"
            C_OK_HELP"-  HISTORY         -- commands related to history \r\n"
            C_OK_HELP"-  QUIT            -- close the connection \r\n"
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
