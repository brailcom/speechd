
/*
 * libspeechd.c - Shared library for easy acces to Speech Dispatcher functions
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
 * $Id: libspeechd.c,v 1.11 2003-10-12 23:25:21 hanke Exp $
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <glib.h>
#include <assert.h>

#include "def.h"
#include "libspeechd.h"

/* --------------------- Public functions ------------------------- */

/* Opens a new Speech Dispatcher connection.
 * Returns socket file descriptor of the created connection
 * or 0 if no connection was opened. */
int 
spd_open(const char* client_name, const char* connection_name, const char* user_name)
{
  struct sockaddr_in address;
  int connection;
  char *set_client_name;
  char *reply;
  char* conn_name;
  char* usr_name;
  char *env_port;
  int port;
  int ret;

  if (client_name == NULL) return 0;

  if (user_name == NULL){
      usr_name = (char*) g_get_user_name();
  }else{
      usr_name = strdup(user_name);
  }

  if(connection_name == NULL){
      conn_name = (char*) xmalloc(6);
      strcpy(conn_name, "main");
  }else{
      conn_name = strdup(connection_name);
  }

  env_port = getenv("SPEECHD_PORT");
  if (env_port != NULL)
      port = strtol(env_port, NULL, 10);
  else
      port = SPEECHD_DEFAULT_PORT;
  
  /* Prepare a new socket */
  address.sin_addr.s_addr = inet_addr("127.0.0.1");
  address.sin_port = htons(port);
  address.sin_family = AF_INET;
  connection = socket(AF_INET, SOCK_STREAM, 0);

#ifdef LIBSPEECHD_DEBUG
  spd_debug = fopen("/tmp/libspeechd_debug", "w");
  if (spd_debug == NULL) FATAL("COULDN'T ACCES FILE INTENDED FOR DEBUG");
#endif /* LIBSPEECHD_DEBUG */

  /* Connect to server */
  ret = connect(connection, (struct sockaddr *)&address, sizeof(address));
  if (ret == -1) return 0;
 
  set_client_name = xmalloc((strlen(client_name) + strlen(conn_name)
                            + strlen(usr_name) + 32) * sizeof(char));

  sprintf(set_client_name, "SET SELF CLIENT_NAME \"%s:%s:%s\"", usr_name,
          client_name, conn_name);

  ret = spd_execute_command(connection, set_client_name);

  xfree(usr_name);
  xfree(conn_name);
  xfree(set_client_name);

  return connection;
}


/* Close a Speech Dispatcher connection */
void
spd_close(int connection)
{
  char command[16];

  sprintf(command, "QUIT");
  spd_execute_command(connection, command);
    
   /* close the socket */
   close(connection);
}

/* Say TEXT with priority PRIORITY.
 * Returns 0 on success, -1 otherwise. */                            
int
spd_say(int connection, SPDPriority priority, const char* text)
{
    static char command[64];
    static int i;
    char *buf;
    char *pos;
    int ret;

    if (text == NULL) return -1;

    /* Set priority */

    ret = spd_set_priority(connection, priority);
    if(ret) return -1;
    
    /* Check if there is no escape sequence in the text */
    /* TODO: This needs to be fixed! */
    //  while(pos = (char*) strstr(text,"\r\n.\r\n")){
    //	text[pos-text+3] = ' ';
    // }
  
    /* Start the data flow */
    sprintf(command, "SPEAK");
    ret = spd_execute_command(connection, command);
    if(ret){     
        SPD_DBG("Can't start data flow");
        return -1;
    }
  
    /* Send data */
    buf = xmalloc((strlen(text) + 4) * sizeof(char));
    sprintf(buf, "%s\r\n", text); 
    spd_send_data(connection, buf, SPD_NO_REPLY);
    xfree(buf);

    /* Terminate data flow */
    sprintf(command, "\r\n.");
    ret = spd_execute_command(connection, command);
    if(ret){
        SPD_DBG("Can't terminate data flow");
        return -1; 
    }

    return 0;
}

/* The same as spd_say, accepts also formated strings */
int
spd_sayf(int fd, SPDPriority priority, const char *format, ...)
{
    static int ret;    
    va_list args;
    char *buf;
    
    /* Print the text to buffer */
    va_start(args, format);
    buf = g_strdup_vprintf(format, args);
    va_end(args);
    
    /* Send the buffer to Speech Dispatcher */
    ret = spd_say(fd, priority, buf);	

    xfree(buf);
    return ret;
}

int
spd_stop(int connection)
{
  return spd_execute_command(connection, "STOP SELF");
}

int
spd_stop_all(int connection)
{
  return spd_execute_command(connection, "STOP ALL");
}

