/* speechd simple client program
 * CVS revision: $Id: say.c,v 1.1 2001-04-10 10:42:05 cerha Exp $
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

#define FATAL(msg) { perror("client: "msg); exit(1); }

int main(int argc, const char **argv) {
   int sockfd;
   struct sockaddr_in address;
   char reply[256];
   int bytes;

   if (argc != 2) {
      fprintf (stderr, "usage: %s ARGUMENT\n", argv[0]);
      exit (EXIT_FAILURE);
   }
   
   sockfd = socket(AF_INET, SOCK_STREAM, 0);

   address.sin_family = AF_INET;
   address.sin_addr.s_addr = inet_addr("127.0.0.1");
   address.sin_port = htons(SPEECH_PORT);

   /* connect to server */
   if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) ==-1)
      FATAL("Unable to connect to server");

   /* write message to the socket */
   write(sockfd, argv[1], strlen(argv[1]));

   /* read reply to the buffer */
   bytes = read(sockfd, reply, 255);

   /* print server reply to as a string */
   reply[bytes] = 0; 
   printf("reply from server: %s (%d bytes)\n", reply, bytes);

   /* close the socket */
   close(sockfd);
   exit(0);
}
