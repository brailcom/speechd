
/*
 * say.c - Super-simple Speech Dispatcher client
 *
 * Copyright (C) 2001, 2002, 2003 Brailcom, o.p.s.
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
 * $Id: say.c,v 1.5 2003-07-06 15:09:18 hanke Exp $
 */

#include <stdio.h>
#include "libspeechd.h"

#define FATAL(msg) { perror("client: "msg); exit(1); }

int main(int argc, const char **argv) {
   int sockfd;
   int err;

   /* Check if the text to say is specified as argument */
   if (argc != 2) {
      fprintf (stderr, "usage: %s ARGUMENT\n", argv[0]);
      return 1;
   }
   
   /* Open a connection to Speech Deamon */
   sockfd = spd_open("say","main", NULL);
   if (sockfd == 0) FATAL("Speech Dispatcher failed to open");

   /* Say the message with priority "text" */
   err = spd_sayf(sockfd, SPD_TEXT, (char*) argv[1]);
   if (err == -1) FATAL("Speech Dispatcher failed to say message");
   
   /* Close the connection */
   spd_close(sockfd);
  
   return 0;
}
