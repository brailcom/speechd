
/*
 * festival.c - Speech Deamon backend for Festival
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
 * $Id: festival.c,v 1.11 2003-05-23 08:54:50 pdm Exp $
 */

#define VERSION "0.1"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>

#include "spd_audio.h"

#include "module.h"
#include "fdset.h"

#include "festival_client.c"

static const int DEBUG_FESTIVAL = 0;

/* TODO: Cleaning, comments! */

/* Thread and process control */
static int festival_speaking = 0;
static int festival_running = 0;
static int waiting_data = 0;

static EVoiceType festival_cur_voice = NO_VOICE;
static signed int festival_cur_rate = 0;
static char *festival_cur_language;

static pthread_t festival_speak_thread;
static pid_t festival_pid;

/* Public function prototypes */
gint	festival_write			(gchar *data, gint len, void*);
gint	festival_stop			(void);
gchar*  festival_pause                  (void);
gint	festival_is_speaking	        (void);
gint	festival_close			(void);

FT_Info *festival_info;

/* Internal functions prototypes */
static void* _festival_speak(void*);
static void _festival_synth(void);
static void _festival_sigpause();
static char* _festival_recode(char *data, char *language);
static cst_wave* festival_conv(FT_Wave *fwave);
static int festival_pause_requested = 0;

static cst_wave *festival_test_wave;

/* Fill the module_info structure with pointers to this modules functions */
OutputModule modinfo_festival = {
   "festival",                     /* name */
   "Software synthetizer Festival", /* description */
   NULL,                        /* GModule (should be set to NULL)*/
   festival_write,                 /* module functions */
   festival_stop,
   festival_pause,
   festival_is_speaking,
   festival_close,
   {0,0}
};

static char*
module_getparam_str(GHashTable *table, char* param_name)
{
    char *param;
    param = g_hash_table_lookup(table, param_name);
    return param;
}

static int
module_getparam_int(GHashTable *table, char* param_name)
{
    char *param_str;
    int param;
    
    param_str = module_getparam_str(table, param_name);
    if (param_str == NULL) return -1;
    
    {
      char *tailptr;
      param = strtol(param_str, &tailptr, 0);
      if (tailptr == param_str) return -1;
    }
    
    return param;
}

static char*
module_getvoice(GHashTable *table, char* language, EVoiceType voice)
{
    SPDVoiceDef *voices;
    char *ret;

    voices = g_hash_table_lookup(table, language);
    if (voices == NULL) return NULL;

    switch(voice){
    case MALE1: 
        ret = voices->male1; break;
    case MALE2: 
        ret = voices->male2; break;
    case MALE3: 
        ret = voices->male3; break;
    case FEMALE1: 
        ret = voices->female1; break;
    case FEMALE2: 
        ret = voices->female2; break;
    case FEMALE3: 
        ret = voices->female3; break;
    case CHILD_MALE: 
        ret = voices->child_male; break;
    case CHILD_FEMALE: 
        ret = voices->child_female; break;
    default:
        FATAL("Unknown voice");
    }

    if (ret == NULL) ret = voices->male1;
    if (ret == NULL) printf("No voice available for this output module!");

    return ret;
}

/* Entry point of this module */
OutputModule*
module_load()
{
    if(DEBUG_FESTIVAL) printf("festival: load_module()\n");
    modinfo_festival.description =
        g_strdup_printf("Festival software synthesizer, version %s",VERSION);

    assert(modinfo_festival.name != NULL);
    assert(modinfo_festival.description != NULL);

    modinfo_festival.settings.params = g_hash_table_new(g_str_hash, g_str_equal);
    modinfo_festival.settings.voices = g_hash_table_new(g_str_hash, g_str_equal);

    return &modinfo_festival;
}

int
module_init(void)
{
    FT_Wave *fwave;
    char *host;
    int port;

    if(DEBUG_FESTIVAL) printf("festival: init_module()\n");

    /* Init festival and register a new voice */
 
    festival_info = festival_default_info();

    host = module_getparam_str(modinfo_festival.settings.params, "host");
    if (host != NULL){
        festival_info->server_host = malloc(strlen(host) * sizeof(char) + 1);
        strcpy(festival_info->server_host, host);
    }
    port = module_getparam_int(modinfo_festival.settings.params, "port");
    if (port != -1) festival_info->server_port = port;

    festival_info = festivalOpen(festival_info);
    if (festival_info == NULL) return -1;

    festivalStringToWaveRequest(festival_info, "test");
    fwave = (FT_Wave*) festivalStringToWaveGetData(festival_info);
    if (DEBUG_FESTIVAL) printf("(init) wave num samples = %d\n", fwave->num_samples);
    festival_test_wave = (cst_wave*) festival_conv(fwave);

    festival_cur_language = malloc(16);
    strcpy(festival_cur_language, "en");

    return 0;
}

