
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
 * $Id: say.c,v 1.8 2005-05-13 10:35:16 hanke Exp $
 */

#include <stdio.h>
#include <assert.h>
#include "libspeechd.h"
#include "options.h"

#define FATAL(msg) { perror("client: "msg); exit(1); }

int main(int argc, char **argv) {
   int sockfd;
   int err;
   SPDPriority spd_priority;

   /* Check if the text to say is specified in the argument */
   if (argc < 2) {
       options_print_help(argv);
       return 1;
   }

   rate = -101;
   pitch = -101;
   volume = -101;
   language = NULL;
   voice_type = NULL;
   punctuation_mode = NULL;
   spelling = -2;
   priority = NULL;

   options_parse(argc, argv);
   
   /* Open a connection to Speech Dispatcher */
   sockfd = spd_open("say","main", NULL);
   if (sockfd == 0) FATAL("Speech Dispatcher failed to open");

   /* Set the desired parameters */

   if (output_module != NULL)
       if(spd_set_output_module(sockfd, output_module))
	   printf("Invalid language!\n");


   if (language != NULL)
       if(spd_set_language(sockfd, language))
	   printf("Invalid language!\n");

   if (voice_type != NULL){
       int ret;
       if (!strcmp(voice_type, "male1")){
	   if(spd_set_voice_type(sockfd, SPD_MALE1))
	       printf("Can't set this voice!\n");
       }
       else if(!strcmp(voice_type, "male2")){
	   if(spd_set_voice_type(sockfd, SPD_MALE2))
	       printf("Can't set this voice!\n");
       }
       else if(!strcmp(voice_type, "male3")){
	   if(spd_set_voice_type(sockfd, SPD_MALE3))
	       printf("Can't set this voice!\n");
       }
       else if(!strcmp(voice_type, "female1")){
	   if(spd_set_voice_type(sockfd, SPD_FEMALE1))
	       printf("Can't set this voice!\n");
       }
       else if(!strcmp(voice_type, "female2")){
	   if(spd_set_voice_type(sockfd, SPD_FEMALE2))
	       printf("Can't set this voice!\n");
       }
       else if(!strcmp(voice_type, "female3")){
	   if(spd_set_voice_type(sockfd, SPD_FEMALE3))
	       printf("Can't set this voice!\n");
       }
       else if(!strcmp(voice_type, "child_male")){
	   if(spd_set_voice_type(sockfd, SPD_CHILD_MALE))
	       printf("Can't set this voice!\n");
       }
       else if(!strcmp(voice_type, "child_female")){
	   if(spd_set_voice_type(sockfd, SPD_CHILD_FEMALE))
	       printf("Can't set this voice!\n");
       }else{
	   printf("Invalid voice\n");
       }
   }

   if (rate != -101) 
       if(spd_set_voice_rate(sockfd, rate))
	   printf("Invalid rate!\n");

   if (pitch != -101) 
       if(spd_set_voice_pitch(sockfd, pitch))
	   printf("Invalid pitch!\n");
   
   if (volume != -101) 
       if(spd_set_volume(sockfd, volume))
	   printf("Invalid volume!\n");

   if (spelling == 1)
       if(spd_set_spelling(sockfd, SPD_SPELL_ON))
	   printf("Can't set spelling to on!\n");

   if (punctuation_mode != NULL){
       if (!strcmp(punctuation_mode, "none")){
	   if(spd_set_punctuation(sockfd, SPD_PUNCT_NONE))
	       printf("Can't set this punctuation mode!\n");
       }
       else if(!strcmp(punctuation_mode, "some")){
	   if(spd_set_punctuation(sockfd, SPD_PUNCT_SOME))
	       printf("Can't set this punctuation mode!\n");
       }
       else if(!strcmp(punctuation_mode, "all")){
	   if(spd_set_punctuation(sockfd, SPD_PUNCT_ALL))
	       printf("Can't set this punctuation mode!\n");
       }else{
	   printf("Invalid punctuation mode.\n");
       }
   }

   /* Set default priority... */
   spd_priority = SPD_TEXT;
   /* ...and look if it wasn't overriden */
   if (priority != NULL){
       if (!strcmp(priority, "important")) spd_priority = SPD_IMPORTANT;
       else if (!strcmp(priority, "message")) spd_priority = SPD_MESSAGE;
       else if (!strcmp(priority, "text")) spd_priority = SPD_TEXT;
       else if (!strcmp(priority, "notification")) spd_priority = SPD_NOTIFICATION;
       else if (!strcmp(priority, "progress")) spd_priority = SPD_PROGRESS;
       else{
	   printf("Invalid priority.\n");
       }
   }

	   

   /* Say the message with priority "text" */
   assert(argv[argc-1]);
   err = spd_sayf(sockfd, spd_priority, (char*) argv[argc-1]);
   if (err == -1) FATAL("Speech Dispatcher failed to say message");
   
   /* Close the connection */
   spd_close(sockfd);
  
   return 0;
}
