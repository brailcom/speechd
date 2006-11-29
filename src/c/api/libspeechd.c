
/*
 * libspeechd.c - Shared library for easy acces to Speech Dispatcher functions
 *
 * Copyright (C) 2001, 2002, 2003, 2006 Brailcom, o.p.s.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * $Id: libspeechd.c,v 1.29 2006-11-29 16:56:11 hanke Exp $
 */


#define _GNU_SOURCE

#include <sys/types.h>
#include <wchar.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>
#include <errno.h>
#include <assert.h>

#include "def.h"
#include "libspeechd.h"

/* Comment/uncomment to switch debugging on/off */
// #define LIBSPEECHD_DEBUG 1

/* --------------  Private functions headers ------------------------*/

static int spd_set_priority(SPDConnection* connection, SPDPriority priority);
static char* escape_dot(const char *otext);
static int isanum(char* str);		
static char* get_reply(SPDConnection *connection);
static int get_err_code(char *reply);
static char* get_param_str(char* reply, int num, int *err);
static int get_param_int(char* reply, int num, int *err);
static void *xmalloc(size_t bytes);
static void xfree(void *ptr);   
static int ret_ok(char *reply);
static void SPD_DBG(char *format, ...);
static void* spd_events_handler(void*);

/* --------------------- Public functions ------------------------- */

#define SPD_REPLY_BUF_SIZE 65536

/* Opens a new Speech Dispatcher connection.
 * Returns socket file descriptor of the created connection
 * or -1 if no connection was opened. */

SPDConnection*
spd_open(const char* client_name, const char* connection_name, const char* user_name, SPDConnectionMode mode)
{
    struct sockaddr_in address;
    SPDConnection *connection;
    char *set_client_name;
    char* conn_name;
    char* usr_name;
    char *env_port;
    int port;
    int ret;
    char tcp_no_delay = 1;
    
    if (client_name == NULL) return NULL;
    
    if (user_name == NULL)
	usr_name = (char*) g_get_user_name();
    else
	usr_name = strdup(user_name);
    
    if(connection_name == NULL)
	conn_name = strdup("main");
    else
	conn_name = strdup(connection_name);
    
    env_port = getenv("SPEECHD_PORT");
    if (env_port != NULL)
	port = strtol(env_port, NULL, 10);
    else
	port = SPEECHD_DEFAULT_PORT;

    connection = xmalloc(sizeof(SPDConnection));
    
    /* Prepare a new socket */
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(port);
    address.sin_family = AF_INET;
    connection->socket = socket(AF_INET, SOCK_STREAM, 0);
    
#ifdef LIBSPEECHD_DEBUG
    spd_debug = fopen("/tmp/libspeechd.log", "w");
    if (spd_debug == NULL) SPD_FATAL("COULDN'T ACCES FILE INTENDED FOR DEBUG");
    SPD_DBG("Debugging started");
#endif /* LIBSPEECHD_DEBUG */
    
  /* Connect to server */
    ret = connect(connection->socket, (struct sockaddr *)&address, sizeof(address));
    if (ret == -1){
	SPD_DBG("Error: Can't connect to server: %s", strerror(errno));
	return NULL;
    }

    connection->callback_begin = NULL;
    connection->callback_end = NULL;
    connection->callback_im = NULL;
    connection->callback_pause = NULL;
    connection->callback_resume = NULL;
    connection->callback_cancel = NULL;

    connection->mode = mode;

    /* Create a stream from the socket */
    connection->stream = fdopen(connection->socket, "r");
    if (!connection->stream) SPD_FATAL("Can't create a stream for socket, fdopen() failed.");
    /* Switch to line buffering mode */
    ret = setvbuf(connection->stream, NULL, _IONBF, SPD_REPLY_BUF_SIZE);
    if (ret) SPD_FATAL("Can't set buffering, setvbuf failed.");

    connection->ssip_mutex = xmalloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(connection->ssip_mutex, NULL);

    if (mode == SPD_MODE_THREADED){
	SPD_DBG("Initializing threads, condition variables and mutexes...");
	connection->events_thread = xmalloc(sizeof(pthread_t));
	connection->cond_reply_ready = xmalloc(sizeof(pthread_cond_t));
	connection->mutex_reply_ready = xmalloc(sizeof(pthread_mutex_t));
	connection->cond_reply_ack = xmalloc(sizeof(pthread_cond_t));
	connection->mutex_reply_ack = xmalloc(sizeof(pthread_mutex_t));
	pthread_cond_init(connection->cond_reply_ready, NULL);
	pthread_mutex_init(connection->mutex_reply_ready, NULL);
	pthread_cond_init(connection->cond_reply_ack, NULL);
	pthread_mutex_init(connection->mutex_reply_ack, NULL);
	ret = pthread_create(connection->events_thread, NULL, spd_events_handler, connection);
	if(ret != 0){
	    SPD_DBG("Thread initialization failed");
	    return NULL;
	}
    }

    setsockopt(connection->socket, IPPROTO_TCP, TCP_NODELAY, &tcp_no_delay, sizeof(int));

    /* By now, the connection is created and operational */

    set_client_name = g_strdup_printf("SET SELF CLIENT_NAME \"%s:%s:%s\"", usr_name,
				      client_name, conn_name);

    ret = spd_execute_command_wo_mutex(connection, set_client_name);   

    xfree(usr_name);  xfree(conn_name);  xfree(set_client_name);

    return connection;
}