/* Public functions */

gint
festival_write(gchar *data, gint len, void* v_set)
{
    int ret;
    FILE *temp;
    float stretch;
    TFDSetElement *set = v_set;
    char *voice;
    char *text;

    /* Tests */
    if(DEBUG_FESTIVAL) printf("festival: write()\n");

    if(data == NULL){
        if(DEBUG_FESTIVAL) printf("festival: requested data NULL\n");		
        return -1;
    }

    if(data == ""){
        if(DEBUG_FESTIVAL) printf("festival: requested data empty\n");
        return -1;
    }

    if(DEBUG_FESTIVAL) printf("requested data: |%s|\n", data);

    if (festival_speaking){
        if(DEBUG_FESTIVAL) printf("festival: speaking when requested to festival-write\n");
        return 0;
    }

    /* Convert to the appropriate character set */
    text = _festival_recode(data, set->language);	
    if (text == NULL) return -1;

    /* Preparing data */
    if((temp = fopen("/tmp/festival_message", "w")) == NULL){
        printf("Festival: couldn't open temporary file\n");
        return 0;
    }
    /* This space is important here! */
    fprintf(temp,"%s \n\r",text);
    fclose(temp);
    fflush(NULL);

    /* Setting voice */
    festivalEmptySocket(festival_info);
    if (set->voice_type != festival_cur_voice || strcmp(set->language, festival_cur_language)){
        voice = module_getvoice(modinfo_festival.settings.voices, set->language,
                                set->voice_type);
        if (voice == NULL){
            if (DEBUG_FESTIVAL) printf("festival: No voice available\n");
            return 0;
        }
        festivalSetVoice(festival_info, voice);

        festival_cur_voice = set->voice_type;
        strcpy(festival_cur_language, set->language);
    }
    
    /* Setting rate */
    if (festival_cur_rate != set->rate){
        festivalSetRate(festival_info, set->rate);
        festival_cur_rate = set->rate;
    }

    /* Running Festival */
    if(DEBUG_FESTIVAL) printf("Festival: creating new thread for festival_speak\n");
    festival_speaking = 1;
    festival_running = 1;
    ret = pthread_create(&festival_speak_thread, NULL, _festival_speak, NULL);
    if(ret != 0){
        if(DEBUG_FESTIVAL) printf("Festival: thread failed\n");
        festival_speaking = 0;
        festival_running = 0;
        return 0;
    }
		
    if(DEBUG_FESTIVAL) printf("Festival: leaving write() normaly\n\r");
    return len;
}

gint
festival_stop(void) 
{
    if(DEBUG_FESTIVAL) printf("festival: stop()\n");

    if(festival_running){
        if(DEBUG_FESTIVAL) printf("festival: stopping process pid %d\n", festival_pid);
        kill(festival_pid, SIGKILL);
        /* Make sure there is no pending message left */
        festivalEmptySocket(festival_info);
    }
}

