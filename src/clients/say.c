/* speechd simple client program
 * CVS revision: $Id: say.c,v 1.5 2002-11-02 22:10:52 hanke Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

#include "robodlib.h"
#include "def.h"

#define FATAL(msg) { perror("client: "msg); exit(1); }

int main(int argc, const char **argv) {
   int sockfd;
   struct sockaddr_in address;
   int err;

   if (argc != 2) {
      fprintf (stderr, "usage: %s ARGUMENT\n", argv[0]);
      exit (EXIT_FAILURE);
   }
   
   sockfd = rbd_init("say","main");
   if (sockfd == 0) FATAL("Robod failed");

   err = rbd_sayf(sockfd, 2, argv[1]);

   if (err != 1) FATAL("Robod failed");
  
   /* close the socket */
   rbd_close(sockfd);
   exit(0);
}
