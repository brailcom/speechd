
/*
 * cstsnd.c - Speech Deamon backend for sound output
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
 * $Id: cstsnd.c,v 1.2 2003-05-18 20:54:40 hanke Exp $
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

const int DEBUG_CSTSND = 0;

/* Thread and process control */
int cstsnd_speaking = 0;
int cstsnd_running = 0;

pthread_t cstsnd_speak_thread;
pid_t cstsnd_pid;

/* Public function prototypes */
gint	cstsnd_write			(gchar *data, gint len, void*);
gint	cstsnd_stop			(void);
gchar*  cstsnd_pause                    (void);
gint	cstsnd_is_speaking	        (void);
gint	cstsnd_close			(void);

/* Internal functions prototypes */
void* _cstsnd_speak(void*);
void _cstsnd_synth(void);
void _cstsnd_sigpause();
int cstsnd_pause_requested = 0;

/* Fill the module_info structure with pointers to this modules functions */
OutputModule modinfo_cstsnd = {
   "cstsnd",                     /* name */
   "Software synthetizer Cstsnd", /* description */
   NULL,                        /* GModule (should be set to NULL)*/
   cstsnd_write,                 /* module functions */
   cstsnd_stop,
   cstsnd_pause,
   cstsnd_is_speaking,
   cstsnd_close,
   {0,0}
};

int
module_init(OutputModule* module)
{
    return 0;
}

/* Entry point of this module */
OutputModule*
module_load(void)
{
    if(DEBUG_CSTSND) printf("cstsnd: init_module()\n");

    /* Init cstsnd and register a new voice */

    modinfo_cstsnd.description =
        g_strdup_printf("Cstsnd sound output module, version %s",VERSION);

    assert(modinfo_cstsnd.name != NULL);
    assert(modinfo_cstsnd.description != NULL);

    return &modinfo_cstsnd;
}

/* Public functions */

gint
cstsnd_write(gchar *data, gint len, void* v_set)
{
    int ret;
    FILE *temp;

    /* Tests */
    if(DEBUG_CSTSND) printf("cstsnd: write()\n");

    if(data == NULL){
        if(DEBUG_CSTSND) printf("cstsnd: requested data NULL\n");		
        return -1;
    }

    if(data == ""){
        if(DEBUG_CSTSND) printf("cstsnd: requested data empty\n");
        return -1;
    }

    if(DEBUG_CSTSND) printf("cstsnd: requested data: |%s|\n", data);
	
    if (cstsnd_speaking){
        if(DEBUG_CSTSND) printf("cstsnd: speaking when requested to cstsnd-write");
        return 0;
    }

    /* Preparing data */
    if((temp = fopen("/tmp/cstsnd_message", "w")) == NULL){
        printf("Cstsnd: couldn't open temporary file\n");
        return 0;
    }
    fprintf(temp,"%s",data);
    fclose(temp);
    fflush(NULL);

    /* Running Cstsnd */
    if(DEBUG_CSTSND) printf("Cstsnd: creating new thread for cstsnd_speak\n");
    cstsnd_speaking = 1;
    cstsnd_running = 1;
    ret = pthread_create(&cstsnd_speak_thread, NULL, _cstsnd_speak, NULL);
    if(ret != 0){
        if(DEBUG_CSTSND) printf("Cstsnd: thread failed\n");
        cstsnd_speaking = 0;
        cstsnd_running = 0;
        return 0;
    }
		
    if(DEBUG_CSTSND) printf("cstsnd: leaving write() normaly\n\r");
    return len;
}

gint
cstsnd_stop(void) 
{
    if(DEBUG_CSTSND) printf("cstsnd: stop()\n");

    if(cstsnd_running){
        if(DEBUG_CSTSND) printf("cstsnd: stopping process pid %d\n", cstsnd_pid);
        kill(cstsnd_pid, SIGKILL);
    }
}

gchar*
cstsnd_pause(void)
{
    GString *pause_text;
    char* text;
    char *ret;
    FILE* temp;
    
    text = malloc(1024 * sizeof(char));

    if(cstsnd_speaking){
        if(kill(cstsnd_pid, SIGUSR1)) printf("cstsnd: error in kill\n");;
        while(cstsnd_running) usleep(10);

        if((temp = fopen("/tmp/cstsnd_message", "r")) == NULL){
            printf("Cstsnd: couldn't open temporary file\n");
            return 0;
        }

        pause_text = g_string_new("");
        while(1){
            ret = fgets(text, 1024, temp);
            if (ret == NULL) break;
            pause_text = g_string_append(pause_text, text);
        }

        if (pause_text == NULL){
            printf("text null");
            return NULL;
        }
        if (pause_text->str == NULL){
                        printf("text str null");
                        return NULL;
        }
        if (strlen(pause_text->str) == 0){
            printf("no text");
            return NULL;
        }

        return pause_text->str;
    }
    printf("cstsnd not running");
    return NULL;
}

