/* speechd simple client program
 * CVS revision: $Id: say.c,v 1.7 2002-12-16 01:42:13 hanke Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

#include "libspeechd.h"
#include "def.h"

#define FATAL(msg) { perror("client: "msg); exit(1); }

int main(int argc, const char **argv) {
   int sockfd;
   struct sockaddr_in address;
   int err;
   int c;
   int *cl_ids;
   char **cl_names;
   int i = 0;

   if (argc != 2) {
      fprintf (stderr, "usage: %s ARGUMENT\n", argv[0]);
      exit (EXIT_FAILURE);
   }
   
   sockfd = spd_init("say","main");
   if (sockfd == 0) FATAL("Speech Deamon failed");

   err = spd_sayf(sockfd, 2, argv[1]);

   if (err != 1) FATAL("Speech Deamon failed");

   cl_names = malloc(sizeof(char*) * 64);      //TODO: must scale
   cl_ids = malloc(sizeof(int) * 64);      //TODO: must scale	   
   
   c = spd_get_client_list(sockfd, cl_names, cl_ids);

   printf("Count: %d\n", c);

   for(i=0;i<=c-1;i++){
   	printf("id: %d name: %s \n", cl_ids[i], cl_names[i]);
   }
   
   /* close the socket */
   spd_close(sockfd);
   exit(0);
}
