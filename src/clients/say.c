/* speechd simple client program
 * CVS revision: $Id: say.c,v 1.2 2002-07-07 21:43:42 hanke Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

#include "def.h"
#include "speechdlib.h"

#define FATAL(msg) { perror("client: "msg); exit(1); }

int main(int argc, const char **argv) {
   int sockfd;
   struct sockaddr_in address;
   int err;

   if (argc != 2) {
      fprintf (stderr, "usage: %s ARGUMENT\n", argv[0]);
      exit (EXIT_FAILURE);
   }
   
   sockfd = speechd_init("say","main");
   if (sockfd == 0) FATAL("Speechd failed");

   err = speechd_say(sockfd, 2, argv[1]);

   if (err != 1) FATAL("Speechd failed");
  
   /* close the socket */
   speechd_close(sockfd);
   exit(0);
}
