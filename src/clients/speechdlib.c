#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

#include "def.h"

#define FATAL(msg) { perror("client: "msg); exit(1); }

int 
speechd_init(char* client_name, char* conn_name)
{
  int sockfd;
  struct sockaddr_in address;
  char helper[256];
  char *buf;

  address.sin_addr.s_addr = inet_addr("127.0.0.1");
  address.sin_port = htons(SPEECH_PORT);
  address.sin_family = AF_INET;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  /* connect to server */
  if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) == -1)
      return 0;

  buf = malloc((strlen(client_name) + strlen(conn_name) + 64) * sizeof(char));
  if (buf == NULL) FATAL ("Not enough memory.\n");
  sprintf(buf, "@client_name %s:%s\n\r", client_name, conn_name);
  if(! ret_ok( send_data(sockfd, buf, 1) )) return 0; 
  free(buf);

  return sockfd;
}

void speechd_close(int fd){
   /* close the socket */
   close(fd);
}

int speechd_say(int fd, int priority, char* text){
  char helper[256];
  char *buf;

  sprintf(helper, "@priority %d\n\r", priority);
  if(! ret_ok( send_data(fd, helper, 1) )) return 0;
  sprintf(helper, "@data on\n\r");
  if(! ret_ok( send_data(fd, helper, 1) )) return 0;

  buf = malloc((strlen(text) + 4)*sizeof(char));
  if (buf == NULL) FATAL ("Not enough memory.\n");
  sprintf(buf, "%s\n\r", text); 
  send_data(fd, buf, 0);

  sprintf(helper, "@data off\n\r");
  if(! ret_ok( send_data(fd, helper, 1) )) return 0; 
  
  return 1;
}

int ret_ok(char *ret){
   if (strstr(ret, "OK")) return 1;
   if (strstr(ret, "ERROR")) return 0;
   FATAL("Internal error during communication.");
}

char* send_data(int fd, char *message, int wfr) {
   char reply[256];
   int bytes;

   /* write message to the socket */
   write(fd, message, strlen(message));

   /* read reply to the buffer */
   if (wfr == 1){
      bytes = read(fd, reply, 255);
      /* print server reply to as a string */
      reply[bytes] = 0; 
      printf("reply from server: %s (%d bytes)\n", reply, bytes);
      return reply;
   }else{
      return "NO REPLY";
   } 
}
