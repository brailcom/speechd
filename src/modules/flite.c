
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
 * $Id: flite.c,v 1.15 2003-04-14 22:54:06 hanke Exp $
 */

#define VERSION "0.1"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <pthread.h>
#include <flite/flite.h>
#include <signal.h>

#include "module.h"
#include "fdset.h"

const int DEBUG_FLITE = 0;

/* Thread and process control */
int flite_speaking = 0;
int flite_running = 0;

EVoiceType flite_cur_voice = MALE1;

pthread_t flite_speak_thread;
pid_t flite_pid;

/* Public function prototypes */
gint	flite_write			(gchar *data, gint len, TFDSetElement *);
gint	flite_stop			(void);
gint	flite_is_speaking	(void);
gint	flite_close			(void);

/* Internal functions prototypes */
void flite_speak(void);

/* Voice */
cst_voice *register_cmu_us_kal();
cst_voice *flite_voice;

/* Fill the module_info structure with pointers to this modules functions */
OutputModule modinfo_flite = {
   "flite",							/* name */
   "Software synthetizer Flite",	/* description */
   NULL,							/* filename */
   NULL,							/* GModule */
   flite_write,						/* module functions */
   flite_stop,
   flite_is_speaking,
   flite_close
};

/* Entry point of this module */
OutputModule*
module_init(void)
{
    if(DEBUG_FLITE) printf("flite: init_module()\n");

    /* Init flite and register a new voice */
    flite_init();
    flite_voice = register_cmu_us_kal();

    if (DEBUG_FLITE && (flite_voice == NULL))
        printf("Couldn't register the basic kal voice.\n"); 
	
    modinfo_flite.description = g_strdup_printf(
                                                "Flite software synthesizer, version %s",VERSION);

    assert(modinfo_flite.name != NULL);
    assert(modinfo_flite.description != NULL);
	
    return &modinfo_flite;
}

/* Public functions */

gint
flite_write(gchar *data, gint len, TFDSetElement* set)
{
    int ret;
    FILE *temp;
    float stretch;

    /* Tests */
    if(DEBUG_FLITE) printf("flite: write()\n");

    if(data == NULL){
        if(DEBUG_FLITE) printf("flite: requested data NULL\n");		
        return -1;
    }

    if(data == ""){
        if(DEBUG_FLITE) printf("flite: requested data empty\n");
        return -1;
    }

	if(DEBUG_FLITE) printf("requested data: |%s|\n", data);
	
    if (flite_speaking){
        if(DEBUG_FLITE) printf("flite: speaking when requested to flite-write");
        return 0;
    }

    /* Preparing data */
    if((temp = fopen("/tmp/flite_message", "w")) == NULL){
        printf("Flite: couldn't open temporary file\n");
        return 0;
    }
    fprintf(temp,"%s\n\r",data);
    fclose(temp);
    fflush(NULL);

    /* Setting voice */
    if (set->voice_type != flite_cur_voice){
        if(set->voice_type == MALE1){
            free(flite_voice);
            flite_voice = (cst_voice*) register_cmu_us_kal();
            flite_cur_voice = MALE1;
        }
        if(set->voice_type == MALE2){
            free(flite_voice);
            /* This is only an experimental voice */
            flite_voice = (cst_voice*) register_cmu_time_awb();	
            flite_cur_voice = MALE2;
        }
    }
	
    stretch = 1;
    if (set->speed < 0) stretch -= ((float) set->speed) / 50;
    if (set->speed > 0) stretch -= ((float) set->speed) / 200;

    feat_set_float(flite_voice->features,"duration_stretch", stretch);
    feat_set_float(flite_voice->features,"int_f0_target_mean", (((float)set->pitch) * 0.8) + 100);
  	
    /* Running Flite */
    if(DEBUG_FLITE) printf("Flite: creating new thread for flite_speak\n");
    flite_speaking = 1;
    flite_running = 1;
    ret = pthread_create(&flite_speak_thread, NULL, flite_speak, NULL);
    if(ret != 0){
        if(DEBUG_FLITE) printf("Flite: thread failed\n");
        flite_speaking = 0;
        flite_running = 0;
        return 0;
    }
		
    if(DEBUG_FLITE) printf("Flite: leaving write() normaly\n\r");
    return len;
}

gint
flite_stop(void) 
{
    if(DEBUG_FLITE) printf("flite: stop()\n");

    if(flite_running){
        if(DEBUG_FLITE) printf("flite: stopping process pid %d\n", flite_pid);
        kill(flite_pid, SIGKILL);
    }
}

gint
flite_is_speaking(void)
{
    int ret;

    if(0) printf("flite: is_speaking: %d %d", flite_speaking, flite_running);
	
    /* If flite just stopped speaking, join the thread */
    if ((flite_speaking == 1) && (flite_running == 0)){
        ret = pthread_join(flite_speak_thread, NULL);
        if (ret != 0){
            if (DEBUG_FLITE) printf("join failed!\n");
            return 1;
        }
        if(DEBUG_FLITE) printf("flite: join ok\n\r");
        flite_speaking = 0;
    }
	
    return flite_speaking; 
}

gint
flite_close(void)
{
    int ret;

    if(DEBUG_FLITE) printf("flite: close()\n");

    if(flite_speaking){
        flite_stop();
        ret = pthread_join(flite_speak_thread, NULL);
        if (ret != 0){
            if (DEBUG_FLITE) printf("join failed!\n");
            return 1;
        }
    }
   
    free(flite_voice);
    return 0;
}

/* Internal functions */
void
flite_speak(void)
{	
    int ret;
    sigset_t all_signals;	
    if(DEBUG_FLITE)	printf("flite: speaking.......\n");

    ret = sigfillset(&all_signals);
    if (ret == 0){
        ret = pthread_sigmask(SIG_BLOCK,&all_signals,NULL);
        if ((ret != 0)&&(DEBUG_FLITE))
            printf("flite: Can't set signal set, expect problems when terminating!\n");
    }else if(DEBUG_FLITE){
        printf("flite: Can't fill signal set, expect problems when terminating!\n");
    }

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);				
	
    /* Create a new process so that we could send it signals */
    flite_pid = fork();
    switch(flite_pid){
    case -1:	
        printf("Can't say the message. fork() failed!\n");
        exit(0);
    case 0:
        /* This is the child. Make flite speak, but exit on SIGINT. */
        flite_file_to_speech("/tmp/flite_message", flite_voice, "play");
        exit(0);
    default:
        /* This is the parent. Wait for the child to terminate. */
        waitpid(flite_pid,NULL,0);
    }
    if(DEBUG_FLITE) printf("flite ended.......\n");
	
    flite_running = 0;
    pthread_exit(NULL);
}	