#define RET(r) \
    { \
    pthread_mutex_unlock(connection->ssip_mutex); \
    return r; \
    }


/* Close a Speech Dispatcher connection */
void
spd_close(SPDConnection* connection)
{

    pthread_mutex_lock(connection->ssip_mutex);

    if (connection->mode == SPD_MODE_THREADED){
	pthread_cancel(*connection->events_thread);
	pthread_mutex_destroy(connection->mutex_reply_ready);
	pthread_mutex_destroy(connection->mutex_reply_ack);
	pthread_cond_destroy(connection->cond_reply_ready);
	pthread_cond_destroy(connection->cond_reply_ack);
	connection->mode = SPD_MODE_SINGLE;
    }

    /* close the socket */
    close(connection->socket);

    pthread_mutex_unlock(connection->ssip_mutex);

    pthread_mutex_destroy(connection->ssip_mutex);
    xfree(connection);
}

/* Say TEXT with priority PRIORITY.
 * Returns msg_uid on success, -1 otherwise. */                            
int
spd_say(SPDConnection *connection, SPDPriority priority, const char* text)
{
    static char command[16];
    char *etext;
    int ret;
    char *reply;
    int err;
    int msg_id;

    if (text == NULL) return -1;

    pthread_mutex_lock(connection->ssip_mutex);

    SPD_DBG("Text to say is: %s", text);

    /* Set priority */
    ret = spd_set_priority(connection, priority);
    if(ret) RET(-1); 
    
    /* Check if there is no escape sequence in the text */
    etext = escape_dot(text);
    if (etext == NULL) etext = (char*) text;
  
    /* Start the data flow */
    sprintf(command, "SPEAK");
    ret = spd_execute_command_wo_mutex(connection, command);
    if(ret){     
        SPD_DBG("Error: Can't start data flow!");
        RET(-1);
    }
  
    /* Send data */
    spd_send_data_wo_mutex(connection, etext, SPD_NO_REPLY);

    /* Terminate data flow */
    ret = spd_execute_command_with_reply(connection, "\r\n.", &reply);
    if(ret){
        SPD_DBG("Can't terminate data flow");
        RET(-1); 
    }

    msg_id = get_param_int(reply, 1, &err);
    if (err < 0){
	SPD_DBG("Can't determine SSIP message unique ID parameter.");
    }
    xfree(reply);

    pthread_mutex_unlock(connection->ssip_mutex);

    return msg_id;
}