int
spd_stop_uid(int connection, int target_uid)
{
  char command[32];

  sprintf(command, "STOP %d", target_uid);
  return spd_execute_command(connection, command);
}

int
spd_cancel(int connection)
{
  return spd_execute_command(connection, "CANCEL SELF");
}

int
spd_cancel_all(int connection)
{
  return spd_execute_command(connection, "CANCEL ALL");
}

int
spd_cancel_uid(int connection, int target_uid)
{
  char command[16];

  sprintf(command, "CANCEL %d", target_uid);
  return spd_execute_command(connection, command);
}

int
spd_pause(int connection)
{
  return spd_execute_command(connection, "PAUSE SELF");
}

int
spd_pause_all(int connection)
{
  return spd_execute_command(connection, "PAUSE ALL");
}

int
spd_pause_uid(int connection, int target_uid)
{
  char command[16];

  sprintf(command, "PAUSE %d", target_uid);
  return spd_execute_command(connection, command);
}

int
spd_resume(int connection)
{
  return spd_execute_command(connection, "RESUME SELF");
}

int
spd_resume_all(int connection)
{
  return spd_execute_command(connection, "RESUME ALL");
}

int
spd_resume_uid(int connection, int target_uid)
{
  static char command[16];

  sprintf(command, "RESUME %d", target_uid);
  return spd_execute_command(connection, command);
}

int
spd_key(int connection, SPDPriority priority, const char *key_name)
{
    static char command[32];
    char *command_key;
    int ret;

    ret = spd_set_priority(connection, priority);
    if(ret) return -1;

    command_key = g_strdup_printf("KEY \"%s\"", key_name);
    ret = spd_execute_command(connection, command_key);
    if(ret) return -1;

    return 0;
}

int
spd_char(int connection, SPDPriority priority, const char *character)
{
    static char command[32];
    char *command_char;
    int ret;

    if (character == NULL) return -1;

    ret = spd_set_priority(connection, priority);
    if(ret) return -1;

    command_char = g_strdup_printf("CHAR \"%s\"", character);
    ret = spd_execute_command(connection, command_char);
    xfree(command_char);
    if(ret) return -1;

    return 0;
}

int
spd_wchar(int connection, SPDPriority priority, wchar_t wcharacter)
{
    static char command[32];
    char *command_char;
    char character[8];
    int ret;

    ret = wcrtomb(character, wcharacter, NULL);
    if( ret <= 0 ) return -1;

    ret = spd_set_priority(connection, priority);
    if(ret) return -1;

    command_char = g_strdup_printf("CHAR \"%s\"", character);
    ret = spd_execute_command(connection, command_char);
 
    return ret;
}

int
spd_sound_icon(int connection, SPDPriority priority, const char *icon_name)
{
    char *command;
    int ret;

    ret = spd_set_priority(connection, priority);
    if(ret) return -1;

    if (icon_name == NULL) return -1;
    command = g_strdup_printf("SOUND_ICON \"%s\"", icon_name);
    ret = spd_execute_command(connection, command);
    xfree (command);
    
    return ret;
}

