
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
 * $Id: flite.c,v 1.38 2004-02-16 15:33:19 hanke Exp $
 */


#include <flite/flite.h>

#include "fdset.h"

#include "module_utils.c"
#include "module_utils_audio.c"

#define MODULE_NAME     "flite"
#define MODULE_VERSION  "0.1"

#define DEBUG_MODULE 1
DECLARE_DEBUG();

/* Thread and process control */
static int flite_speaking = 0;

static pthread_t flite_speak_thread;
static pid_t flite_pid;
static sem_t *flite_semaphore;

static char **flite_message;
static EMessageType flite_message_type;

static int flite_position = 0;
static int flite_pause_requested = 0;

/* Internal functions prototypes */
static void flite_set_rate(signed int rate);
static void flite_set_pitch(signed int pitch);
static void flite_set_voice(EVoiceType voice);

static void* _flite_speak(void*);
static void _flite_child(TModuleDoublePipe dpipe, const size_t maxlen);
static void flite_child_close(TModuleDoublePipe dpipe);

/* Voice */
cst_voice *register_cmu_us_kal();
cst_voice *flite_voice;

/* Fill the module_info structure with pointers to this modules functions */
//DECLARE_MODINFO("flite", "Flite software synthesizer v. 1.2");

MOD_OPTION_1_INT(FliteMaxChunkLength);
MOD_OPTION_1_STR(FliteDelimiters);

/* Public functions */

int
module_load(void)
{  
   INIT_SETTINGS_TABLES();

   REGISTER_DEBUG();
   
   MOD_OPTION_1_INT_REG(FliteMaxChunkLength, 300);
   MOD_OPTION_1_STR_REG(FliteDelimiters, ".");

   return 0;
}

int
module_init(void)
{
    int ret;

   INIT_DEBUG();

    /* Init flite and register a new voice */
    flite_init();
    flite_voice = register_cmu_us_kal();

    if (flite_voice == NULL){
        DBG("Couldn't register the basic kal voice.\n");
        return -1;
    }

    DBG("FliteMaxChunkLength = %d\n", FliteMaxChunkLength);
    DBG("FliteDelimiters = %s\n", FliteDelimiters);
    
    flite_message = malloc (sizeof (char*));    
    flite_semaphore = module_semaphore_init();

    DBG("Flite: creating new thread for flite_speak\n");
    flite_speaking = 0;
    ret = pthread_create(&flite_speak_thread, NULL, _flite_speak, NULL);
    if(ret != 0){
        DBG("Flite: thread failed\n");
        return -1;
    }
								
    return 0;
}


int
module_speak(gchar *data, size_t bytes, EMessageType msgtype)
{
    int ret;

    DBG("write()\n");

    if (flite_speaking){
        DBG("Speaking when requested to write");
        return 0;
    }
    
    if(module_write_data_ok(data) != 0) return -1;

    *flite_message = strdup(data);
    flite_message_type = MSGTYPE_TEXT;
    module_strip_punctuation_default(*flite_message);

    DBG("Requested data: |%s|\n", data);
	
    /* Setting voice */
    UPDATE_PARAMETER(voice, flite_set_voice);
    UPDATE_PARAMETER(rate, flite_set_rate);
    UPDATE_PARAMETER(pitch, flite_set_pitch);

    /* Send semaphore signal to the speaking thread */
    flite_speaking = 1;    
    sem_post(flite_semaphore);    
		
    DBG("Flite: leaving write() normaly\n\r");
    return bytes;
}

int
module_stop(void) 
{
    DBG("flite: stop()\n");

    if(flite_speaking && (flite_pid != 0)){
        DBG("flite: killing process pid %d\n", flite_pid);
        kill(flite_pid, SIGKILL);
    }
    return 0;
}

size_t
module_pause(void)
{
    DBG("pause requested\n");
    if(flite_speaking){

        DBG("Sending request to pause to child\n");
        flite_pause_requested = 1;
        DBG("Waiting in pause for child to terminate\n");
        while(flite_speaking) usleep(10);

        DBG("paused at byte: %d", flite_position);
        return flite_position;
        
    }else{
        return 0;
    }
}

int
module_is_speaking(void)
{
    return flite_speaking; 
}

void
module_close(int status)
{
    
    DBG("flite: close()\n");

    if(flite_speaking){
        module_stop();
    }

    if (module_terminate_thread(flite_speak_thread) != 0)
        exit(1);
   
    xfree(flite_voice);
    
    CLOSE_DEBUG_FILE();

    exit(status);
}

/* Internal functions */


void*
_flite_speak(void* nothing)
{	

    DBG("flite: speaking thread starting.......\n");

    module_speak_thread_wfork(flite_semaphore, &flite_pid,
                              _flite_child, module_parent_wfork, 
                              &flite_speaking, flite_message, &flite_message_type,
                              FliteMaxChunkLength, FliteDelimiters,
                              &flite_position, &flite_pause_requested);
    flite_speaking = 0;

    DBG("flite: speaking thread ended.......\n");    

    pthread_exit(NULL);
}	

void
_flite_child(TModuleDoublePipe dpipe, const size_t maxlen)
{
    char *text;  
    cst_wave* wave;
    sigset_t some_signals;
    int bytes;

    sigfillset(&some_signals);
    module_sigunblockusr(&some_signals);

    module_child_dp_init(dpipe);

    text = malloc((maxlen + 1) * sizeof(char));

    DBG("Entering child loop\n");
    while(1){
        /* Read the waiting data */
        bytes = module_child_dp_read(dpipe, text, maxlen);
        DBG("read %d bytes in child", bytes);
        if (bytes == 0){
            free(text);
            flite_child_close(dpipe);
        }

        //        text[bytes] = '.';
        text[bytes] = 0;

        DBG("child: got data |%s|, bytes:%d", text, bytes);

        DBG("Speaking in child...");
        module_sigblockusr(&some_signals);        
        {
	    const char outtype[] = "play";
	    flite_text_to_speech(text, flite_voice, outtype);
	    /*            spd_audio_open(wave);
			  spd_audio_play_wave(wave);
			  spd_audio_close(); */
        }
        module_sigunblockusr(&some_signals);        

        DBG("child->parent: ok, send more data", text);      
        module_child_dp_write(dpipe, "C", 1);
    }
}


static void
flite_set_rate(signed int rate)
{
    float stretch = 1;

    assert(rate >= -100 && rate <= +100);
    if (rate < 0) stretch -= ((float) rate) / 50;
    if (rate > 0) stretch -= ((float) rate) / 200;
    feat_set_float(flite_voice->features, "duration_stretch",
                   stretch);
}

static void
flite_set_pitch(signed int pitch)
{
    float f0;

    assert(pitch >= -100 && pitch <= +100);
    f0 = ( ((float) pitch) * 0.8 ) + 100;
    feat_set_float(flite_voice->features, "int_f0_target_mean", f0);
}

static void
flite_set_voice(EVoiceType voice)
{
    if (voice == MALE1){
        free(flite_voice);
        flite_voice = (cst_voice*) register_cmu_us_kal();
    }
}

static void
flite_child_close(TModuleDoublePipe dpipe)
{   
    spd_audio_close();
    DBG("child: Pipe closed, exiting, closing pipes..\n");
    module_child_dp_close(dpipe);          
    DBG("Child ended...\n");
    exit(0);
}

#include "module_main.c"
