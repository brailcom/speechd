
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
 * $Id: cstsnd.c,v 1.4 2003-07-07 08:39:25 hanke Exp $
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
#include "spd_audio.h"

#include "module_utils.c"

#define MODULE_NAME     "flite"
#define MODULE_VERSION  "0.1"

#define DEBUG_MODULE  1
DECLARE_DEBUG_FILE("/tmp/debug-flite");

/* Thread and process control */
static int cstsnd_speaking = 0;
static int cstsnd_running = 0;

static pthread_t cstsnd_speak_thread;
static pid_t cstsnd_pid;

static char* cstsnd_message;

/* Public function prototypes */
DECLARE_MODULE_PROTOTYPES();

/* Internal functions prototypes */
static void* _cstsnd_speak(void*);
static void _cstsnd_synth(void);

/* Fill the module_info structure with pointers to this modules functions */
DECLARE_MODINFO("cstsnd", "CST sound plugin from Flite 1.2");

/* Entry point of this module */
OutputModule*
module_load(void)
{
    INIT_DEBUG_FILE();

    DBG("module_load()\n");

    return &module_info;
}

int
module_init(OutputModule* module)
{
    return 0;
}

/* Public functions */

static gint
module_write(gchar *data, size_t bytes, TFDSetElement* set)
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

    DBG("cstsnd: requested file: |%s|\n", data);
	
    if (cstsnd_speaking){
        DBG("cstsnd: speaking when requested to cstsnd-write");
        return 0;
    }

    cstsnd_message = strdup(data);

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

static gint
module_stop(void) 
{
    DBG("cstsnd: stop()\n");

    if(cstsnd_running){
        DBG("cstsnd: stopping process pid %d\n", cstsnd_pid);
        kill(cstsnd_pid, SIGKILL);
    }
}

static size_t
module_pause(void)
{
     return 0;
}

static gint
module_is_speaking(void)
{
    int ret;

    /* If cstsnd just stopped speaking, join the thread */
    if ((cstsnd_speaking == 1) && (cstsnd_running == 0)){
        ret = pthread_join(cstsnd_speak_thread, NULL);
        if (ret != 0){
            DBG("join failed!\n");
            return 1;
        }
        DBG("cstsnd: join ok\n\r");
        cstsnd_speaking = 0;
    }
	
    return cstsnd_speaking; 
}

static gint
module_close(void)
{
    int ret;

    DBG("cstsnd: close()\n");

    if(cstsnd_speaking){
        module_stop();
        ret = pthread_join(cstsnd_speak_thread, NULL);
        if (ret != 0){
            DBG("join failed!\n");
            return 1;
        }
    }
   
    return 0;
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

        _cstsnd_synth();

    default:
        /* This is the parent. Wait for the child to terminate. */
        waitpid(cstsnd_pid, &status, 0);

        DBG("child terminated -: status:%d signal?:%d signal number:%d.\n",
            WIFEXITED(status), WIFSIGNALED(status), WTERMSIG(status));

    }

    DBG("cstsnd terminated.......\n");
	
    cstsnd_running = 0;
    pthread_exit(NULL);
}	

static void
_cstsnd_synth()
{
    int ret;
    cst_wave* wave;
    sigset_t *all_signals;	

    sigfillset(all_signals);
    ret = sigprocmask(SIG_BLOCK, all_signals, NULL);
    if (ret != 0)
        DBG("flite: Can't block signals, expect problems with terminating!\n");

    wave = (cst_wave*) spd_audio_read_wave(cstsnd_message);
 
    spd_audio_open(wave);
    spd_audio_play_wave(wave);
    spd_audio_close(wave);
}