/* The same as spd_say, accepts also formated strings */
int
spd_sayf(SPDConnection *connection, SPDPriority priority, const char *format, ...)
{
    static int ret;    
    va_list args;
    char *buf;

    if (format == NULL) return -1;
    
    /* Print the text to buffer */
    va_start(args, format);
    buf = g_strdup_vprintf(format, args);
    va_end(args);
    
    /* Send the buffer to Speech Dispatcher */
    ret = spd_say(connection, priority, buf);	
    xfree(buf);

    return ret;
}

int
spd_stop(SPDConnection *connection)
{
  return spd_execute_command(connection, "STOP SELF");
}

int
spd_stop_all(SPDConnection *connection)
{
  return spd_execute_command(connection, "STOP ALL");
}

int
spd_stop_uid(SPDConnection *connection, int target_uid)
{
  static char command[16];

  sprintf(command, "STOP %d", target_uid);
  return spd_execute_command(connection, command);
}

int
spd_cancel(SPDConnection *connection)
{
  return spd_execute_command(connection, "CANCEL SELF");
}

int
spd_cancel_all(SPDConnection *connection)
{
  return spd_execute_command(connection, "CANCEL ALL");
}

int
spd_cancel_uid(SPDConnection *connection, int target_uid)
{
  static char command[16];

  sprintf(command, "CANCEL %d", target_uid);
  return spd_execute_command(connection, command);
}

int
spd_pause(SPDConnection *connection)
{
  return spd_execute_command(connection, "PAUSE SELF");
}

int
spd_pause_all(SPDConnection *connection)
{
  return spd_execute_command(connection, "PAUSE ALL");
}

int
spd_pause_uid(SPDConnection *connection, int target_uid)
{
  char command[16];

  sprintf(command, "PAUSE %d", target_uid);
  return spd_execute_command(connection, command);
}

int
spd_resume(SPDConnection *connection)
{
  return spd_execute_command(connection, "RESUME SELF");
}

int
spd_resume_all(SPDConnection *connection)
{
  return spd_execute_command(connection, "RESUME ALL");
}

int
spd_resume_uid(SPDConnection *connection, int target_uid)
{
  static char command[16];

  sprintf(command, "RESUME %d", target_uid);
  return spd_execute_command(connection, command);
}

int
spd_key(SPDConnection *connection, SPDPriority priority, const char *key_name)
{
    char *command_key;
    int ret;

    if (key_name == NULL) return -1;

    pthread_mutex_lock(connection->ssip_mutex);

    ret = spd_set_priority(connection, priority);
    if (ret) RET(-1);

    command_key = g_strdup_printf("KEY %s", key_name);
    ret = spd_execute_command_wo_mutex(connection, command_key);
    xfree(command_key);
    if (ret) RET(-1);

    pthread_mutex_unlock(connection->ssip_mutex);

    return 0;
}

int
spd_char(SPDConnection *connection, SPDPriority priority, const char *character)
{
    static char command[16];
    int ret;

    if (character == NULL) return -1;
    if (strlen(character)>6) return -1;

    pthread_mutex_lock(connection->ssip_mutex);

    ret = spd_set_priority(connection, priority);
    if (ret) RET(-1);

    sprintf(command, "CHAR %s", character);
    ret = spd_execute_command_wo_mutex(connection, command);
    if (ret) RET(-1);

    pthread_mutex_unlock(connection->ssip_mutex);

    return 0;
}

int
spd_wchar(SPDConnection *connection, SPDPriority priority, wchar_t wcharacter)
{
    static char command[16];
    char character[8];
    int ret;

    pthread_mutex_lock(connection->ssip_mutex);

    ret = wcrtomb(character, wcharacter, NULL);
    if (ret <= 0) RET(-1);

    ret = spd_set_priority(connection, priority);
    if (ret) RET(-1);

    assert(character != NULL);
    sprintf(command, "CHAR %s", character);
    ret = spd_execute_command_wo_mutex(connection, command);
    if (ret) RET(-1);

    pthread_mutex_unlock(connection->ssip_mutex);
 
    return 0;
}