gint
cstsnd_is_speaking(void)
{
    int ret;

    /* If cstsnd just stopped speaking, join the thread */
    if ((cstsnd_speaking == 1) && (cstsnd_running == 0)){
        ret = pthread_join(cstsnd_speak_thread, NULL);
        if (ret != 0){
            if (DEBUG_CSTSND) printf("join failed!\n");
            return 1;
        }
        if(DEBUG_CSTSND) printf("cstsnd: join ok\n\r");
        cstsnd_speaking = 0;
    }
	
    return cstsnd_speaking; 
}

gint
cstsnd_close(void)
{
    int ret;

    if(DEBUG_CSTSND) printf("cstsnd: close()\n");

    if(cstsnd_speaking){
        cstsnd_stop();
        ret = pthread_join(cstsnd_speak_thread, NULL);
        if (ret != 0){
            if (DEBUG_CSTSND) printf("join failed!\n");
            return 1;
        }
    }
   
    return 0;
}

/* Internal functions */
void*
_cstsnd_speak(void* nothing)
{	
    int ret;
    sigset_t all_signals;	

    if(DEBUG_CSTSND)	printf("cstsnd: speaking.......\n");

    ret = sigfillset(&all_signals);
    if (ret == 0){
        ret = pthread_sigmask(SIG_BLOCK,&all_signals,NULL);
        if ((ret != 0)&&(DEBUG_CSTSND))
            printf("cstsnd: Can't set signal set, expect problems when terminating!\n");
    }else if(DEBUG_CSTSND){
        printf("cstsnd: Can't fill signal set, expect problems when terminating!\n");
    }

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);				
	
    /* Create a new process so that we could send it signals */
    cstsnd_pid = fork();
    switch(cstsnd_pid){
    case -1:	
        printf("Can't say the message. fork() failed!\n");
        exit(0);

    case 0:
        /* This is the child. Make cstsnd speak, but exit on SIGINT. */

        if(signal(SIGUSR1, _cstsnd_sigpause) == SIG_ERR) printf("signal error");
        _cstsnd_synth();

    default:
        /* This is the parent. Wait for the child to terminate. */
        waitpid(cstsnd_pid,NULL,0);
    }

    if(DEBUG_CSTSND) printf("cstsnd ended.......\n");
	
    cstsnd_running = 0;
    pthread_exit(NULL);
}	

void
_cstsnd_sigunblockusr(sigset_t *some_signals)
{
    int ret;

    sigdelset(some_signals, SIGUSR1);
    ret = sigprocmask(SIG_SETMASK, some_signals, NULL);
    if ((ret != 0)&&(DEBUG_CSTSND))
        printf("cstsnd: Can't block signal set, expect problems with terminating!\n");
}

void
_cstsnd_sigblockusr(sigset_t *some_signals)
{
    int ret;

    sigaddset(some_signals, SIGUSR1);
    ret = sigprocmask(SIG_SETMASK, some_signals, NULL);
    if ((ret != 0)&&(DEBUG_CSTSND))
        printf("cstsnd: Can't block signal set, expect problems with terminating!\n");
}

void
_cstsnd_synth()
{
    char *text;
    char *ret;
    FILE* sp_file;
    GString *cstsnd_pause_text;
    cst_wave* wave;
    char *wavefile;
    sigset_t some_signals;	
    int i;
    char c;
    int terminate = 0;

    sigfillset(&some_signals);
    _cstsnd_sigunblockusr(&some_signals);

    wavefile = malloc(1024 * sizeof(char));
    sp_file = fopen("/tmp/cstsnd_message","r");

    if (sp_file == NULL){
        printf("cstsnd: Can't open Cstsnd temporary file\n");
        exit(0);
    }

    wavefile = fgets(text, 1024, sp_file);

    wave = spd_audio_read_wave(wavefile);

    _cstsnd_sigblockusr(&some_signals);        
 
    spd_audio_open(wave);
    spd_audio_play_wave(wave);
    spd_audio_close(wave);

    _cstsnd_sigunblockusr(&some_signals);

}

void
_cstsnd_sigpause()
{
    cstsnd_pause_requested = 1;
    if(DEBUG_CSTSND) printf("cstsnd: received signal for pause\n");
}
