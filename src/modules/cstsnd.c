
/*
 * cstsnd.c - Speech Dispatcher backend for sound output
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
 * $Id: cstsnd.c,v 1.9 2003-10-07 16:52:00 hanke Exp $
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

#include "module_utils.c"
#include "module_utils_audio.c"

#define MODULE_NAME     "cstsnd"
#define MODULE_VERSION  "0.1"

DECLARE_DEBUG();

/* Thread and process control */
static int cstsnd_speaking = 0;
static int cstsnd_running = 0;

static pthread_t cstsnd_speak_thread;
static pid_t cstsnd_pid;

static char* cstsnd_message;

/* Internal functions prototypes */
static void* _cstsnd_speak(void*);
static void _cstsnd_synth(void);

/* Entry point of this module */
int
module_load(void)
{   
    INIT_SETTINGS_TABLES();
    REGISTER_DEBUG();

    DBG("module_load()\n");

    return 0;
}

int
module_init(void)
{
   INIT_DEBUG(); 
   return 0;
}

/* Public functions */

int
module_speak(char *data, size_t bytes, EMessageType msgtype)
{
    int ret;

    /* Tests */
    DBG("cstsnd: write()\n");

    if(data == NULL){
        DBG("cstsnd: requested data NULL\n");		
        return -1;
    }

    if(data == ""){
        DBG("cstsnd: requested data empty\n");
        return -1;
    }
	
    if (cstsnd_speaking){
        DBG("cstsnd: speaking when requested to cstsnd-write");
        return 0;
    }

    cstsnd_message = strdup(data);

    /* Strip the trailing "\n" */
    {
        int l;
        l = strlen(cstsnd_message) - 1;
        if ((cstsnd_message[l] == '\n') || (cstsnd_message[l] == '\r'))
            cstsnd_message[l] = 0;
        l--;
        if ((cstsnd_message[l] == '\n') || (cstsnd_message[l] == '\r'))
            cstsnd_message[l] = 0;
    }

    DBG("cstsnd: requested file: |%s|\n", cstsnd_message);    

    /* Running Cstsnd */
    DBG("Cstsnd: creating new thread for cstsnd_speak\n");
    cstsnd_speaking = 1;
    cstsnd_running = 1;
    ret = pthread_create(&cstsnd_speak_thread, NULL, _cstsnd_speak, NULL);
    if(ret != 0){
        DBG("Cstsnd: thread failed\n");
        cstsnd_speaking = 0;
        cstsnd_running = 0;
        return 0;
    }
		
    DBG("cstsnd: leaving write() normaly\n\r");
    return bytes;
}

int
module_stop(void) 
{
    DBG("cstsnd: stop()\n");

    if(cstsnd_running){
        DBG("cstsnd: stopping process pid %d\n", cstsnd_pid);
        kill(-cstsnd_pid, SIGKILL); /* Kill the process group */
    }
    
}

size_t
module_pause(void)
{
     return 0;
}

int
module_is_speaking(void)
{
    int ret;

    /* If cstsnd just stopped speaking, join the thread */
    if ((cstsnd_speaking == 1) && (cstsnd_running == 0)){
        ret = pthread_join(cstsnd_speak_thread, NULL);
        if (ret != 0){
            DBG("join failed!\n");
            module_signal_end();
            return 1;
        }
        DBG("cstsnd: join ok\n\r");
        cstsnd_speaking = 0;
    }
	
    return cstsnd_speaking; 
}

void
module_close(int status)
{
    int ret;

    DBG("cstsnd: close()\n");

    if(cstsnd_speaking){
        module_stop();
        ret = pthread_join(cstsnd_speak_thread, NULL);
        if (ret != 0){
            DBG("join failed!\n");
            exit(1);
        }
    }
   
    exit(status);
}

/* Internal functions */
static void*
_cstsnd_speak(void* nothing)
{	
    int ret;
    int status;

    DBG("cstsnd: speaking.......\n");

    set_speaking_thread_parameters();

    /* Create a new process so that we could send it signals */
    cstsnd_pid = fork();
    switch(cstsnd_pid){
    case -1:	
        DBG("Can't say the message. fork() failed!\n");
        exit(0);

    case 0:
        /* This is the child. Make cstsnd speak, but exit on SIGINT. */
        setpgrp();
        _cstsnd_synth();
        
        exit(0);
    default:
        /* This is the parent. Wait for the child to terminate. */
        waitpid(cstsnd_pid, &status, 0);

        DBG("child terminated -: status:%d signal?:%d signal number:%d.\n",
            WIFEXITED(status), WIFSIGNALED(status), WTERMSIG(status));
    }

    DBG("cstsnd terminated.......\n");
	
    cstsnd_running = 0;
    module_signal_end();
    pthread_exit(NULL);
}	

static void
_cstsnd_synth()
{
    int ret;
    cst_wave* wave;
    sigset_t all_signals;	

    sigfillset(&all_signals);
    ret = sigprocmask(SIG_BLOCK, &all_signals, NULL);
    if (ret != 0)
        DBG("flite: Can't block signals, expect problems with terminating!\n");

    spd_audio_play_file(cstsnd_message);
    DBG("cstsnd_message: |%s|\n", cstsnd_message);
}

#include "module_main.c"