int
spd_sound_icon(SPDConnection *connection, SPDPriority priority, const char *icon_name)
{
    char *command;
    int ret;

    if (icon_name == NULL) return -1;

    pthread_mutex_lock(connection->ssip_mutex);

    ret = spd_set_priority(connection, priority);
    if (ret) RET(-1);

    command = g_strdup_printf("SOUND_ICON %s", icon_name);
    ret = spd_execute_command_wo_mutex(connection, command);
    xfree (command);
    if (ret) RET(-1);

    pthread_mutex_unlock(connection->ssip_mutex);
    
    return 0;
}

int
spd_w_set_punctuation(SPDConnection *connection, SPDPunctuation type, const char* who)
{
    char command[32];
    int ret;

    if (type == SPD_PUNCT_ALL)
        sprintf(command, "SET %s PUNCTUATION all", who);
    if (type == SPD_PUNCT_NONE)
        sprintf(command, "SET %s PUNCTUATION none", who);
    if (type == SPD_PUNCT_SOME)
        sprintf(command, "SET %s PUNCTUATION some", who);

    ret = spd_execute_command(connection, command);    
    
    return ret;
}

int
spd_w_set_capital_letters(SPDConnection *connection, SPDCapitalLetters type, const char* who)
{
    char command[64];
    int ret;

    if (type == SPD_CAP_NONE)
        sprintf(command, "SET %s CAP_LET_RECOGN none", who);
    if (type == SPD_CAP_SPELL)
        sprintf(command, "SET %s CAP_LET_RECOGN spell", who);
    if (type == SPD_CAP_ICON)
        sprintf(command, "SET %s CAP_LET_RECOGN icon", who);

    ret = spd_execute_command(connection, command);    
    
    return ret;
}

int
spd_w_set_spelling(SPDConnection *connection, SPDSpelling type, const char* who)
{
    char command[32];
    int ret;

    if (type == SPD_SPELL_ON)
        sprintf(command, "SET %s SPELLING on", who);
    if (type == SPD_SPELL_OFF)
        sprintf(command, "SET %s SPELLING off", who);

    ret = spd_execute_command(connection, command);

    return ret;
}

int
spd_set_data_mode(SPDConnection *connection, SPDDataMode mode)
{
    char command[32];
    int ret;

    if (mode == SPD_DATA_TEXT)
        sprintf(command, "SET SELF SSML_MODE off");
    if (mode == SPD_DATA_SSML)
        sprintf(command, "SET SELF SSML_MODE on");

    ret = spd_execute_command(connection, command);

    return ret;
}

int
spd_w_set_voice_type(SPDConnection *connection, SPDVoiceType type, const char *who)
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

