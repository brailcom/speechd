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

char* send_data(int fd, char *message, int wfr);

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

  debugg = fopen("/tmp/debugg", "w");
  if (debugg == NULL) FATAL("COULDN'T ACCES FILE");
  /* connect to server */
  if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) == -1)
      return 0;

  buf = malloc((strlen(client_name) + strlen(conn_name) + 64) * sizeof(char));
  if (buf == NULL) FATAL ("Not enough memory.\n");

  sprintf(buf, "@set client_name %s:%s\n\r", client_name, conn_name);
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
  char helper[256];
  char *buf;

  sprintf(helper, "@set priority %d\n\r", priority);
  if(!ret_ok(send_data(fd, helper, 1))) return 0;

  sprintf(helper, "@data on\n\r");
  if(!ret_ok(send_data(fd, helper, 1))) return 0;

  buf = malloc((strlen(text) + 4)*sizeof(char));
  if (buf == NULL) FATAL ("Not enough memory.\n");
  sprintf(buf, "%s\n\r", text); 
  send_data(fd, buf, 0);

  sprintf(helper, "@data off\n\r");
  if(!ret_ok(send_data(fd, helper, 1))) return 0; 
  
  return 1;
}

int
spd_sayf (int fd, int priority, char *format, ...)
{
	va_list args;
	char buf[512];
	int ret;
	
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

  sprintf(helper, "@stop\n\r");
  if(!ret_ok(send_data(fd, helper, 1))) return 0;

  return 1;
}

int
spd_stop_fd(int fd, int target)
{
  char helper[32];

  sprintf(helper, "@stop %d\n\r", target);
  if(!ret_ok(send_data(fd, helper, 1))) return 0;
}

int
spd_pause(int fd)
{
  char helper[16];

  sprintf(helper, "@pause\n\r");
  if(!ret_ok(send_data(fd, helper, 1))) return 0;

  return 1;
}

int
spd_pause_fd(int fd, int target)
{
  char helper[16];

  sprintf(helper, "@pause %d\n\r", target);
  if(!ret_ok(send_data(fd, helper, 1))) return 0;
}

int
spd_resume(int fd)
{
	char helper[16];

	sprintf(helper, "@resume\n\r");

	if(!ret_ok(send_data(fd, helper, 1))) return 0;
	return 1;
}

int
spd_resume_fd(int fd, int target)
{
	char helper[16];

	sprintf(helper, "@resume %d\n\r", target);

	if(!ret_ok(send_data(fd, helper, 1))) return 0;
	return 1;
}

int
spd_history_say_msg(int fd, int id)
{
	char helper[32];
	sprintf(helper,"@history say ID %d\n\r",id);

	if(!ret_ok(send_data(fd, helper, 1))) return 0;
	return 1;
}

int
spd_history_select_client(int fd, int id)
{
	char helper[64];
	sprintf(helper,"@history cursor set %d first\n\r",id);

	if(!ret_ok(send_data(fd, helper, 1))) return 0;
	return 1;
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
	int header_ok;
	int count;
	char *record;
	int record_int;
	char *record_str;	

	sprintf(command, "@history get client_list\n\r");
	reply = send_data(fd, command, 1);

	header_ok = parse_response_header(reply);
	if(header_ok != 1){
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

	sprintf(command, "@history get message_list %d 0 20\n\r", target);
	reply = send_data(fd, command, 1);

	header_ok = parse_response_header(reply);
	if(header_ok != 1){
		free(reply);
		return -1;
	}

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
	spd_sayf(fd, 3, "%c\n\0", c);
	
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

int ret_ok(char *ret){
//   char str[256];
   char quantum_trashbin[30000];
   /* TODO: It seems that this piece of code is subject to the
	* laws of quantum mechanics. If you cease to observe what's
	* going on (remove this sprintf), it works in a completely
	* different manner. It doesn't stop in either one of the strstr
	* functions and executes internall error. But if you uncomment
	* the sprintf and try to determine what's going on, everything
	* works correctly. Any help appreciated. */
   if (ret == NULL) FATAL("Couldn't determine the exact velocity and position at once.");
   fprintf(debugg,"ret_ok::	%s\n", ret);
   sprintf(quantum_trashbin, "returned: %s", ret);
   if ((char*)strstr(ret, "OK") != NULL) return 1;
   if ((char*)strstr(ret, "ERR") != NULL) return 0;
   printf("returned: |%s|\n", ret);
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
//	printf("pos:%d, rec:%s num_str:_%s_", pos, record, num_str);
	if (!isanum(num_str)) return -9999;
	num = atoi(num_str);
	free(num_str);
	return num;
}

int parse_response_header(char *resp)
{
	int ret;
	char *header;
	int i;
	
	header = malloc(sizeof(char) * strlen(resp));
	
   fprintf(debugg,"command sent:	%s\n", resp);
	for(i=0;i<=strlen(resp)-1;i++){
		if ((resp[i]!='\\') && (resp[i]!='\n'))	header[i] = resp[i];
		else break;
	}
	header[i] = 0;
	ret = ret_ok(header);

	free(header);
	return ret;
}

char*
parse_response_data(char *resp, int pos)
{
	int p = 0;
	char *data;
	int i;
	int n = 0;
		
	data = malloc(sizeof(char) * strlen(resp));
	assert(data!=NULL);
	
	for(i=0;i<=strlen(resp)-1;i++){
		if (resp[i]=='\\'){
			   	p++;
				i+=2;
				continue;
		}
		if (p==pos){
			for(;i<=strlen(resp)-1;i++){
				if ((resp[i]!='\\') && (resp[i]!='\n')){
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
   fprintf(debugg,"command sent:	%s", message);

   /* read reply to the buffer */
   if (wfr == 1){
      bytes = read(fd, reply, 1000);
      /* print server reply to as a string */
      reply[bytes] = 0; 
      fprintf(debugg, "reply from server:	%s\n", reply);
      return reply;
   }else{
      fprintf(debugg, "reply from server: no reply\n\n");
	  strcpy(reply, "NO REPLY\n\r");
      return reply;
   } 
}

/* isanum() tests if the given string is a number,
 *  *  * returns 1 if yes, 0 otherwise. */
int
isanum(char *str){
    int i;
    for(i=0;i<=strlen(str)-1;i++){
        if (!isdigit(str[i]))   return 0;
    }
    return 1;
}

