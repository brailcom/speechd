
/*
 * festival.c - Speech Dispatcher backend for Flite (Festival Lite)
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
 * $Id: festival.c,v 1.18 2003-06-27 13:43:10 hanke Exp $
 */

#include "festival_client.c"

#include "module.h"
#include "fdset.h"

#include "module_utils.c"

#define MODULE_NAME     "festival"
#define MODULE_VERSION  "0.1"

#define DEBUG_MODULE 1
DECLARE_DEBUG_FILE("/tmp/debug-festival");

/* Thread and process control */
static pthread_t festival_speak_thread;
static pid_t festival_pid = 0;
static sem_t *festival_semaphore;
static int festival_speaking = 0;
static int festival_pause_requested = 0;

static EVoiceType festival_cur_voice = MALE1;
static int festival_cur_rate;
static int festival_cur_pitch;
static char* festival_cur_language = NULL;

static char **festival_message;
static int festival_position = 0;

FT_Info *festival_info;

/* Public function prototypes */
DECLARE_MODULE_PROTOTYPES();

/* Internal functions prototypes */
static void* _festival_speak(void*);
static void festival_child_close(TModuleDoublePipe dpipe);

static size_t _festival_parent(TModuleDoublePipe dpipe, const char* message,
                               const size_t maxlen, const char* dividers,
                               int *pause_requested);

static void festival_parent_clean();

static cst_wave* festival_conv(FT_Wave *fwave);
static void festival_fill_sample_wave();

static void festival_set_rate(signed int rate);
static void festival_set_pitch(signed int pitch);
static void festival_set_voice(EVoiceType voice, char* language);


/* Fill the module_info structure with pointers to this modules functions */
DECLARE_MODINFO("festival", "Festival software synthesizer");

MOD_OPTION_1_INT(FestivalMaxChunkLenght);
MOD_OPTION_1_STR(FestivalDelimiters);
MOD_OPTION_1_STR(FestivalServerHost);
MOD_OPTION_1_INT(FestivalServerPort);
MOD_OPTION_1_INT(FestivalPitchDeviation);

/* Public functions */

OutputModule*
module_load(configoption_t **options, int *num_options)
{
    INIT_DEBUG_FILE();

    DBG("module_load()\n");

    MOD_OPTION_1_INT_REG(FestivalMaxChunkLenght, 150);
    MOD_OPTION_1_STR_REG(FestivalDelimiters, ".");

    MOD_OPTION_1_STR_REG(FestivalServerHost, "localhost");
    MOD_OPTION_1_INT_REG(FestivalServerPort, 1314);

    MOD_OPTION_1_INT_REG(FestivalPitchDeviation, 14);

    module_register_settings_voices(&module_info);

    DBG("ok, loaded...\n");
    return &module_info;
}

int
module_init(void)
{
    int ret;

    DBG("module_init()");

    /* Init festival and register a new voice */
    festival_info = festivalDefaultInfo();
    festival_info->server_host = FestivalServerHost;
    festival_info->server_port = FestivalServerPort;

    festival_info = festivalOpen(festival_info);
    if (festival_info == NULL) return -1;

    festival_fill_sample_wave();

    festival_cur_language = strdup("en");
    festival_cur_voice = NO_VOICE;
    festival_cur_rate = 0;
    festival_cur_pitch = 0;

    DBG("FestivalServerHost = %s\n", FestivalServerHost);
    DBG("FestivalServerPort = %d\n", FestivalServerPort);
    DBG("FestivalMaxChunkLenght = %d\n", FestivalMaxChunkLenght);
    DBG("FestivalDelimiters = %s\n", FestivalDelimiters);
    
    festival_message = (char**) xmalloc (sizeof (char*));    
    festival_semaphore = module_semaphore_init();
    if (festival_semaphore == NULL) return -1;

    DBG("Festival: creating new thread for festival_speak\n");
    festival_speaking = 0;
    ret = pthread_create(&festival_speak_thread, NULL, _festival_speak, NULL);
    if(ret != 0){
        DBG("Festival: thread failed\n");
        return -1;
    }
								
    return 0;
}