#define SPD_SET_COMMAND_INT(param, ssip_name, condition) \
    int \
    spd_w_set_ ## param (SPDConnection *connection, signed int val, const char* who) \
    { \
        static char command[64]; \
        if ((!condition)) return -1; \
        sprintf(command, "SET %s " #ssip_name " %d", who, val); \
        return spd_execute_command(connection, command); \
    } \
    int \
    spd_set_ ## param (SPDConnection *connection, signed int val) \
    { \
        return spd_w_set_ ## param (connection, val, "SELF"); \
    } \
    int \
    spd_set_ ## param ## _all(SPDConnection *connection, signed int val) \
    { \
        return spd_w_set_ ## param (connection, val, "ALL"); \
    } \
    int \
    spd_set_ ## param ## _uid(SPDConnection *connection, signed int val, unsigned int uid) \
    { \
        char who[8]; \
        sprintf(who, "%d", uid); \
        return spd_w_set_ ## param (connection, val, who); \
    }

#define SPD_SET_COMMAND_STR(param, ssip_name) \
    int \
    spd_w_set_ ## param (SPDConnection *connection, const char *str, const char* who) \
    { \
        char *command; \
        int ret; \
        if (str == NULL) return -1; \
        command = g_strdup_printf("SET %s " #param " %s", \
                              who, str); \
        ret = spd_execute_command(connection, command); \
        xfree(command); \
        return ret; \
    } \
    int \
    spd_set_ ## param (SPDConnection *connection, const char *str) \
    { \
        return spd_w_set_ ## param (connection, str, "SELF"); \
    } \
    int \
    spd_set_ ## param ## _all(SPDConnection *connection, const char *str) \
    { \
        return spd_w_set_ ## param (connection, str, "ALL"); \
    } \
    int \
    spd_set_ ## param ## _uid(SPDConnection *connection, const char *str, unsigned int uid) \
    { \
        char who[8]; \
        sprintf(who, "%d", uid); \
        return spd_w_set_ ## param (connection, str, who); \
    }

#define SPD_SET_COMMAND_SPECIAL(param, type) \
    int \
    spd_set_ ## param (SPDConnection *connection, type val) \
    { \
        return spd_w_set_ ## param (connection, val, "SELF"); \
    } \
    int \
    spd_set_ ## param ## _all(SPDConnection *connection, type val) \
    { \
        return spd_w_set_ ## param (connection, val, "ALL"); \
    } \
    int \
    spd_set_ ## param ## _uid(SPDConnection *connection, type val, unsigned int uid) \
    { \
        char who[8]; \
        sprintf(who, "%d", uid); \
        return spd_w_set_ ## param (connection, val, who); \
    }

SPD_SET_COMMAND_INT(voice_rate, RATE, ((val >= -100) && (val <= +100)) );
SPD_SET_COMMAND_INT(voice_pitch, PITCH, ((val >= -100) && (val <= +100)) );
SPD_SET_COMMAND_INT(volume, VOLUME, ((val >= -100) && (val <= +100)) );

SPD_SET_COMMAND_STR(language, LANGUAGE);
SPD_SET_COMMAND_STR(output_module, OUTPUT_MODULE);

SPD_SET_COMMAND_SPECIAL(punctuation, SPDPunctuation);
SPD_SET_COMMAND_SPECIAL(capital_letters, SPDCapitalLetters);
SPD_SET_COMMAND_SPECIAL(spelling, SPDSpelling);
SPD_SET_COMMAND_SPECIAL(voice_type, SPDVoiceType);

#undef SPD_SET_COMMAND_INT
#undef SPD_SET_COMMAND_STR
#undef SPD_SET_COMMAND_SPECIAL

int
spd_set_notification_on(SPDConnection *connection, SPDNotification notification)
{
    if (connection->mode == SPD_MODE_THREADED)
	return spd_set_notification(connection, notification, "on");
    else
	return -1;
}

int
spd_set_notification_off(SPDConnection *connection, SPDNotification notification)
{
    if (connection->mode == SPD_MODE_THREADED)
	return spd_set_notification(connection, notification, "off");
    else
	return -1;
}


#define NOTIFICATION_SET(val, ssip_val) \
    if (notification & val){ \
	sprintf(command, "SET SELF NOTIFICATION "ssip_val" %s", state);\
	ret = spd_execute_command_wo_mutex(connection, command);\
	if (ret < 0) RET(-1);\
    }

int
spd_set_notification(SPDConnection *connection, SPDNotification notification, const char* state)
{
    static char command[64];
    int ret;

    if (connection->mode != SPD_MODE_THREADED) return -1;

    if (state == NULL){
	SPD_DBG("Requested state is NULL");
	return -1;
    }
    if (strcmp(state, "on") && strcmp(state, "off")){
	SPD_DBG("Invalid argument for spd_set_notification: %s", state);
	return -1;
    }

    pthread_mutex_lock(connection->ssip_mutex);

    NOTIFICATION_SET(SPD_INDEX_MARKS, "index_marks");
    NOTIFICATION_SET(SPD_BEGIN, "begin");
    NOTIFICATION_SET(SPD_END, "end");
    NOTIFICATION_SET(SPD_CANCEL, "cancel");
    NOTIFICATION_SET(SPD_PAUSE, "pause");
    NOTIFICATION_SET(SPD_RESUME, "resume");
    NOTIFICATION_SET(SPD_RESUME, "pause");

    pthread_mutex_unlock(connection->ssip_mutex);

    return 0;
}
#undef NOTIFICATION_SET

//int
//spd_get_client_list(SPDConnection *connection, char **client_names, int *client_ids, int* active){
//        SPD_DBG("spd_get_client_list: History is not yet implemented.");
//        return -1;
//
//}

int
spd_get_message_list_fd(SPDConnection *connection, int target, int *msg_ids, char **client_names)
{
        SPD_DBG("spd_get_client_list: History is not yet implemented.");
        return -1;
#if 0
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
#endif
}

int
spd_execute_command(SPDConnection *connection, char* command)
{
    char *reply;
    int ret;

    pthread_mutex_lock(connection->ssip_mutex);

    ret = spd_execute_command_with_reply(connection, command, &reply);
    xfree(reply);

    pthread_mutex_unlock(connection->ssip_mutex);

    return ret;
}

int
spd_execute_command_wo_mutex(SPDConnection *connection, char* command)
{
    char *reply;
    int ret;

    ret = spd_execute_command_with_reply(connection, command, &reply);
    xfree(reply);

    return ret;
}

int
spd_execute_command_with_reply(SPDConnection *connection, char* command, char **reply)
{
    char *buf;    
    int r;
    
    buf = g_strdup_printf("%s\r\n", command);
    *reply = spd_send_data_wo_mutex(connection, buf, SPD_WAIT_REPLY);
    xfree(buf);
    
    r = ret_ok(*reply);

    if (!r) return -1;
    else return 0;
}


char*
spd_send_data(SPDConnection *connection, const char *message, int wfr)
{
    char *reply;
    pthread_mutex_lock(connection->ssip_mutex);
    reply = spd_send_data_wo_mutex(connection, message, wfr);
    pthread_mutex_unlock(connection->ssip_mutex);
    return reply;
}

char*
spd_send_data_wo_mutex(SPDConnection *connection, const char *message, int wfr)
{

    char *reply;
    int bytes;

    if (connection->mode == SPD_MODE_THREADED){
	/* Make sure we don't get the cond_reply_ready signal before we are in
	   cond_wait() */
	pthread_mutex_lock(connection->mutex_reply_ready);
    }
    /* write message to the socket */
    write(connection->socket, message, strlen(message));
    SPD_DBG(">> : |%s|", message);

    /* read reply to the buffer */
    if (wfr){
	if (connection->mode == SPD_MODE_THREADED){
	    /* Wait until the reply is ready */
	    pthread_cond_wait(connection->cond_reply_ready, connection->mutex_reply_ready);
	    pthread_mutex_unlock(connection->mutex_reply_ready);
	    /* Read the reply */
	    reply = connection->reply;
	    connection->reply = NULL;
	    bytes = strlen(reply);
	    if (bytes == 0){
		SPD_DBG("Error: Can't read reply, broken socket.");
		return NULL;
	    }
	    /* Signal the reply has been read */
	    pthread_mutex_lock(connection->mutex_reply_ack);
	    pthread_cond_signal(connection->cond_reply_ack);
	    pthread_mutex_unlock(connection->mutex_reply_ack);
	}else{
	    reply = get_reply(connection);
	}
	SPD_DBG("<< : |%s|\n", reply);	
    }else{
	if (connection->mode == SPD_MODE_THREADED)
	    pthread_mutex_unlock(connection->mutex_reply_ready);
        SPD_DBG("<< : no reply expected");
        return "NO REPLY";
    } 

    return reply;
}


/* --------------------- Internal functions ------------------------- */

static int
spd_set_priority(SPDConnection *connection, SPDPriority priority)
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
        SPD_DBG("Error: Can't set priority! Incorrect value.");
        return -1;
    }
		 
    sprintf(command, "SET SELF PRIORITY %s", p_name);
    return spd_execute_command_wo_mutex(connection, command);
}


