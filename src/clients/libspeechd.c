
/*
 * libspeechd.c - Shared library for easy acces to Speech Deamon functions
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
 * $Id: libspeechd.c,v 1.13 2003-04-14 02:12:20 hanke Exp $
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <glib.h>
#include <assert.h>

#include "def.h"
#include "libspeechd.h"

#define FATAL(msg) { perror("client: "msg); exit(1); }

static FILE* debugg;

/* --------------------- Public functions ------------------------- */

int 
spd_init(char* client_name, char* conn_name)
{
  int sockfd;
  struct sockaddr_in address;
  char helper[256];
  char *buf;

  address.sin_addr.s_addr = inet_addr("127.0.0.1");
  address.sin_port = htons(SPEECH_PORT);
  address.sin_family = AF_INET;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  debugg = fopen("/tmp/libspeechd_debug", "w");
  if (debugg == NULL) FATAL("COULDN'T ACCES FILE");
  /* connect to server */
  if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) == -1)
      return 0;

  buf = malloc((strlen(client_name) + strlen(conn_name) + 64) * sizeof(char));
  if (buf == NULL) FATAL ("Not enough memory.\n");

  sprintf(buf, "SET CLIENT_NAME %s:%s\r\n", client_name, conn_name);
  if(! ret_ok( send_data(sockfd, buf, 1) )) return 0; 

  free(buf);

  return sockfd;
}

void
spd_close(int fd)
{
   /* close the socket */
   close(fd);
}

int
spd_say(int fd, int priority, char* text)
{
  static char helper[256];
  char *buf;
  static int i;
  static char *pos;

  sprintf(helper, "SET PRIORITY %d\r\n", priority);
  if(!ret_ok(send_data(fd, helper, 1))){
		  if(LIBSPEECHD_DEBUG) printf("Can't set priority");
		  return -1;
	}
		  
  /* This needs to be fixed! */
//  while(pos = (char*) strstr(text,"\r\n.\r\n")){
//	text[pos-text+3] = ' ';
 // }
  
  sprintf(helper, "SPEAK\r\n");
  if(!ret_ok(send_data(fd, helper, 1))){
		  if(LIBSPEECHD_DEBUG) printf("Can't start data flow");
		  return -1;
	}
  
  buf = malloc((strlen(text) + 16)*sizeof(char));
  if (buf == NULL) FATAL ("Not enough memory.\n");
  sprintf(buf, "%s\r\n", text); 
  send_data(fd, buf, 0);

  sprintf(helper, "\r\n.\r\n");
  if(!ret_ok(send_data(fd, helper, 1))){
		  if(LIBSPEECHD_DEBUG) printf("Can't terminate data flow");
		  return -1; 
  }

  return 0;
}

int
spd_sayf(int fd, int priority, char *format, ...)
{
	va_list args;
	char *buf;
	int ret;
	
	buf = malloc(strlen(format)*3+4096);	// TODO: solve this problem!!!
	
	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);

	ret = spd_say(fd, priority, buf);	
	return ret;
}

int
spd_stop(int fd)
{
  char helper[32];

  sprintf(helper, "STOP\r\n");
  if(!ret_ok(send_data(fd, helper, 1))) return -1;

  return 0;
}

int
spd_stop_fd(int fd, int target)
{
  char helper[32];

  sprintf(helper, "STOP %d\r\n", target);
  if(!ret_ok(send_data(fd, helper, 1))) return -1;
  return 0;
}

int
spd_cancel(int fd)
{
  char helper[32];

  sprintf(helper, "CANCEL\r\n");
  if(!ret_ok(send_data(fd, helper, 1))) return -1;

  return 0;
}

int
spd_cancel_fd(int fd, int target)
{
  char helper[32];

  sprintf(helper, "CANCEL %d\r\n", target);
  if(!ret_ok(send_data(fd, helper, 1))) return -1;
  return 0;
}


int
spd_pause(int fd)
{
  char helper[16];

  sprintf(helper, "PAUSE\r\n");
  if(!ret_ok(send_data(fd, helper, 1))) return -1;

  return 0;
}

int
spd_pause_fd(int fd, int target)
{
  char helper[16];

  sprintf(helper, "PAUSE %d\r\n", target);
  if(!ret_ok(send_data(fd, helper, 1))) return -1;
  return 0;
}

int
spd_resume(int fd)
{
	char helper[16];

	sprintf(helper, "RESUME\r\n");

	if(!ret_ok(send_data(fd, helper, 1))) return -1;
	return 0;
}

int
spd_resume_fd(int fd, int target)
{
	char helper[16];

	sprintf(helper, "RESUME %d\r\n", target);

	if(!ret_ok(send_data(fd, helper, 1))) return -1;
	return 0;
}

int
spd_icon(int fd, char *name)
{
	static char helper[256];
	char *buf;

	sprintf(helper, "SOUND_ICON %s\r\n", name);
	if(!ret_ok(send_data(fd, helper, 1))) return -1;

	return 0;
}

