
/*
 * clibrary.c - Testing the C library for Speech Dispatcher
 *
 * Copyright (C) 2003 Brailcom, o.p.s.
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
 * $Id: clibrary.c,v 1.2 2003-10-12 23:35:23 hanke Exp $
 */

#include <stdio.h>

#include "libspeechd.h"
#include "def.h"

int main() {
   int conn;
   int err;
   int i;   
   
   printf("Start of the test of the test.\n");
   
   printf("Trying to initialize Speech Deamon...");
   conn = spd_open("say", NULL, NULL);
   if (conn == 0){
	  printf("Speech Deamon failed");
	  exit(1);
   }
   printf("OK\n");


   printf("Say inviting message\n");
   spd_say(conn, SPD_MESSAGE, "Hello, how are you? I'm Speech Deamon");
   
   sleep(4);

   printf("\n");
   printf("Try to lower pitch and higher rate...\n");
   spd_set_voice_pitch(conn, -20);
   spd_set_voice_rate(conn, +20);
   printf("...and say something\n");
   spd_say(conn, SPD_MESSAGE, "Do you like this voice more?");
   
   sleep(4);

   printf("Switch punctuation mode to `all'.");
   printf("\n");

   printf("\n");
   printf("Keep this pitch, set higher rate and let's test priorities.\n"
          "Priority progress should choose some messages\n"
          "to report the progrees and it should allways say\n"
          "the last message.\n");

   sleep(10);

   spd_set_voice_rate(conn, +90);

   for (i = 0; i<= 100; i++){
       printf("%d%% completed\n", i);
       spd_sayf(conn, SPD_PROGRESS, "%d%% completed", i);
   }

   printf("Trying to close Speech Dispatcher connection...");
   spd_close(conn);
   printf("OK\n");

   printf("End of the test.\n");
   exit(0);
}