static char*
get_reply(SPDConnection *connection)
{
    GString *str;
    char *line = NULL;
    size_t N = 0;
    int bytes;
    char *reply;

    str = g_string_new("");

    /* Wait for activity on the socket, when there is some,
       read all the message line by line */
    do{
	bytes = getline(&line, &N, connection->stream);	
	if (bytes == -1){
	    SPD_FATAL("Error: Can't read reply, broken socket!");		
	}
	g_string_append(str, line);
	/* terminate if we reached the last line (without '-' after numcode) */
    }while( !((strlen(line) < 4) || (line[3] == ' ')));
    
    /* The resulting message received from the socket is stored in reply */
    reply = str->str;
    g_string_free(str, FALSE);

    return reply;
}

static void*
spd_events_handler(void* conn)
{
    char *reply;
    int reply_code;
    SPDConnection *connection = conn;

    while(1){

	/* Read the reply/event (block if none is available) */
	reply = get_reply(connection);
        SPD_DBG("<< : |%s|\n", reply);

	reply_code = get_err_code(reply);

	if ((reply_code >= 700) && (reply_code < 800)){
	    int msg_id;
	    int client_id;
	    int err;

	    SPD_DBG("Callback detected: %s", reply);

	    /* This is an index mark */
	    /* Extract message id */
	    msg_id = get_param_int(reply, 1, &err);
	    if (err < 0){
		SPD_DBG("Bad reply from Speech Dispatcher: %s (code %d)", reply, err);
		break;
	    }
	    client_id = get_param_int(reply, 2, &err);
	    if (err < 0){
		SPD_DBG("Bad reply from Speech Dispatcher: %s (code %d)", reply, err);
		break;
	    }
	    /*  Decide if we want to call a callback */
	    if ((reply_code == 701) && (connection->callback_begin))
		connection->callback_begin(msg_id, client_id, SPD_EVENT_BEGIN);
	    if ((reply_code == 702) && (connection->callback_end))
		connection->callback_end(msg_id, client_id, SPD_EVENT_END);
	    if ((reply_code == 703) && (connection->callback_cancel))
		connection->callback_cancel(msg_id, client_id, SPD_EVENT_CANCEL);
	    if ((reply_code == 704) && (connection->callback_pause))
		connection->callback_pause(msg_id, client_id, SPD_EVENT_PAUSE);
	    if ((reply_code == 705) && (connection->callback_resume))
		connection->callback_resume(msg_id, client_id, SPD_EVENT_RESUME);
	    if ((reply_code == 700) && (connection->callback_im)){
		char* im;
		int err;
		im = get_param_str(reply, 3, &err);
		if ((err < 0) || (im == NULL)){
		    SPD_DBG("Broken reply from Speech Dispatcher: %s", reply);
		    break;
		}
		/* Call the callback */
		connection->callback_im(msg_id, client_id, SPD_EVENT_INDEX_MARK, im);
		xfree(im);
	    }

	}else{
	    /* This is a protocol reply */
	    pthread_mutex_lock(connection->mutex_reply_ready);
	    /* Prepare the reply to the reply buffer in connection */
	    connection->reply = reply;
	    /* Signal the reply is available on the condition variable */
	    /* this order is correct and necessary */
	    pthread_cond_signal(connection->cond_reply_ready);
	    pthread_mutex_lock(connection->mutex_reply_ack); 
	    pthread_mutex_unlock(connection->mutex_reply_ready);
	    /* Wait until it has bean read */
	    pthread_cond_wait(connection->cond_reply_ack, connection->mutex_reply_ack);
	    pthread_mutex_unlock(connection->mutex_reply_ack);
	    /* Continue */	
	}
    }
    return 0; 			/* to please gcc */
}

