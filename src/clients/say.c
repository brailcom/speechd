
/*
 * say.c - Supersimple Speech Deamon client
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
 * $Id: say.c,v 1.8 2003-02-01 22:16:55 hanke Exp $
 */

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

   err = spd_sayf(sockfd, 2, (char*) argv[1]);

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
