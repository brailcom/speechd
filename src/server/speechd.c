#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "def.h"

int main() {
   int server_socket;
   struct sockaddr_in server_address;
   fd_set readfds, testfds;

   server_socket = socket(AF_INET, SOCK_STREAM, 0);

   server_address.sin_family = AF_INET;
   server_address.sin_addr.s_addr = htonl(INADDR_ANY);
   server_address.sin_port = htons(SPEECH_PORT);

   if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
      die("speechd: bind() failed");

   /* Create a connection queue and initialize readfds to handle input from server_socket. */
   if (listen(server_socket, 5) == -1)
      die("speechd: listen() failed");

   FD_ZERO(&readfds);
   FD_SET(server_socket, &readfds);

   /* Now wait for clients and requests.
    * Since we have passed a null pointer as the timeout parameter, no timeout will occur.
    * The program will exit and report an error if select returns a value of less than 1.  */
   while (1) {
      int fd, fdmax=0;
      testfds = readfds;

      printf("speech server waiting for clients...\n");

      if (select(FD_SETSIZE, &testfds, (fd_set *)0, (fd_set *)0, (struct timeval *) 0) < 1)
	 die("speechd: select() failed");

      /* Once we know we've got activity,
       * we find which descriptor it's on by checking each in turn using FD_ISSET. */

      for (fd = 0; fd < fdmax && fd < FD_SETSIZE; fd++) {
	 if (FD_ISSET(fd,&testfds)) {

	    if (fd == server_socket) { /* activity is on server_socket (request for a new connection) */

	       struct sockaddr_in client_address;
	       int client_len = sizeof(client_address);
	       int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_len);

	       /* We add the associated client_socket to the descriptor set. */
	       FD_SET(client_socket, &readfds);
	       if (client_socket > fdmax) fdmax = client_socket;
	       printf("adding client on fd %d\n", client_socket);

	    } else {	/* client activity */

	       int nread;
	       ioctl(fd, FIONREAD, &nread);

	       if (nread == 0) {
		  /* Client has gone away and we remove it from the descriptor set. */
		  close(fd);
		  FD_CLR(fd, &readfds);
		  if (fd == fdmax) fdmax--; /* this may not work every time, but who cares.. */
		  printf("removing client on fd %d\n", fd);
	       } else {
		  /* Here we 'serve' the client. */
		  char ch;
		  read(fd, &ch, 1);
		  printf("serving client on fd %d\n", fd);
		  ch++;
		  write(fd, &ch, 1);
	       }
	    }
	 }
      }
   }
}