gchar*
festival_pause(void)
{
    GString *pause_text;
    char* text;
    char *ret;
    FILE* temp;
    
    text = malloc(1024 * sizeof(char));

    if(festival_speaking){
        if(kill(festival_pid, SIGUSR1)) printf("festival: error in kill\n");;
        while(festival_running) usleep(10);

        if((temp = fopen("/tmp/festival_message", "r")) == NULL){
            printf("Festival: couldn't open temporary file\n");
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
    printf("festival not running");
    return NULL;
}

gint
festival_is_speaking(void)
{
    int ret;

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

gint
festival_close(void)
{
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

    festivalClose(festival_info);
   
    return 0;
}

/* Internal functions */
static void*
_festival_speak(void* nothing)
{	
    int ret;
    sigset_t all_signals;	
    int status;

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
        fflush(NULL);
        exit(0);

    case 0:
        /* This is the child. Make festival speak, but exit on SIGINT. */

        if(signal(SIGUSR1, _festival_sigpause) == SIG_ERR) printf("signal error");
        _festival_synth();

    default:
        /* This is the parent. Wait for the child to terminate. */
        waitpid(festival_pid,&status, 0);
    }

    if(DEBUG_FESTIVAL) printf("festival ended....%d.%d..\n", WIFEXITED(status),
                              WIFSIGNALED(status));
	
    festival_running = 0;
    pthread_exit(NULL);
}	

static void
_festival_sigunblockusr(sigset_t *some_signals)
{
    int ret;

    sigdelset(some_signals, SIGUSR1);
    ret = sigprocmask(SIG_SETMASK, some_signals, NULL);
    if ((ret != 0)&&(DEBUG_FESTIVAL))
        printf("festival: Can't block signal set, expect problems with terminating!\n");
}

static void
_festival_sigblockusr(sigset_t *some_signals)
{
    int ret;

    sigaddset(some_signals, SIGUSR1);
    ret = sigprocmask(SIG_SETMASK, some_signals, NULL);
    if ((ret != 0)&&(DEBUG_FESTIVAL))
        printf("festival: Can't block signal set, expect problems with terminating!\n");
}

static cst_wave*
festival_conv(FT_Wave *fwave)
{
    cst_wave *ret;

    if (fwave == NULL) return NULL;

    ret = (cst_wave*) spd_malloc(sizeof(cst_wave));
    ret->type = (char*) spd_malloc(8);
    sprintf((char*) ret->type, "riff");
    ret->num_samples = fwave->num_samples;
    ret->sample_rate = fwave->sample_rate;
    ret->num_channels = 1;
    ret->samples = fwave->samples;

    return ret;   
}

static void
_festival_synth()
{
    char *text;
    char *ret;
    FILE* sp_file;
    GString *festival_pause_text;
    cst_wave *wave;
    FT_Wave *fwave;
    sigset_t some_signals;	
    int i;
    char c;
    int terminate = 0;
    int first_chunk = 1;

    if (DEBUG_FESTIVAL) printf("festival: In synth");

    sigfillset(&some_signals);

    _festival_sigunblockusr(&some_signals);

    festivalEmptySocket(festival_info);

    text = malloc(1024 * sizeof(char));
    sp_file = fopen("/tmp/festival_message","r");

    if (sp_file == NULL){
        printf("festival: Can't open Festival temporary file\n");
        exit(0);
    }

    /* Read the remaining groups of bytes, synthesize them and send
       to sound output */
    while(1){
        if (festival_pause_requested == 1){          
            festival_pause_text = g_string_new("");

            festivalStringToWaveGetData(festival_info);

            festival_pause_text = g_string_append(festival_pause_text, text);

            while(1){
                ret = fgets(text, 1024, sp_file);

                if (ret == NULL){
                    fclose(sp_file);
                    if((sp_file = fopen("/tmp/festival_message", "w")) == NULL){
                        printf("festival: couldn't open temporary file\n");
                        return;
                    }
                    fprintf(sp_file,"%s",festival_pause_text->str);
                    if (DEBUG_FESTIVAL) printf("festival: paused text:|%s|", festival_pause_text->str);
                    fclose(sp_file);
                    if (DEBUG_FESTIVAL) printf("terminating festival run\n");
                    free(text);
                    exit(0);
                }
                festival_pause_text = g_string_append(festival_pause_text, text);
            }
        }

        for (i=0; i<=200; i++){
            c = fgetc(sp_file);
            if (c == EOF){
                text[i] = 0;
                terminate = 1;
                break;
            }
            /* We have to divide the text into small chunks to be able to
             start early */
            if (c == '.' || c == '!' || c == '?'|| c == ';' || c == '-' || c == ','){
                if (i==0){                    
                    text[0] = 0;
                }else{
                    text[i] = c;
                }
                break;
            }
            text[i] = c;
        }

        text[i+1] = 0;

        if (text[0] != 0){
            _festival_sigblockusr(&some_signals);        
            if (first_chunk){
                first_chunk = 0;
                festivalStringToWaveRequest(festival_info, text);
                spd_audio_open(festival_test_wave);
            }else{
                fwave = festivalStringToWaveGetData(festival_info);
                festivalStringToWaveRequest(festival_info, text);
                wave = festival_conv(fwave);
                if (wave != NULL) spd_audio_play_wave(wave);
            }
            _festival_sigunblockusr(&some_signals);
        }

        if (terminate == 1){

            if (text[0] != 0){                
                fwave = festivalStringToWaveGetData(festival_info);
                wave = festival_conv(fwave);
                if (wave!=NULL) spd_audio_play_wave(wave);
            }

            fclose(sp_file);
            free(text);
            spd_audio_close();
            if(DEBUG_FESTIVAL) printf("festival: exiting from festival");

            exit(0);
        }
    }
}

static void
_festival_sigpause()
{
    festival_pause_requested = 1;
    if(DEBUG_FESTIVAL) printf("festival: received signal for pause\n");
}

static char *
_festival_recode(char *data, char *language)
{
    char *recoded;
    
    if (!strcmp(language, "cs"))
        recoded = g_convert(data, strlen(data), "ISO8859-2", "UTF-8", NULL, NULL, NULL);
    else
        recoded = data;

    if (DEBUG_FESTIVAL && (recoded == NULL)) printf("festival: Conversion to ISO8859-2 failed\n");

    return recoded;
}
