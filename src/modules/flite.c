
/*
 * flite.c - Speech Deamon backend for Flite (Festival Lite)
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
 * $Id: flite.c,v 1.9 2003-03-19 19:32:40 pdm Exp $
 */

#define VERSION "0.0.2"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <pthread.h>
#include <flite/flite.h>
#include <signal.h>

#include "module.h"
#include "fdset.h"

#define FATAL(msg) { printf("flite output module for speechd [%s:%d]: "msg"\n", __FILE__, __LINE__); exit(1); }

int f_speaking = 0;
int f_running = 0;

pthread_t f_speak_thread;

gint       flite_write      (const gchar *data, gint len, TFDSetElement *);
gint       flite_stop       (void);
gint       flite_is_speaking (void);
gint       flite_close(void);

/* fill the module_info structure with pointers to this modules functions */
OutputModule modinfo_flite = {
   "flite",
   "Software synthetizer Flite",
   NULL, /* filename */
   flite_write,
   flite_stop,
   flite_is_speaking,
   flite_close
};

cst_voice *register_cmu_us_kal();
cst_voice *mvoice;

pid_t flite_pid;

/* entry point of this module */
OutputModule *module_init(void) {
   printf("flite: init_module()\n");

   /* Init flite and register a new voice */
   flite_init();
   mvoice = register_cmu_us_kal();
   
   /*modinfo.name = g_strdup("flite"),
   modinfo_flite.description = g_strdup_printf(
	"Flite software synthesizer, version %s",VERSION);*/
   return &modinfo_flite;
}

void
flite_speak()
{	
//	printf("flite speaking.......\n");
	flite_pid = fork();
	switch(flite_pid){
		case -1:	FATAL("fork failed!\n");
					exit(0);
		case 0:
			/* This is the child. Make flite speak, but exit on SIGINT. */
			flite_file_to_speech("/tmp/flite_message", mvoice, "play");
			exit(0);
		default:
			/* This is the parent. Wait for the child to terminate. */
			waitpid(flite_pid,NULL,0);
	}
//	printf("flite ended.......\n");
	f_running = 0;
	pthread_exit(NULL);
}	

/* module operations */
gint flite_write(const gchar *data, gint len, TFDSetElement* set) {
	int ret;
	FILE *temp;

//	printf("flite: write()\n");

	if (f_speaking) return 0;
   
	if((temp = fopen("/tmp/flite_message", "w")) == NULL)
		FATAL("Output module flite couldn't open temporary file");
	fprintf(temp,"%s\n\r",data);
	fclose(temp);
   
   {
//		printf("creating new thread for flite speak\n");
		f_speaking = 1;
		f_running = 1;
		ret = pthread_create(&f_speak_thread, NULL, flite_speak, NULL);
		if(ret != 0) printf("THREAD FAILED\n");
	}
   
//	printf("flite: leaving\n\r");
	return len;
}

gint flite_stop(void) {
//	printf("flite: stop()\n");
	if(f_speaking){
		kill(flite_pid, SIGINT);
	}
}

gint flite_is_speaking(void) {
	int ret;
	if ((f_speaking == 1) && (f_running == 0)){
		ret = pthread_join(f_speak_thread, NULL);
		if (ret != 0) FATAL("join failed!\n");
		f_speaking = 0;
	}
	return f_speaking; 
}

gint flite_close(void){
//   printf("flite: close()\n");
   return 0;
}