static int
ret_ok(char *reply)
{
	int err;

	err = get_err_code(reply);
		
	if ((err>=100) && (err<300)) return 1;
	if (err>=300) return 0;

	SPD_FATAL("Internal error during communication.");
}

static char*
get_param_str(char* reply, int num, int *err)
{
    int i;
    char *tptr;
    char *pos;
    char *pos_begin;
    char *pos_end;
    char *rep;

    assert(err != NULL);

    if (num < 1){
	*err = -1;
	return NULL;
    }

    pos = reply;
    for (i=0; i<=num-2; i++){
	pos = strstr(pos, "\r\n");	
	if (pos == NULL){
	    *err = -2;
	    return NULL;
	}
	pos += 2;
    }

    if (strlen(pos) < 4) return NULL;
    
    *err = strtol(pos, &tptr, 10);
    if (*err >= 300 && *err <= 399)
	return NULL;

    if ((*tptr != '-') || (tptr != pos+3)){
	*err = -2;
	return NULL;
    }

    pos_begin = pos + 4;
    pos_end = strstr(pos_begin, "\r\n");
    if (pos_end == NULL){
	*err = -2;
	return NULL;
    }

    rep = (char*) strndup(pos_begin, pos_end - pos_begin);
    *err = 0;
    
    return rep;
}

