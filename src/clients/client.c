/*  Make the necessary includes and set up the variables.  */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "def.h"

int main()
{
	int sockfd;
	struct sockaddr_in address;
	char ch = 'A';

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_port = htons(SPEECH_PORT);

	if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) ==-1)
		die("client: Unable to connect to server");

	/* We can now read/write via sockfd. */
	write(sockfd, &ch, 1);
	read(sockfd, &ch, 1);
	printf("char from server = %c\n", ch);
	close(sockfd);
	exit(0);
}
