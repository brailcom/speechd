
/*
 * flite.c - Speech Deamon backend for Festival
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
 * $Id: festival.c,v 1.3 2003-04-05 21:17:16 hanke Exp $
 */

#define VERSION "0.0.1"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <pthread.h>
#include <festival/festival.h>
#include <signal.h>

#include "module.h"
#include "fdset.h"

const int DEBUG_FESTIVAL = 0;

/* Thread and process control */
int festival_speaking = 0;
int festival_running = 0;

pthread_t festival_speak_thread;
pid_t festival_pid;

/* Public function prototypes */
gint	festival_write			(gchar *data, gint len, void*);
gint	festival_stop			(void);
gint	festival_is_speaking	(void);
gint	festival_close			(void);

/* Internal functions prototypes */
void* festival_speak(void*);

/* Fill the module_info structure with pointers to this modules functions */
OutputModule modinfo_festival = {
   "festival",							/* name */
   "Software synthetizer Festival",	/* description */
   NULL,							/* filename */
   NULL,							/* GModule */
   festival_write,						/* module functions */
   festival_stop,
   festival_is_speaking,
   festival_close
};

/* Entry point of this module */
OutputModule*
module_init(void)
{
	if(DEBUG_FESTIVAL) printf("festival: init_module()\n");

	/* Init festival and register a new voice */
	festival_initialize(1,210000);	/* This 210000 is from festival documentation */
	
	modinfo_festival.description = g_strdup_printf(
		"Festival software synthesizer, version %s",VERSION);

	assert(modinfo_festival.name != NULL);
	assert(modinfo_festival.description != NULL);
	
	return &modinfo_festival;
}

/* Public functions */

gint
festival_write(gchar *data, gint len, void* sett)
{
	int ret;
	FILE *temp;
	TFDSetElement *set;

	set = (TFDSetElement*) sett;

	if(DEBUG_FESTIVAL) printf("festival: write()\n");

	if(data == NULL){
		if(DEBUG_FESTIVAL) printf("festival: requested data NULL\n");		
	   	return -1;
	}

	if(data == ""){
		if(DEBUG_FESTIVAL) printf("festival: requested data empty\n");
		return -1;
	}

	if (festival_speaking){
		if(DEBUG_FESTIVAL) printf("festival: speaking when requested to festival-write");
	   	return 0;
   	}
	
	if((temp = fopen("/tmp/festival_message", "w")) == NULL){
		printf("festival: couldn't open temporary file\n");
		return 0;
	}
	fprintf(temp,"%s\n\r",data);
	fclose(temp);
	fflush(temp);
  
	if(DEBUG_FESTIVAL) printf("festival: creating new thread for festival_speak\n");
	festival_speaking = 1;
	festival_running = 1;
	ret = pthread_create(&festival_speak_thread, NULL, festival_speak, NULL);
	if(ret != 0){
		if(DEBUG_FESTIVAL) printf("festival: thread failed\n");
		festival_speaking = 0;
		festival_running = 0;
		return 0;
	}
		
   	if(DEBUG_FESTIVAL) printf("festival: leaving write() normaly\n\r");
	return len;
}

gint festival_stop(void) {
	if(DEBUG_FESTIVAL) printf("festival: stop()\n");

	if(festival_running){
		if(DEBUG_FESTIVAL) printf("festival: stopping process pid %d\n", festival_pid);
		kill(festival_pid, SIGINT);
	}
}

gint festival_is_speaking(void) {
	int ret;

	if(0) printf("festival: is_speaking: %d %d", festival_speaking, festival_running);
	
	/* If festival just stopped speaking, join the thread */
	if ((festival_speaking == 1) && (festival_running == 0)){
		ret = pthread_join(festival_speak_thread, NULL);
		if (ret != 0){
			if (DEBUG_FESTIVAL) printf("join failed!\n");
			return 1;
		}
   		if(DEBUG_FESTIVAL) printf("festival: join ok\n\r");
		festival_speaking = 0;
	}
	
	return festival_speaking; 
}

gint festival_close(void){
	int ret;

	if(DEBUG_FESTIVAL) printf("festival: close()\n");

	if(festival_speaking){
		festival_stop();
		ret = pthread_join(festival_speak_thread, NULL);
		if (ret != 0){
			if (DEBUG_FESTIVAL) printf("join failed!\n");
			return 1;
		}
	}
   
   return 0;
}

/* Internal functions */
void*
festival_speak(void*)
{	
	int ret;
	sigset_t all_signals;	
	if(DEBUG_FESTIVAL)	printf("festival: speaking.......\n");

	ret = sigfillset(&all_signals);
    if (ret == 0){
        ret = pthread_sigmask(SIG_BLOCK,&all_signals,NULL);
        if ((ret != 0)&&(DEBUG_FESTIVAL))
			printf("festival: Can't set signal set, expect problems when terminating!\n");
    }else if(DEBUG_FESTIVAL){
        printf("festival: Can't fill signal set, expect problems when terminating!\n");
    }

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);				
	
	/* Create a new process so that we could send it signals */
	festival_pid = fork();
	switch(festival_pid){
		case -1:	
			printf("Can't say the message. fork() failed!\n");
			exit(0);
		case 0:
			/* This is the child. Make festival speak, but exit on SIGINT. */
			festival_say_file("/tmp/festival_message");
			exit(0);
		default:
			/* This is the parent. Wait for the child to terminate. */
			waitpid(festival_pid,NULL,0);
	}
	if(DEBUG_FESTIVAL) printf("festival ended.......\n");
	
	festival_running = 0;
	pthread_exit(NULL);
}	