static int
get_param_int(char* reply, int num, int *err)
{
    char *rep_str;
    char *tptr;
    int ret;

    rep_str = get_param_str(reply, num, err);
    if (rep_str == NULL){
	/* err is already set to the error return code, just return */
	return 0;
    }
    
    ret = strtol(rep_str, &tptr, 10);
    if (*tptr != '\0'){
	/* this is not a number */
	*err = -3;
	return 0;
    }    
    xfree(rep_str);

    return ret;
}

static int
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

/* isanum() tests if the given string is a number,
 *  returns 1 if yes, 0 otherwise. */
static int
isanum(char *str)
{
    int i;
    if (str == NULL) return 0;
    for(i=0;i<=strlen(str)-1;i++){
        if (!isdigit(str[i]))   return 0;
    }
    return 1;
}

static void*
xmalloc(size_t bytes)
{
    void *mem;

    mem = malloc(bytes);
    if (mem == NULL){
        SPD_FATAL("Not enough memmory!");
        exit(1);
    }
    
    return mem;
}

static void
xfree(void *ptr)
{
    if (ptr != NULL)
        free(ptr);
}

static char*
escape_dot(const char *text)
{
    char *seq;
    GString *ntext;
    char *p;
    char *otext;
    char *ret = NULL;
    int len;

    if (otext == NULL) return NULL;

    SPD_DBG("Incomming text to escaping: |%s|", otext);

    p = (char*) text;
    otext = p;

    ntext = g_string_new("");

    len = strlen(text);
    if (len == 1){
        if (!strcmp(text, ".")){
            g_string_append(ntext, "..");
            p += 1;
        }
    }
    else if (len >= 3){
        if ((p[0] == '.') && (p[1] == '\r') && (p[2] == '\n')){
            g_string_append(ntext, "..\r\n");
            p += 3;
        }
    }

    //    SPD_DBG("Altering text (I): |%s|", ntext->str);

    while (seq = strstr(p, "\r\n.\r\n")){
        assert(seq>p);
        g_string_append_len(ntext, p, seq-p);
        g_string_append(ntext, "\r\n..\r\n");
        p = seq+5;
    }

    //   SPD_DBG("Altering text (II): |%s|", ntext->str);    

    len = strlen(p);
    if (len >= 3){
        if ((p[len-3] == '\r') && (p[len-2] == '\n')
            && (p[len-1] == '.')){
            g_string_append(ntext, p);
            g_string_append(ntext, ".");
            p += len;
            //  SPD_DBG("Altering text (II-b): |%s|", ntext->str);    
        }
    }

    if (p == otext){
        SPD_DBG("No escaping needed.");
        g_string_free(ntext, 1);
        return NULL;
    }else{
        g_string_append(ntext, p);
        ret = ntext->str;
        g_string_free(ntext, 0);
    }

    SPD_DBG("Altered text after escaping: |%s|", ret);

    return ret;
}

#ifdef LIBSPEECHD_DEBUG
static void
SPD_DBG(char *format, ...)
{
        va_list args;

        va_start(args, format);
        vfprintf(spd_debug, format, args);
        va_end(args);
	fprintf(spd_debug, "\n");
	fflush(spd_debug);
}
#else  /* LIBSPEECHD_DEBUG */
static void
SPD_DBG(char *format, ...)
{
}
#endif /* LIBSPEECHD_DEBUG */
