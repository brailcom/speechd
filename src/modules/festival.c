
/*
 * festival.c - Speech Dispatcher backend for Festival
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
 * $Id: festival.c,v 1.34 2003-10-09 21:16:58 hanke Exp $
 */

#include "module.h"
#include "fdset.h"

#include "module_utils.c"
#include "module_utils_audio.c"

#include "festival_client.c"

#define MODULE_NAME     "festival"
#define MODULE_VERSION  "0.1"

#define DEBUG_MODULE 1
DECLARE_DEBUG();

/* Thread and process control */
static pthread_t festival_speak_thread;
static pid_t festival_pid = 0;
static sem_t *festival_semaphore;
static int festival_speaking = 0;
static int festival_pause_requested = 0;

static char **festival_message;
static EMessageType festival_message_type;
static int festival_position = 0;

FT_Info *festival_info;

/* Internal functions prototypes */
void* _festival_speak(void*);

size_t _festival_parent(TModuleDoublePipe dpipe, const char* message,
                        EMessageType msgtype,
                        const size_t maxlen, const char* dividers,
                        int *pause_requested);

void festival_parent_clean();

cst_wave* festival_conv(FT_Wave *fwave);

void festival_set_rate(signed int rate);
void festival_set_pitch(signed int pitch);
void festival_set_voice(EVoiceType voice);
void festival_set_language(char* language);
void festival_set_punctuation_mode(EPunctMode punct);


MOD_OPTION_1_INT(FestivalMaxChunkLength);
MOD_OPTION_1_STR(FestivalDelimiters);
MOD_OPTION_1_STR(FestivalServerHost);
MOD_OPTION_1_STR(FestivalStripPunctChars);
MOD_OPTION_1_INT(FestivalServerPort);
MOD_OPTION_1_INT(FestivalPitchDeviation);
MOD_OPTION_1_INT(FestivalDebugSaveOutput);
MOD_OPTION_1_STR(FestivalRecodeFallback);

/* Public functions */

int
module_load(void)
{

    INIT_SETTINGS_TABLES();

    REGISTER_DEBUG();

    MOD_OPTION_1_INT_REG(FestivalMaxChunkLength, 150);
    MOD_OPTION_1_STR_REG(FestivalDelimiters, ".");

    MOD_OPTION_1_STR_REG(FestivalServerHost, "localhost");
    MOD_OPTION_1_INT_REG(FestivalServerPort, 1314);

    MOD_OPTION_1_INT_REG(FestivalDebugSaveOutput, 0);

    MOD_OPTION_1_STR_REG(FestivalStripPunctChars, "");

    MOD_OPTION_1_STR_REG(FestivalRecodeFallback, "?");

    return 0;
}