int
spd_say_key(int fd, int priority, int c)
{
	static int r;

	return 0;	
}

int
spd_voice_rate(int fd, int rate)
{
	if(rate < -100) return -1;
	if(rate > +100) return -1;
	return 0;
}

int
spd_voice_pitch(int fd, int pitch)
{
	if(pitch < -100) return -1;
	if(pitch > +100) return -1;
	return 0;
}

int
spd_voice_punctuation(int fd, int flag)
{
	if((flag != 0)&&(flag != 1)&&(flag != 2)) return -1;
	return 0;
}

int
spd_voice_cap_let_recognition(int fd, int flag)
{
	if((flag != 0)&&(flag != 1)) return -1;
	return 0;
}

int
spd_voice_spelling(int fd, int flag)
{
	if((flag != 0)&&(flag != 1)) return -1;
	return 0;
}

int
spd_history_say_msg(int fd, int id)
{
	char helper[32];
	sprintf(helper,"HISTORY SAY ID %d\r\n",id);

	if(!ret_ok(send_data(fd, helper, 1))) return -1;
	return 0;
}

int
spd_history_select_client(int fd, int id)
{
	char helper[64];
	sprintf(helper,"HISTORY CURSOR SET %d FIRST\r\n",id);

	if(!ret_ok(send_data(fd, helper, 1))) return -1;
	return 0;
}

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
	reply = (char*) send_data(fd, command, 1);

	footer_ok = parse_response_footer(reply);
	if(footer_ok != 1){
		free(reply);
		return -1;
	}
	
	for(count=0;  ;count++){
		record = (char*) parse_response_data(reply, count+1);
		if (record == NULL) break;
//		MSG(3,"record:(%s)\n", record);
		record_int = get_rec_int(record, 0);
		client_ids[count] = record_int;
//		MSG(3,"record_int:(%d)\n", client_ids[count]);
		record_str = (char*) get_rec_str(record, 1);
		assert(record_str!=NULL);
		client_names[count] = record_str;
//		MSG(3,"record_str:(%s)\n", client_names[count]);		
		record_int = get_rec_int(record, 2);
		active[count] = record_int;
//		MSG(3,"record_int:(%d)\n", active[count]);		
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
	reply = send_data(fd, command, 1);

/*	header_ok = parse_response_header(reply);
	if(header_ok != 1){
		free(reply);
		return -1;
	}*/

	for(count=0;  ;count++){
		record = (char*) parse_response_data(reply, count+1);
		if (record == NULL) break;
//		MSG(3,"record:(%s)\n", record);
		record_int = get_rec_int(record, 0);
		msg_ids[count] = record_int;
//		MSG(3,"record_int:(%d)\n", msg_ids[count]);
		record_str = (char*) get_rec_str(record, 1);
		assert(record_str!=NULL);
		client_names[count] = record_str;
//		MSG(3,"record_str:(%s)\n", client_names[count]);		
	}
	return count;
}

char*
spd_command_line(int fd, char *buffer, int pos, int c){
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
	spd_say_key(fd, 3, c);	
	
	/* TODO: */
	/* What kind of symbol do we have. */
		switch(c){
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
ret_ok(char *reply)
{
	int err;

	err = get_err_code(reply);
		
	if ((err>=100) && (err<300)) return 1;
	if (err>=300) return 0;
	
	printf("reply:%d", err);
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
	if(LIBSPEECHD_DEBUG) fprintf(debugg, "pos:%d, rec:%s num_str:_%s_", pos, record, num_str);
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
	if(LIBSPEECHD_DEBUG) fprintf(debugg,"send_data:	reply: %s\n", reply);

	err_code[0] = reply[0];	err_code[1] = reply[1];
	err_code[2] = reply[2];	err_code[3] = '\0';

	if(LIBSPEECHD_DEBUG) fprintf(debugg,"ret_ok: err_code:	|%s|\n", err_code);
   
	if(isanum(err_code)){
		err = atoi(err_code);
	}else{
		fprintf(debugg,"ret_ok: not a number\n");
		return -1;	
	}

	return err;
}

char*
send_data(int fd, char *message, int wfr)
{
	char *reply;
	int bytes;

	/* TODO: 1000?! */
	reply = malloc(sizeof(char) * 1000);
   
	/* write message to the socket */
	write(fd, message, strlen(message));

	message[strlen(message)] = 0;
	if(LIBSPEECHD_DEBUG) fprintf(debugg,"command sent:	|%s|", message);

	/* read reply to the buffer */
	if (wfr == 1){
		bytes = read(fd, reply, 1000);
		/* print server reply to as a string */
		reply[bytes] = 0; 
		if(LIBSPEECHD_DEBUG) fprintf(debugg, "reply from server:	%s\n", reply);
	}else{
		if(LIBSPEECHD_DEBUG) fprintf(debugg, "reply from server: no reply\n\n");
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

