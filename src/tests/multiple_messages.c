
/*
 * multiple_messages.c - Multiple messages Speech Deamon test
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
 * $Id: multiple_messages.c,v 1.1 2003-03-16 19:53:36 hanke Exp $
 */

#include <stdio.h>

#include "libspeechd.h"
#include "def.h"

#define FATAL(msg) { perror("client: "msg); exit(1); }

int main() {
   int sockfd;
   int err;
   int i, j;
   
	printf("Start of the test of the test.\n");
   
	printf("Trying to initialize Speech Deamon...");
	sockfd = spd_init("say","main");
	if (sockfd == 0) FATAL("Speech Deamon failed");
	printf("OK\n");

	for(i=1;i<=3;i++){
		printf("Testing 100 messages on priority %d:\n", i);
		if (i==1) printf("The messages shouldn't interrupt each other\n");
		if (i==2) printf("The messages shouldn't interrupt each other\n");
		if (i==3) printf("You shouldn't hear anything during sending these messages\n");
		sleep(1);
		printf("Start");
		fflush(NULL);
		for (j=0;j<100;j++){
			err = spd_sayf(sockfd, i, "These are many many many many many many many many many many many
						   many many many many many many many many many many many many  many many many
						   many many many many many many many many many bytes of text in level 3.
				   ");
			if (err == -1) printf("ERROR");
			else printf(".");
			fflush(NULL);
		}
		printf("(wait) ");
		sleep(2);
		spd_stop(sockfd);
		printf("stop!\n");
		printf("\n");
	}

   
	printf("Trying to close Speech Deamon connection...");
	spd_close(sockfd);
	printf("OK\n");

	printf("End of the test.\n");
	exit(0);
}