#define SPD_SET_COMMAND_INT(param, ssip_name, condition) \
    int \
    spd_w_set_ ## param (int connection, signed int val, const char* who) \
    { \
        static char command[64]; \
        int ret; \
        if ((!condition)) return -1; \
        sprintf(command, "SET %s " #ssip_name " %d", who, val); \
        return spd_execute_command(connection, command); \
    } \
    int \
    spd_set_ ## param (int connection, signed int val) \
    { \
        char who[] = "SELF"; \
        return spd_w_set_ ## param (connection, val, who); \
    } \
    int \
    spd_set_ ## param ## _all(int connection, signed int val) \
    { \
        char who[] = "ALL"; \
        return spd_w_set_ ## param (connection, val, who); \
    } \
    int \
    spd_set_ ## param ## _uid(int connection, signed int val, unsigned int uid) \
    { \
        char who[16]; \
        sprintf(who, "%d", uid); \
        return spd_w_set_ ## param (connection, val, who); \
    }

#define SPD_SET_COMMAND_STR(param, ssip_name) \
    int \
    spd_w_set_ ## param (int connection, const char *str, const char* who) \
    { \
        char *command; \
        if (str == NULL) return -1; \
        command = g_strdup_printf("SET %s " #param " \"%s\"", \
                              who, str); \
        return spd_execute_command(connection, command); \
    } \
    int \
    spd_set_ ## param (int connection, const char *str) \
    { \
        char who[] = "SELF"; \
        return spd_w_set_ ## param (connection, str, who); \
    } \
    int \
    spd_set_ ## param ## _all(int connection, const char *str) \
    { \
        char who[] = "ALL"; \
        return spd_w_set_ ## param (connection, str, who); \
    } \
    int \
    spd_set_ ## param ## _uid(int connection, const char *str, unsigned int uid) \
    { \
        char who[16]; \
        sprintf(who, "%d", uid); \
        return spd_w_set_ ## param (connection, str, who); \
    }

#define SPD_SET_COMMAND_SPECIAL(param, type) \
    int \
    spd_set_ ## param (int connection, type val) \
    { \
        char who[] = "SELF"; \
        return spd_w_set_ ## param (connection, val, who); \
    } \
    int \
    spd_set_ ## param ## _all(int connection, type val) \
    { \
        char who[] = "ALL"; \
        return spd_w_set_ ## param (connection, val, who); \
    } \
    int \
    spd_set_ ## param ## _uid(int connection, type val, unsigned int uid) \
    { \
        char who[16]; \
        sprintf(who, "%d", uid); \
        return spd_w_set_ ## param (connection, val, who); \
    }

SPD_SET_COMMAND_INT(voice_rate, RATE, ((val >= -100) && (val <= +100)) );
SPD_SET_COMMAND_INT(voice_pitch, PITCH, ((val >= -100) && (val <= +100)) );

SPD_SET_COMMAND_STR(language, LANGUAGE);
SPD_SET_COMMAND_STR(punctuation_important, PUNCTUATION_IMPORTANT);
SPD_SET_COMMAND_STR(punctuation_table, PUNCTUATION_TABLE);
SPD_SET_COMMAND_STR(spelling_table, SPELLING_TABLE);
SPD_SET_COMMAND_STR(text_table, TEXT_TABLE);
SPD_SET_COMMAND_STR(sound_table, SOUND_TABLE);
SPD_SET_COMMAND_STR(char_table, CHARACTER_TABLE);
SPD_SET_COMMAND_STR(key_table, KEY_TABLE);

SPD_SET_COMMAND_SPECIAL(punctuation, SPDPunctuation);
SPD_SET_COMMAND_SPECIAL(spelling, SPDSpelling);
SPD_SET_COMMAND_SPECIAL(voice_type, SPDVoiceType);

int
spd_w_set_punctuation(int connection, SPDPunctuation type, const char* who)
{
    static char command[32];
    int ret;

    if (type == SPD_PUNCT_ALL)
        sprintf(command, "SET %s PUNCTUATION ALL", who);
    if (type == SPD_PUNCT_NONE)
        sprintf(command, "SET %s PUNCTUATION NONE", who);
    if (type == SPD_PUNCT_SOME)
        sprintf(command, "SET %s PUNCTUATION SOME", who);

    ret = spd_execute_command(connection, command);    
    
    return ret;
}

int
spd_w_set_spelling(int connection, SPDSpelling type, const char* who)
{
    static char command[32];
    int ret;

    if (type == SPD_SPELL_ON)
        sprintf(command, "SET %s SPELLING ON", who);
    if (type == SPD_SPELL_OFF)
        sprintf(command, "SET %s SPELLING OFF", who);

    ret = spd_execute_command(connection, command);

    return ret;
}

int
spd_w_set_voice_type(int connection, SPDVoiceType type, const char *who)
{
    static char command[64];

    switch(type){
    case SPD_MALE1: sprintf(command, "SET %s VOICE MALE1", who); break;
    case SPD_MALE2: sprintf(command, "SET %s VOICE MALE2", who); break;
    case SPD_MALE3: sprintf(command, "SET %s VOICE MALE3", who); break;
    case SPD_FEMALE1: sprintf(command, "SET %s VOICE FEMALE1", who); break;
    case SPD_FEMALE2: sprintf(command, "SET %s VOICE FEMALE2", who); break;
    case SPD_FEMALE3: sprintf(command, "SET %s VOICE FEMALE3", who); break;
    case SPD_CHILD_MALE: sprintf(command, "SET %s VOICE CHILD_MALE", who); break;
    case SPD_CHILD_FEMALE: sprintf(command, "SET %s VOICE CHILD_FEMALE", who); break;
    default: return -1;
    }
    
    return spd_execute_command(connection, command);      
}

#undef SPD_SET_COMMAND_INT
#undef SPD_SET_COMMAND_STR
#undef SPD_SET_COMMAND_SPECIAL

/* Takes the buffer, position of cursor in the buffer
 * and key that have been hit and tries to handle all
 * the speech output, tabulator completion and other
 * things and returns the new string. The cursor should
 * be moved then by the client program to the end of
 * this returned string.
 * The client should call this function and display it's
 * output every time there is some input to this command line.
 */

int
spd_get_client_list(int fd, char **client_names, int *client_ids, int* active){
	char command[128];
	char *reply;
	int footer_ok;
	int count;
	char *record;
	int record_int;
	char *record_str;	

	sprintf(command, "HISTORY GET CLIENT_LIST\r\n");
	reply = (char*) spd_send_data(fd, command, 1);

	footer_ok = parse_response_footer(reply);
	if(footer_ok != 1){
		free(reply);
		return -1;
	}
	
	for(count=0;  ;count++){
		record = (char*) parse_response_data(reply, count+1);
		if (record == NULL) break;
		record_int = get_rec_int(record, 0);
		client_ids[count] = record_int;
		record_str = (char*) get_rec_str(record, 1);
		assert(record_str!=NULL);
		client_names[count] = record_str;
		record_int = get_rec_int(record, 2);
		active[count] = record_int;
	}	
	return count;
}

int
spd_get_message_list_fd(int fd, int target, int *msg_ids, char **client_names)
{
	char command[128];
	char *reply;
	int header_ok;
	int count;
	char *record;
	int record_int;
	char *record_str;	

	sprintf(command, "HISTORY GET MESSAGE_LIST %d 0 20\r\n", target);
	reply = spd_send_data(fd, command, 1);

/*	header_ok = parse_response_header(reply);
	if(header_ok != 1){
		free(reply);
		return -1;
	}*/

	for(count=0;  ;count++){
		record = (char*) parse_response_data(reply, count+1);
		if (record == NULL) break;
		record_int = get_rec_int(record, 0);
		msg_ids[count] = record_int;
		record_str = (char*) get_rec_str(record, 1);
		assert(record_str!=NULL);
		client_names[count] = record_str;
	}
	return count;
}

char*
spd_command_line(int fd, char *buffer, int pos, char* c){
	char* new_s;

	if (buffer == NULL){
		new_s = malloc(sizeof(char) * 32);		
		buffer = malloc(sizeof(char));
		buffer[0] = 0;
	}else{
	   	if ((pos > strlen(buffer)) || (pos < 0)) return NULL;
		new_s = malloc(sizeof(char) * strlen(buffer) * 2);
	}
	new_s[0] = 0;
		
	/* Speech output for the symbol. */
	spd_key(fd, SPD_NOTIFICATION, c);	
	
	/* TODO: */
	/* What kind of symbol do we have. */
		switch(c[0]){
			/* Completion. */
			case '\t':	
					break;
			/* Other tasks. */	
			/* Deleting symbol */
			case '-':	/* TODO: Should be backspace. */
				strcpy(new_s, buffer);
				new_s[pos-1]=0;
				pos--;
				break;
			/* Adding symbol */
			default:	
				sprintf(new_s, "%s%c", buffer, c);
				pos++;
		}
	/* Speech output */

	/* Return the new string. */
	return new_s;
}

/* --------------------- Internal functions ------------------------- */

int
spd_set_priority(int connection, SPDPriority priority)
{
    static char p_name[16];
    static char command[64];

    switch(priority){
    case SPD_IMPORTANT: strcpy(p_name, "IMPORTANT"); break;
    case SPD_MESSAGE: strcpy(p_name, "MESSAGE"); break;
    case SPD_TEXT: strcpy(p_name, "TEXT"); break;
    case SPD_NOTIFICATION: strcpy(p_name, "NOTIFICATION"); break;
    case SPD_PROGRESS: strcpy(p_name, "PROGRESS"); break;
    default: 
        SPD_DBG("Can't set priority");
        return -1;
    }
		 
    sprintf(command, "SET SELF PRIORITY %s", p_name);
    return spd_execute_command(connection, command);
}

int
ret_ok(char *reply)
{
	int err;

	err = get_err_code(reply);
	xfree(reply);
		
	if ((err>=100) && (err<300)) return 1;
	if (err>=300) return 0;

	FATAL("Internal error during communication.");
}

char*
get_rec_part(char *record, int pos)
{
	int i, n;
	char *part;
	int p = 0;

	part = malloc(sizeof(char)*strlen(record));
	for(i=0;i<=strlen(record)-1;i++){
		if (record[i]==' '){
			   	p++;
				continue;
		}
		if(p == pos){
			n = 0;
			for(  ;i<=strlen(record)-1;i++){
				if(record[i] != ' '){
						part[n] = record[i];
						n++;
				}else{
						part[n] = 0;
						return part;
				}
			}
			part[n] = 0;
			return part;
		}
	}
	return NULL;
}

char*
get_rec_str(char *record, int pos)
{
	return(get_rec_part(record, pos));
}

int
get_rec_int(char *record, int pos)
{
	char *num_str;
	int num;

	assert(record!=0);
	num_str = get_rec_part(record, pos);
	if (!isanum(num_str)) return -9999;
	num = atoi(num_str);
	free(num_str);
	return num;
}

int parse_response_footer(char *resp)
{
	int ret;
	int i;
	int n = 0;
	char footer[256];
	for(i=0;i<=strlen(resp)-1;i++){
		if (resp[i]=='\r'){
			i+=2;
			if(resp[i+3] == ' '){
				for(; i<=strlen(resp)-1; i++){
					if (resp[i] != '\r'){
							footer[n] = resp[i];
							n++;
					}else{
						footer[n]=0;
						ret = ret_ok(footer);
						return ret;
					}
				}					
			}
		}
	}
	ret = ret_ok(footer);

	return ret;
}

char*
parse_response_data(char *resp, int pos)
{
	int p = 1;
	char *data;
	int i;
	int n = 0;
		
	data = malloc(sizeof(char) * strlen(resp));
	assert(data!=NULL);

	if (resp == NULL) return NULL;
	if (pos<1) return NULL;
	
	for(i=0;i<=strlen(resp)-1;i++){
		if (resp[i]=='\r'){
			   	p++;
				/* Skip the LFCR sequence */
				i++;
				if(i+3 < strlen(resp)-1){
					if(resp[i+4] == ' ') return NULL;
				}
				continue;
		}
		if (p==pos){
			/* Skip the ERR code */
			i+=4;	
			/* Read the data */
			for(; i<=strlen(resp)-1; i++){
				if (resp[i] != '\r'){
						data[n] = resp[i];
						n++;
				}else{
					data[n]=0;
					return data;	
				}
			}	
			data[n]=0;
			break;
		}
	}
	free(data);
	return NULL;
}

int
get_err_code(char *reply)
{
	char err_code[4];
	int err;
	
	if (reply == NULL) return -1;
	SPD_DBG("spd_send_data:	reply: %s\n", reply);

	err_code[0] = reply[0];	err_code[1] = reply[1];
	err_code[2] = reply[2];	err_code[3] = '\0';

	SPD_DBG("ret_ok: err_code:	|%s|\n", err_code);
   
	if(isanum(err_code)){
		err = atoi(err_code);
	}else{
		SPD_DBG("ret_ok: not a number\n");
		return -1;	
	}

	return err;
}

char*
spd_send_data(int fd, char *message, int wfr)
{
	char *reply;
	int bytes;

	/* TODO: 1000?! */
	reply = malloc(sizeof(char) * 1000);
   
	/* write message to the socket */
	write(fd, message, strlen(message));

	message[strlen(message)] = 0;
	SPD_DBG("command sent:	|%s|", message);

	/* read reply to the buffer */
	if (wfr == 1){
		bytes = read(fd, reply, 1000);
		/* print server reply to as a string */
		reply[bytes] = 0; 
		SPD_DBG("reply from server:	%s\n", reply);
	}else{
		SPD_DBG("reply from server: no reply\n\n");
		return "NO REPLY";
	} 

	return reply;
}

/* isanum() tests if the given string is a number,
 *  returns 1 if yes, 0 otherwise. */
int
isanum(char *str){
    int i;
	if (str == NULL) return 0;
    for(i=0;i<=strlen(str)-1;i++){
        if (!isdigit(str[i]))   return 0;
    }
    return 1;
}

int
spd_execute_command(int connection, char* command)
{
    char *buf;
    char *ret;
    
    buf = g_strdup_printf("%s\r\n", command);
    ret = spd_send_data(connection, buf, SPD_WAIT_REPLY);
    
    if(!ret_ok(ret))  return -1;
    else              return 0;
}

void*
xmalloc(unsigned int bytes)
{
    void *mem;

    mem = malloc(bytes);
    if (mem == NULL){
        FATAL("Not enough memmory!");
        exit(1);
    }
    
    return mem;
}

void
xfree(void *ptr)
{
    if (ptr != NULL){
        free(ptr);
        ptr = NULL;
    }
}

#ifdef LIBSPEECHD_DEBUG
void
SPD_DBG(char *format, ...)
{
        va_list args;

        va_start(args, format);
        vfprintf(spd_debug, format, args);
        va_end(args);
}
#else  /* LIBSPEECHD_DEBUG */
void
SPD_DBG(char *format, ...)
{
}
#endif /* LIBSPEECHD_DEBUG */
