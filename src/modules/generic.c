
/*
 * generic.c - Speech Dispatcher generic output module
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
 * $Id: generic.c,v 1.1 2003-08-11 14:59:24 hanke Exp $
 */

#include <glib.h>

#include "module.h"
#include "fdset.h"

#include "module_utils.c"

#define MODULE_NAME     "generic"
#define MODULE_VERSION  "0.1"

#define DEBUG_MODULE 1
DECLARE_DEBUG_FILE("/tmp/debug-generic");

/* Thread and process control */
static int generic_speaking = 0;
static EVoiceType generic_cur_voice = MALE1;

static pthread_t generic_speak_thread;
static pid_t generic_pid;
static sem_t *generic_semaphore;

static char **generic_message;

static int generic_position = 0;
static int generic_pause_requested = 0;

static char *execute_synth_str1;
static char *execute_synth_str2;

static int generic_msg_pitch;
static int generic_msg_rate;
static EVoiceType generic_msg_voice;
static char* generic_msg_language;

/* Public function prototypes */
DECLARE_MODULE_PROTOTYPES();

/* Internal functions prototypes */
static void* _generic_speak(void*);
static void _generic_child(TModuleDoublePipe dpipe, const size_t maxlen);
static void generic_child_close(TModuleDoublePipe dpipe);

/* Fill the module_info structure with pointers to this modules functions */
DECLARE_MODINFO("Epos", "Generic software synthesizer");

MOD_OPTION_1_STR(GenericExecuteSynth);
MOD_OPTION_1_INT(GenericMaxChunkLenght);
MOD_OPTION_1_STR(GenericDelimiters);

MOD_OPTION_1_INT(GenericRateAdd);
MOD_OPTION_1_FLOAT(GenericRateMultiply);
MOD_OPTION_1_INT(GenericPitchAdd);
MOD_OPTION_1_FLOAT(GenericPitchMultiply);

MOD_OPTION_3_HT(GenericLanguage, code, name, charset);

/* Public functions */

OutputModule*
module_load(configoption_t **options, int *num_options)
{
    INIT_DEBUG_FILE();

    MOD_OPTION_1_STR_REG(GenericExecuteSynth, "");

    MOD_OPTION_1_INT_REG(GenericMaxChunkLenght, 300);
    MOD_OPTION_1_STR_REG(GenericDelimiters, ".");

    MOD_OPTION_1_INT_REG(GenericRateAdd, 0);
    MOD_OPTION_1_FLOAT_REG(GenericRateMultiply, 1);
    MOD_OPTION_1_INT_REG(GenericPitchAdd, 0);
    MOD_OPTION_1_FLOAT_REG(GenericPitchMultiply, 1);

    MOD_OPTION_HT_REG(GenericLanguage);

    module_register_settings_voices(&module_info);

    DBG("module_load()\n");

    return &module_info;
}

int
module_init(void)
{
    int ret;

    DBG("GenericMaxChunkLenght = %d\n", GenericMaxChunkLenght);
    DBG("GenericDelimiters = %s\n", GenericDelimiters);
    DBG("GenericExecuteSynth = %s\n", GenericExecuteSynth);
    
    generic_message = malloc (sizeof (char*));    
    generic_semaphore = module_semaphore_init();

    DBG("Generic: creating new thread for generic_speak\n");
    generic_speaking = 0;
    ret = pthread_create(&generic_speak_thread, NULL, _generic_speak, NULL);
    if(ret != 0){
        DBG("Generic: thread failed\n");
        return -1;
    }
								
    return 0;
}


