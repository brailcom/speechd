
/*
 * flite.c - Speech Dispatcher backend for Flite (Festival Lite)
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
 * $Id: flite.c,v 1.24 2003-06-02 15:47:39 hanke Exp $
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

const int DEBUG_FLITE = 0;

/* Thread and process control */
int flite_speaking = 0;
int flite_running = 0;

EVoiceType flite_cur_voice = MALE1;

pthread_t flite_speak_thread;
pid_t flite_pid;

/* Public function prototypes */
gint	flite_write			(gchar *data, gint len, void*);
gint	flite_stop			(void);
gchar*  flite_pause                     (void);
gint	flite_is_speaking	        (void);
gint	flite_close			(void);

/* Internal functions prototypes */
void* _flite_speak(void*);
void _flite_synth(void);
void _flite_sigpause();
int flite_pause_requested = 0;

/* Voice */
cst_voice *register_cmu_us_kal();
cst_voice *flite_voice;

/* Fill the module_info structure with pointers to this modules functions */
OutputModule modinfo_flite = {
   "flite",                     /* name */
   "Software synthetizer Flite", /* description */
   NULL,                        /* GModule (should be set to NULL)*/
   flite_write,                 /* module functions */
   flite_stop,
   flite_pause,
   flite_is_speaking,
   flite_close,
   {0, 0}
};


int
module_init(OutputModule *module)
{
    /* Init flite and register a new voice */
    flite_init();
    flite_voice = register_cmu_us_kal();

    if (DEBUG_FLITE && (flite_voice == NULL))
        printf("Couldn't register the basic kal voice.\n"); 

    return 0;
}

/* Entry point of this module */
OutputModule*
module_load(void)
{
    if(DEBUG_FLITE) printf("flite: init_module()\n");
	
    modinfo_flite.description =
        g_strdup_printf("Flite software synthesizer, version %s",VERSION);

    assert(modinfo_flite.name != NULL);
    assert(modinfo_flite.description != NULL);

    modinfo_flite.settings.params = g_hash_table_new(g_str_hash, g_str_equal);
    modinfo_flite.settings.voices = g_hash_table_new(g_str_hash, g_str_equal);

    return &modinfo_flite;
}

/* Public functions */

gint
flite_write(gchar *data, gint len, void* v_set)
{
    int ret;
    FILE *temp;
    float stretch;
    TFDSetElement *set = v_set;

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
            /* This is only an experimental voice. But if you want
             to know what's the time... :)*/
            flite_voice = (cst_voice*) register_cmu_time_awb();	
            flite_cur_voice = MALE2;
        }
    }
	
    stretch = 1;
    if (set->rate < 0) stretch -= ((float) set->rate) / 50;
    if (set->rate > 0) stretch -= ((float) set->rate) / 200;

    feat_set_float(flite_voice->features,"duration_stretch", stretch);
    feat_set_float(flite_voice->features,"int_f0_target_mean", (((float)set->pitch) * 0.8) + 100);
  	
    /* Running Flite */
    if(DEBUG_FLITE) printf("Flite: creating new thread for flite_speak\n");
    flite_speaking = 1;
    flite_running = 1;
    ret = pthread_create(&flite_speak_thread, NULL, _flite_speak, NULL);
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

gchar*
flite_pause(void)
{
    GString *pause_text;
    char* text;
    char *ret;
    FILE* temp;
    
    text = malloc(1024 * sizeof(char));

    if(flite_speaking){
        if(kill(flite_pid, SIGUSR1)) printf("flite: error in kill\n");;
        while(flite_running) usleep(10);

        if((temp = fopen("/tmp/flite_message", "r")) == NULL){
            printf("Flite: couldn't open temporary file\n");
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
    printf("flite not running");
    return NULL;
}

gint
flite_is_speaking(void)
{
    int ret;

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
void*
_flite_speak(void* nothing)
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
        return NULL;

    case 0:
        /* This is the child. Make flite speak, but exit on SIGINT. */

        if(signal(SIGUSR1, _flite_sigpause) == SIG_ERR) printf("signal error");
        _flite_synth();

    default:
        /* This is the parent. Wait for the child to terminate. */
        waitpid(flite_pid,NULL,0);
    }

    if(DEBUG_FLITE) printf("flite ended.......\n");
	
    flite_running = 0;
    pthread_exit(NULL);
}	

void
_flite_sigunblockusr(sigset_t *some_signals)
{
    int ret;

    sigdelset(some_signals, SIGUSR1);
    ret = sigprocmask(SIG_SETMASK, some_signals, NULL);
    if ((ret != 0)&&(DEBUG_FLITE))
        printf("flite: Can't block signal set, expect problems with terminating!\n");
}

void
_flite_sigblockusr(sigset_t *some_signals)
{
    int ret;

    sigaddset(some_signals, SIGUSR1);
    ret = sigprocmask(SIG_SETMASK, some_signals, NULL);
    if ((ret != 0)&&(DEBUG_FLITE))
        printf("flite: Can't block signal set, expect problems when terminating!\n");
}

void
_flite_synth()
{
    char *text;
    char *ret;
    FILE* sp_file;
    GString *flite_pause_text;
    cst_wave* wave;
    sigset_t some_signals;	
    int i;
    char c;
    int terminate = 0;

    sigfillset(&some_signals);
    _flite_sigunblockusr(&some_signals);

    text = malloc(1024 * sizeof(char));
    sp_file = fopen("/tmp/flite_message","r");

    if (sp_file == NULL){
        printf("flite: Can't open Flite temporary file\n");
        exit(0);
    }

    /* We have separated the first run, because we have to open
       the audio device also */
    wave = flite_text_to_wave("test", flite_voice);
    spd_audio_open(wave);

    /* Read the remaining groups of bytes, synthesize them and send
       to sound output */
    while(1){
        if (flite_pause_requested == 1){          
            flite_pause_text = g_string_new("");
            //            printf("string created\n");
            while(1){

                ret = fgets(text, 1024, sp_file);

                if (ret == NULL){
                    fclose(sp_file);
                    if((sp_file = fopen("/tmp/flite_message", "w")) == NULL){
                        printf("Flite: couldn't open temporary file\n");
                        return;
                    }
                    fprintf(sp_file,"%s",flite_pause_text->str);
                    fclose(sp_file);
                    if (DEBUG_FLITE) printf("terminating flite run\n");
                    free(text);
                    exit(0);
                }
                flite_pause_text = g_string_append(flite_pause_text, text);
            }
        }

        for (i=0; i<=510; i++){
            c = fgetc(sp_file);
            if (c == EOF){
                terminate = 1;
            }
            if (c == '.' || c == '!' || c == '?'|| c == ';'){
                text[i] = c;
                break;
            }
            text[i] = c;
        }

        text[i+1] = 0;

        _flite_sigblockusr(&some_signals);        
        wave = flite_text_to_wave(text, flite_voice);
        spd_audio_play_wave(wave);
        _flite_sigunblockusr(&some_signals);

        if (terminate == 1){
            fclose(sp_file);
            free(text);
            spd_audio_close();
            exit(0);
        }
    }
}

void
_flite_sigpause()
{
    flite_pause_requested = 1;
    if(DEBUG_FLITE) printf("flite: received signal for pause\n");
}