int
module_init(void)
{
    int ret;

    INIT_DEBUG();

    DBG("module_init()");

    /* Init festival and register a new voice */
    festival_info = festivalDefaultInfo();
    festival_info->server_host = FestivalServerHost;
    festival_info->server_port = FestivalServerPort;

    festival_info = festivalOpen(festival_info);
    if (festival_info == NULL) return -1;

    DBG("FestivalServerHost = %s\n", FestivalServerHost);
    DBG("FestivalServerPort = %d\n", FestivalServerPort);
    DBG("FestivalMaxChunkLength = %d\n", FestivalMaxChunkLength);
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

int
module_speak(char *data, size_t bytes, EMessageType msgtype)
{
    int ret;

    DBG("module_write()\n");

    if (data == NULL) return -1;

    if (festival_speaking){
        DBG("Speaking when requested to write\n");
        return -1;
    }

    /* Check the parameters */
    if(module_write_data_ok(data) != 0) return -1;

    DBG("Requested data: |%s| (before recoding)\n", data);
    *festival_message = module_recode_to_iso(data, bytes, msg_settings.language, FestivalRecodeFallback);
    if (*festival_message == NULL){
        return -1;
    }
    DBG("Requested data after recoding: |%s|\n", *festival_message);
    if(msgtype == MSGTYPE_TEXT){
        module_strip_punctuation_some(*festival_message, FestivalStripPunctChars);
        DBG("Requested after stripping punct: |%s|\n", *festival_message);
    }else{
        DBG("Non-textual event: |%s|\n", *festival_message);
    }

    festival_message_type = msgtype;
    if ((msgtype == MSGTYPE_TEXT) && (msg_settings.spelling_mode == SPELLING_ON))
        festival_message_type = MSGTYPE_SPELL;

    /* If the connection crashed or language or voice
     change, we will need to set all the parameters again */
    if (festival_connection_crashed){
        CLEAN_OLD_SETTINGS_TABLE();
        festival_connection_crashed = 0;
    }
    if ((msg_settings.voice != msg_settings_old.voice) 
        || strcmp(msg_settings.language, msg_settings_old.language)){
        CLEAN_OLD_SETTINGS_TABLE();
    }

    /* Setting voice parameters */
    UPDATE_STRING_PARAMETER(language, festival_set_language);
    UPDATE_PARAMETER(voice, festival_set_voice);
    UPDATE_PARAMETER(rate, festival_set_rate);
    UPDATE_PARAMETER(pitch, festival_set_pitch);
    UPDATE_PARAMETER(punctuation_mode, festival_set_punctuation_mode);

    /* Send semaphore signal to the speaking thread */
    festival_speaking = 1;    
    sem_post(festival_semaphore);    
		
    DBG("Festival: leaving write() normaly\n\r");
    return bytes;
}

int
module_stop(void) 
{
    DBG("stop()\n");

    if(festival_speaking && (festival_pid != 0)){
        DBG("stopping process pid %d\n", festival_pid);
        kill(festival_pid, SIGKILL);
    }
}

size_t
module_pause(void)
{
    DBG("pause requested\n");
    if(festival_speaking){

        DBG("Sending request to pause to child\n");
        festival_pause_requested = 1;
        DBG("Waiting in pause for child to terminate\n");
        while(festival_speaking) usleep(10);

        DBG("paused at byte: %d", current_index_mark);
        return current_index_mark;        
    }else{
        return -1;
    }
}

int
module_is_speaking(void)
{
    return festival_speaking; 
}

void
module_close(int status)
{
    
    DBG("festival: close()\n");

    while(festival_speaking){
        module_stop();
        usleep(5);
    }

    DBG("festivalClose()");
    festivalClose(festival_info);
    
    if (module_terminate_thread(festival_speak_thread) != 0)
        exit(1);

    delete_FT_Info(festival_info);

    DBG("Removing junk files in tmp/");
    system("rm -f /tmp/est* 2> /dev/null");

    DBG("Closing debug file");
    CLOSE_DEBUG_FILE();

    exit(status);
}


/* Internal functions */


void*
_festival_speak(void* nothing)
{	

    DBG("festival: speaking thread starting.......\n");

    module_speak_thread_wfork(festival_semaphore, &festival_pid,
                              module_audio_output_child, _festival_parent, 
                              &festival_speaking, festival_message, &festival_message_type,
                              (size_t) FestivalMaxChunkLength, FestivalDelimiters,
                              &festival_position, &festival_pause_requested);
    festival_speaking = 0;

    DBG("festival: speaking thread ended.......\n");    

    pthread_exit(NULL);
}	

size_t
_festival_parent(TModuleDoublePipe dpipe, const char* message,
              EMessageType msgtype, const size_t maxlen, const char* dividers,
              int *pause_requested)
{
    int pos = 0;
    int i;
    int ret;
    int bytes;
    size_t read_bytes;
    int child_terminated = 0;
    FT_Wave *fwave;
    char buf[4096];
    int terminate = 0;
    int first_run = 1;
    int r = -999;
    static int debug_count = 0;
    int o_bytes = 0;
    int pause = -1;
    cst_wave wave_parameters;
    int cont = 0;

    DBG("Entering parent process, closing pipes");

    module_parent_dp_init(dpipe);

    pos = 0;
    while(1){
        DBG("  Looping...\n");
        o_bytes = bytes;

        if (msgtype == MSGTYPE_TEXT)
            bytes = module_get_message_part(message, buf, &pos, 4095, dividers);
        else{
            bytes = module_get_message_part(message, buf, &pos, 4095, "");
        }
        if (current_index_mark != -1) pause = current_index_mark;
        if (bytes == 0){
            if (msgtype == MSGTYPE_TEXT)
                bytes = module_get_message_part(message, buf, &pos, 4095, dividers);
            else
                return -1;
        }
        
        DBG("returned %d bytes\n", bytes); 

        assert( ! ((bytes > 0) && (buf == NULL)) );

        if (first_run){
            DBG("Synthesizing: |%s|", buf);
            if (bytes > 0){
                switch(msgtype)
                    {
                    case MSGTYPE_TEXT: r = festivalStringToWaveRequest(festival_info, buf); break;
                    case MSGTYPE_SOUND_ICON: r = festivalSoundIcon(festival_info, buf); break;
                    case MSGTYPE_CHAR: r = festivalCharacter(festival_info, buf); break;
                    case MSGTYPE_KEY: r = festivalKey(festival_info, buf); break;
                    case MSGTYPE_SPELL: r = festivalSpell(festival_info, buf); break;
                    }
            }
            DBG("s-returned %d\n", r);
            first_run = 0;
        }else{                                       
            DBG("Retrieving data\n");
            if (o_bytes > 0) fwave = festivalStringToWaveGetData(festival_info, &cont, 0);
            else fwave = NULL;

            DBG("ss-returned %d\n", r);
            DBG("Ok, next step...\n");

            /* This is not synchronous here, but there is no other way to do it */
            DBG("Sending request for synthesis");
            if (bytes > 0){
                r = festivalStringToWaveRequest(festival_info, buf);
            }                

            /* fwave can be NULL if the given text didn't produce any sound
               output, e.g. this text: "." */
            if (fwave != NULL){
                DBG("Sending buf to child in wav: %d samples\n",
                    (fwave->num_samples) * sizeof(short));
                if (fwave->num_samples != 0){
                    ret = module_parent_send_samples(dpipe, fwave->samples,
                                                     fwave->num_samples, fwave->sample_rate);
                    if (ret == -1) terminate = 1;
                    if(FestivalDebugSaveOutput){
                        char filename_debug[256];
                        sprintf(filename_debug, "/tmp/debug-festival-%d.snd", debug_count++);
                        save_FT_Wave_snd(fwave, filename_debug);
                    }
                }
                delete_FT_Wave(fwave);
                DBG("parent: Sent %d bytes\n", ret);            
                ret = module_parent_dp_write(dpipe, "\r\nOK_SPEECHD_DATA_SENT\r\n", 24);
                if (ret == -1) terminate = 1;
                
                if (terminate != 1) terminate = module_parent_wait_continue(dpipe);
            }            
        }
        if ((pause >= 0) && (*pause_requested)){
            if (bytes > 0){
                fwave = festivalStringToWaveGetData(festival_info, &cont, 0);    
                delete_FT_Wave(fwave);
            }
            module_parent_dp_close(dpipe);
            *pause_requested = 0;
            current_index_mark = pause;        
            DBG("Pause requested in parent, index mark %d\n", current_index_mark);
            return current_index_mark;
        }

        if (terminate && (bytes > 0)){
            if (o_bytes > 0){
                fwave = festivalStringToWaveGetData(festival_info, &cont, 0);    
                delete_FT_Wave(fwave);
            }
        }

        if (terminate || (bytes == -1)){
            DBG("End of data in parent, closing pipes [%d:%d]", terminate, bytes);
            module_parent_dp_close(dpipe);
            break;
        }
            
    }    


}

void
festival_set_language(char* language)
{
    FestivalSetLanguage(festival_info, language);
}

void
festival_set_voice(EVoiceType voice)
{
    char* voice_name;

    voice_name = EVoice2str(voice);
    FestivalSetVoice(festival_info, voice_name);
    xfree(voice_name);

    /* Because the new voice can use diferent bitrate */
    DBG("Filling new sample wave\n");
}

void
festival_set_rate(signed int rate)
{
    FestivalSetRate(festival_info, rate);
}

void
festival_set_pitch(signed int pitch)
{
    FestivalSetPitch(festival_info, pitch);
}

void
festival_set_punctuation_mode(EPunctMode punct)
{
    char *punct_mode;
    punct_mode = EPunctMode2str(punct);
    FestivalSetPunctuationMode(festival_info, punct_mode);
    xfree(punct_mode);
}

/* Warning: In order to be faster, this function doesn't copy
 the samples from cst_wave to fwave, so be sure to free the
 fwave only, not fwave->samples after conversion! */
cst_wave*
festival_conv(FT_Wave *fwave)
{
    cst_wave *ret;

    if (fwave == NULL) return NULL;

    ret = (cst_wave*) xmalloc(sizeof(cst_wave));
    ret->type = strdup("riff");
    ret->num_samples = fwave->num_samples;
    ret->sample_rate = fwave->sample_rate;
    ret->num_channels = 1;

    /* The data isn't copied, only the pointer! */
    ret->samples = fwave->samples;

    return ret;
}


#include "module_main.c"