static gint
module_write(gchar *data, size_t bytes, TFDSetElement* set)
{
    int ret;
    TGenericLanguage *language;

    DBG("write()\n");

    if (generic_speaking){
        DBG("Speaking when requested to write");
        return 0;
    }
    
    if(module_write_data_ok(data) != 0) return -1;
    assert(set!=NULL);

    generic_msg_pitch = set->pitch;
    generic_msg_rate = set->rate;
    generic_msg_voice = set->voice_type;

    assert(set->language);
    generic_msg_language = strdup(set->language);

    /* Set the appropriate charset */
    language = (TGenericLanguage*) module_get_ht_option(GenericLanguage, set->language);
    if (language != NULL){
        if (language->charset != NULL){
            *generic_message = (char*) g_convert(data, bytes, language->charset, "UTF-8", NULL, NULL, NULL);
        }else{
            *generic_message = module_recode_to_iso(data, bytes, set->language);
        }
    }else{
        *generic_message = module_recode_to_iso(data, bytes, set->language);
    }
    module_strip_punctuation_default(*generic_message);

    DBG("Requested data: |%s|\n", data);
	
    /* Send semaphore signal to the speaking thread */
    generic_speaking = 1;    
    sem_post(generic_semaphore);    
		
    DBG("Generic: leaving write() normaly\n\r");
    return bytes;
}

static gint
module_stop(void) 
{
    DBG("generic: stop()\n");

    if(generic_speaking && generic_pid != 0){
        DBG("generic: stopping process group pid %d\n", generic_pid);
        kill(-generic_pid, SIGKILL);
    }
}

static size_t
module_pause(void)
{
    DBG("pause requested\n");
    if(generic_speaking){

        DBG("Sending request to pause to child\n");
        generic_pause_requested = 1;
        DBG("Waiting in pause for child to terminate\n");
        while(generic_speaking) usleep(10);

        DBG("paused at byte: %d", generic_position);
        return generic_position;
        
    }else{
        return 0;
    }
}

static gint
module_is_speaking(void)
{
    return generic_speaking; 
}

static gint
module_close(void)
{
    
    DBG("generic: close()\n");

    if(generic_speaking){
        module_stop();
    }

    if (module_terminate_thread(generic_speak_thread) != 0)
        return -1;
    
    CLOSE_DEBUG_FILE();

    return 0;
}



/* Internal functions */

char*
string_replace(char *string, char* token, char* data)
{
    char *p;
    char *str1;
    char *str2;
    int len;
    char *new;

    /* Split the string in two parts, ommit the token */
    p = strstr(string, token);
    if (p == NULL) return string;
    *p = 0;

    str1 = string;
    str2 = p + (strlen(token));        

    /* Put it together, replacing token with data */
    new = g_strdup_printf("%s%s%s", str1, data, str2);
    free(string);

    return new;
}