static gint
module_write(gchar *data, size_t bytes, TFDSetElement* set)
{
    int ret;

    DBG("module_write()\n");

    assert(set != NULL);
    assert(data != NULL);

    if (festival_speaking){
        DBG("Speaking when requested to write\n");
        return 0;
    }

    /* Check the parameters */
    if(module_write_data_ok(data) != 0) return -1;

    DBG("Requested data: |%s| (before recoding)\n", data);
    *festival_message = module_recode_to_iso(data, bytes, set->language);
    if (*festival_message == NULL){
        return -1;
    }
    module_strip_punctuation_default(*festival_message);

    /* Setting voice */
    festival_set_voice(set->voice_type, set->language);
    festival_set_rate(set->rate);
    festival_set_pitch(set->pitch);

    /* Send semaphore signal to the speaking thread */
    festival_speaking = 1;    
    sem_post(festival_semaphore);    
		
    DBG("Festival: leaving write() normaly\n\r");
    return bytes;
}

static gint
module_stop(void) 
{
    DBG("stop()\n");

    if(festival_speaking && (festival_pid != 0)){
        DBG("stopping process pid %d\n", festival_pid);
        kill(festival_pid, SIGKILL);
    }
}

static size_t
module_pause(void)
{
    DBG("pause requested\n");
    if(festival_speaking){

        DBG("Sending request to pause to child\n");
        festival_pause_requested = 1;
        DBG("Waiting in pause for child to terminate\n");
        while(festival_speaking) usleep(10);

        DBG("paused at byte: %d", festival_position);
        return festival_position;
        
    }else{
        return 0;
    }
}

gint
module_is_speaking(void)
{
    return festival_speaking; 
}

gint
module_close(void)
{
    
    DBG("festival: close()\n");

    while(festival_speaking){
        module_stop();
    }

    if (module_terminate_thread(festival_speak_thread) != 0)
        return -1;

    xfree(festival_cur_language);

    delete_FT_Info(festival_info);

    system("rm /tmp/est* 2> /dev/null");

    CLOSE_DEBUG_FILE();

    return 0;
}


/* Internal functions */


void*
_festival_speak(void* nothing)
{	

    DBG("festival: speaking thread starting.......\n");

    module_speak_thread_wfork(festival_semaphore, &festival_pid,
                              module_audio_output_child, _festival_parent, 
                              &festival_speaking, festival_message,
                              (size_t) FestivalMaxChunkLenght, FestivalDelimiters,
                              &festival_position, &festival_pause_requested);
    festival_speaking = 0;

    DBG("festival: speaking thread ended.......\n");    

    pthread_exit(NULL);
}	

static size_t
_festival_parent(TModuleDoublePipe dpipe, const char* message,
              const size_t maxlen, const char* dividers,
              int *pause_requested)
{
    int pos = 0;
    int i;
    int ret;
    size_t bytes;
    size_t read_bytes;
    int child_terminated = 0;
    FT_Wave *fwave;
    char buf[4096];
    int terminate = 0;
    int first_run = 1;

    DBG("Entering parent process, closing pipes");

    module_parent_dp_init(dpipe);

    pos = 0;
    while(1){
        DBG("  Looping...\n");

        bytes = module_get_message_part(message, buf, &pos, maxlen, dividers);
        DBG("returned %d bytes\n", bytes); 

        assert( ! ((bytes > 0) && (buf == NULL)) );

        if (first_run){
            DBG("Synthesizing: |%s|", buf);
            if (bytes != 0) festivalStringToWaveRequest(festival_info, buf);
            first_run = 0;
        }else{
            fwave = festivalStringToWaveGetData(festival_info);        
            if (bytes != 0) festivalStringToWaveRequest(festival_info, buf);

            /* fwave can be NULL if the given text didn't produce any sound
               output, e.g. this text: "." */
            if (fwave != NULL){
                DBG("Sending buf to child in wav: %d samples\n", (fwave->num_samples) * sizeof(short));
            
                ret = module_parent_send_samples(dpipe, fwave->samples, fwave->num_samples);
                delete_FT_Wave(fwave);
                if (ret == -1) terminate = 1;

                DBG("parent: Sent %d bytes\n", ret);            
                ret = module_parent_dp_write(dpipe, "\r\n.\r\n", 5);
                if (ret == -1) terminate = 1;
                
                if (terminate != 1) terminate = module_parent_wait_continue(dpipe);
            }
        }
        if (*pause_requested && bytes != 0){
            fwave = festivalStringToWaveGetData(festival_info);    
            delete_FT_Wave(fwave);
            DBG("Pause requested in parent, position %d\n", pos);
            module_parent_dp_close(dpipe);
            *pause_requested = 0;
            return pos-bytes;
        }

        if (terminate && (bytes != 0)){
            fwave = festivalStringToWaveGetData(festival_info);    
            delete_FT_Wave(fwave);
        }

        if (terminate || (bytes == 0)){
            DBG("End of data in parent, closing pipes");
            module_parent_dp_close(dpipe);
            break;
        }
            
    }    
}