void*
_generic_speak(void* nothing)
{	
    TModuleDoublePipe module_pipe;
    int ret;
    int status;

    DBG("generic: speaking thread starting.......\n");

    set_speaking_thread_parameters();

    while(1){        
        sem_wait(generic_semaphore);
        DBG("Semaphore on\n");

        ret = pipe(module_pipe.pc);
        if (ret != 0){
            DBG("Can't create pipe pc\n");
            generic_speaking = 0;
            continue;
        }

        ret = pipe(module_pipe.cp);
        if (ret != 0){
            DBG("Can't create pipe cp\n");
            close(module_pipe.pc[0]);     close(module_pipe.pc[1]);
            generic_speaking = 0;
            continue;
        }

        /* Create a new process so that we could send it signals */
        generic_pid = fork();

        switch(generic_pid){
        case -1:	
            DBG("Can't say the message. fork() failed!\n");
            close(module_pipe.pc[0]);     close(module_pipe.pc[1]);
            close(module_pipe.cp[0]);     close(module_pipe.cp[1]);
            generic_speaking = 0;
            continue;

        case 0:
            {
            char *e_string;
            char str_pitch[16];
            char str_rate[16];
            char *p;
            char *voice_name;
            TGenericLanguage *language;

            snprintf(str_pitch,15,"%.2f",
                     ((float) generic_msg_pitch) * GenericPitchMultiply + GenericPitchAdd);
            snprintf(str_rate,15,"%.2f",
                     ((float) generic_msg_rate) * GenericRateMultiply + GenericRateAdd);
            language = (TGenericLanguage*) module_get_ht_option(GenericLanguage, generic_msg_language);
            voice_name = module_getvoice(&module_info, generic_msg_language, generic_msg_voice);

            e_string = strdup(GenericExecuteSynth);

            e_string = string_replace(e_string, "$PITCH", str_pitch);
            e_string = string_replace(e_string, "$RATE", str_rate);                       
            if (language != NULL){
                if (language->name != NULL){
                    e_string = string_replace(e_string, "$LANG", language->name);                       
                }else
                    e_string = string_replace(e_string, "$LANG", generic_msg_language);
            }else{
                e_string = string_replace(e_string, "$LANG", generic_msg_language);
            }

            if (voice_name != NULL){
                e_string = string_replace(e_string, "$VOICE", voice_name);                       
            }else{
                DBG("No voice available");
                exit (1);
            }

            /* Cut it into two strings */           
            p = strstr(e_string, "$DATA");
            if (p == NULL) exit(1);
            *p = 0;
            execute_synth_str1 = strdup(e_string);
            execute_synth_str2 = strdup(p + (strlen("$DATA")));

            free(e_string);

            /* execute_synth_str1 se sem musi nejak dostat */
            DBG("Starting child...\n");
            _generic_child(module_pipe, GenericMaxChunkLenght);
            }
            break;

        default:
            /* This is the parent. Send data to the child. */

            generic_position = module_parent_wfork(module_pipe, *generic_message,
                                                   GenericMaxChunkLenght, GenericDelimiters,
                                                   &generic_pause_requested);

            DBG("Waiting for child...");
            waitpid(generic_pid, &status, 0);            
            
            generic_speaking = 0;

            module_signal_end();

            DBG("child terminated -: status:%d signal?:%d signal number:%d.\n",
                WIFEXITED(status), WIFSIGNALED(status), WTERMSIG(status));
        }        
    }

    generic_speaking = 0;

    DBG("generic: speaking thread ended.......\n");    

    pthread_exit(NULL);
}	

void
_generic_child(TModuleDoublePipe dpipe, const size_t maxlen)
{
    char *text;  
    sigset_t some_signals;
    int bytes;
    char *command;
    GString *message;
    int i;

    sigfillset(&some_signals);
    module_sigunblockusr(&some_signals);

    /* Set this process as a process group leader (so that SIGKILL
     is also delivered to the child processes created by system()) */
    setpgrp();

    module_child_dp_init(dpipe);

    text = malloc((maxlen + 1) * sizeof(char));

    DBG("Entering child loop\n");
    while(1){
        /* Read the waiting data */
        bytes = module_child_dp_read(dpipe, text, maxlen);
        DBG("read %d bytes in child", bytes);
        if (bytes == 0){
            free(text);
            generic_child_close(dpipe);

        }

        text[bytes] = 0;

        /* Escape any quotes */
        message = g_string_new("");
        for(i=0; i<=strlen(text); i++){
            if (text[i] == '\"')
                message = g_string_append(message, "\\\"");
            else
                g_string_append_printf(message, "%c", text[i]);
        }

        DBG("child: got data |%s|", message->str);

        command = malloc((strlen(message->str)+strlen(execute_synth_str1)+
                          strlen(execute_synth_str2) + 1) * sizeof(char));

        sprintf(command, "%s%s%s", execute_synth_str1, message->str, execute_synth_str2);        

        DBG("child: synth command = |%s|", command);

        DBG("Speaking in child...");
        module_sigblockusr(&some_signals);        
        {
            system(command);
        }
        module_sigunblockusr(&some_signals);        

        free(command);
        free(text);
        g_string_free(message, 1);

        DBG("child->parent: ok, send more data", text);      
        module_child_dp_write(dpipe, "C", 1);
    }
}

static void
generic_child_close(TModuleDoublePipe dpipe)
{   
    DBG("child: Pipe closed, exiting, closing pipes..\n");
    module_child_dp_close(dpipe);          
    DBG("Child ended...\n");
    exit(0);
}