static void
festival_set_voice(EVoiceType voice, char *language)
{
    char* voice_name;

    if (voice != festival_cur_voice || strcmp(language, festival_cur_language)){
        voice_name = module_getvoice(&module_info, language, voice);
        if (voice_name == NULL){
            DBG("No voice available\n");
            return;
        }
        festivalSetVoice(festival_info, voice_name);

        festival_cur_voice = voice;

        xfree(festival_cur_language);
        festival_cur_language = strdup(language);

        festival_cur_rate = 0;
        festival_cur_pitch = 0;

        /* Because the new voice can use diferent bitrate */
        DBG("Filling new sample wave\n");
        festival_fill_sample_wave();
    }
}

static void
festival_set_rate(signed int rate)
{
    assert(rate >= -100 && rate <= +100);
    if (festival_cur_rate != rate){
        festivalSetRate(festival_info, rate);
        festival_cur_rate = rate;
    }
}

static void
festival_set_pitch(signed int pitch)
{
    assert(pitch >= -100 && pitch <= +100);
    festivalSetPitch(festival_info, pitch, FestivalPitchDeviation);
    return;
}

static void
festival_fill_sample_wave()
{
    FT_Wave *fwave;
    int ret;

    module_cstwave_free(module_sample_wave);

    /* Synthesize a sample string*/
    ret = festivalStringToWaveRequest(festival_info, "test");    
    if (ret != 0){
        DBG("Festival module request to synthesis failed.\n");
        module_sample_wave = NULL;
        return;
    }

    /* Get it back from server */
    fwave = (FT_Wave*) festivalStringToWaveGetData(festival_info);
    if (fwave == NULL){
        DBG("Sample wave is NULL, this voice doesn't work properly in Festival!\n");
        module_sample_wave = NULL;
        return;
    }

    DBG("Synthesized wave num samples = %d\n", fwave->num_samples);

    /* Save it as cst_wave */
    module_sample_wave = (cst_wave*) festival_conv(fwave);
    /* Free fwave structure, but not the samples, because we
       point to them from module_sample_wave! */
    xfree(fwave);
}

static void
festival_child_close(TModuleDoublePipe dpipe)
{   
    spd_audio_close();

    DBG("child: Parent pipe closed, exiting, closing pipes..\n");
    module_child_dp_close(dpipe);          

    DBG("Child ended...\n");
    exit(0);
}

/* Warning: In order to be faster, this function doesn't copy
 the samples from cst_wave to fwave, so be sure to free the
 fwave only, not fwave->samples after conversion! */
static cst_wave*
festival_conv(FT_Wave *fwave)
{
    cst_wave *ret;

    if (fwave == NULL) return NULL;

    ret = (cst_wave*) spd_malloc(sizeof(cst_wave));
    ret->type = strdup("riff");
    ret->num_samples = fwave->num_samples;
    ret->sample_rate = fwave->sample_rate;
    ret->num_channels = 1;

    /* The data isn't copied, only the pointer! */
    ret->samples = fwave->samples;

    return ret;
}
