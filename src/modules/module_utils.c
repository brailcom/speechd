/*
 * module_utils.c - Module utilities
 *           Functions to help writing output modules for Speech Dispatcher
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
 * $Id: module_utils.c,v 1.7 2003-07-10 16:11:55 pdm Exp $
 */

#include <semaphore.h>
#include <dotconf.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <errno.h>

#include "spd_audio.h"

static FILE *debug_file;

#define DEBUG_MODULE 1

#define MODULE_VERSION "0.0"

#define DECLARE_DEBUG_FILE(name) static char debug_filename[]=name;

#define INIT_DEBUG_FILE() \
    debug_file = fopen(debug_filename, "w"); \
    if (debug_file == NULL){ \
       printf("Can't open debug file!"); \
       debug_file = stdout; \
    }

#define CLOSE_DEBUG_FILE() \
    if (debug_file != NULL) fclose(debug_file);

#define DBG(arg...) if (DEBUG_MODULE){ \
    fprintf(debug_file, arg); \
    fflush(debug_file); \
  }

#define FATAL(msg) { \
     fprintf(stderr, "Fatal error in output module [%s:%d]:\n   "msg, \
             __FILE__, __LINE__); \
     exit(EXIT_FAILURE); \
   }

#define DECLARE_MODULE_PROTOTYPES() \
  static gint    module_write        (gchar *data, size_t bytes, TFDSetElement* settings); \
  static gint    module_stop         (void); \
  static size_t  module_pause        (void); \
  static gint    module_is_speaking  (void); \
  static gint    module_close        (void);

#define DECLARE_MODINFO(name, descr) \
static OutputModule module_info = { \
       name,                                 /* name */ \
       descr ", module v." MODULE_VERSION,   /* description */ \
       NULL,                                 /* GModule (should be set to NULL)*/ \
       module_write,                         /* module functions */ \
       module_stop, \
       module_pause, \
       module_is_speaking, \
       module_close, \
       {0, 0} \
    };

#define UPDATE_PARAMETER(old_value, new_value, setter) \
  if (old_value != (new_value)) \
    { \
      old_value = (new_value); \
      setter ((new_value)); \
    }

#define UPDATE_STRING_PARAMETER(old_value, new_value, setter) \
  if (old_value == NULL || strcmp (old_value, new_value)) \
    { \
      if (old_value != NULL) \
      { \
        g_free (old_value); \
	old_value = NULL; \
      } \
      if (new_value != NULL) \
      { \
        old_value = g_strdup (new_value); \
        setter ((new_value)); \
      } \
    }


#define CHILD_SAMPLE_BUF_SIZE 16384

typedef struct{
    int pc[2];
    int cp[2];
}TModuleDoublePipe;

static cst_wave *module_sample_wave = NULL;

static void* xmalloc(size_t size);
static void* xrealloc(void* data, size_t size);
static void xfree(void* data);

static int module_get_message_part(const char* message, char* part,
                                   unsigned int *pos, size_t maxlen,
                                   const char* dividers);

static void set_speaking_thread_parameters();
static short* module_add_samples(short* samples, short* data, size_t bytes, 
                                 size_t *num_samples);

static void module_parent_dp_init(TModuleDoublePipe dpipe);
static void module_child_dp_init(TModuleDoublePipe dpipe);
static void module_parent_dp_close(TModuleDoublePipe dpipe);
static void module_child_dp_close(TModuleDoublePipe dpipe);

static void module_child_dp_write(TModuleDoublePipe dpipe, const char *msg, size_t bytes);
static int module_parent_dp_write(TModuleDoublePipe dpipe, const char *msg, size_t bytes);
static int module_parent_dp_read(TModuleDoublePipe dpipe, char *msg, size_t maxlen);
static int module_child_dp_read(TModuleDoublePipe dpipe, char *msg, size_t maxlen);

static void module_strip_punctuation_default(char *buf);
static void module_strip_punctuation_some(char *buf, char* punct_some);

static void module_cstwave_free(cst_wave* wave);

static void module_sigblockall(void);
static void module_sigblockusr(sigset_t *signal_set);
static void module_sigunblockusr(sigset_t *signal_set);

static void*
xmalloc(size_t size)
{
    void *p;

    p = malloc(size);
    if (p == NULL) FATAL("Not enough memmory");
    return p;
}          

static void*
xrealloc(void *data, size_t size)
{
    void *p;

    if (data != NULL) 
        p = realloc(data, size);
    else 
        p = malloc(size);

    if (p == NULL) FATAL("Not enough memmory");

    return p;
}          

static void
xfree(void *data)
{
    if (data != NULL) free(data);
}

static int
module_get_message_part(const char* message, char* part, unsigned int *pos, size_t maxlen,
                        const char* dividers)
{    
    int i, n;
    int num_dividers;

    assert(part != NULL);
    assert(message != NULL);

    if (message[*pos] == 0) return 0;
    
    if (dividers != NULL){
        num_dividers = strlen(dividers);
    }else{
        num_dividers = 0;
    }

    for(i=0; i <= maxlen-1; i++){
        part[i] = message[*pos];

        if (part[i] == 0){            
            return i;
        }
        (*pos)++;

        // DBG("pos: %d", *pos);
        for(n = 0; n <= num_dividers; n++){
            if (part[i] == dividers[n]){    
                part[i+1] = 0;                
                return i+1;            
            }           
        }
    }
    part[i] = 0;

    return i;
}

int
module_speak_thread_wfork(sem_t *semaphore, pid_t *process_pid, 
                          void (*child_function) (TModuleDoublePipe, const size_t maxlen),
                          size_t (*parent_function) (TModuleDoublePipe, const char* message,
                                                     const size_t maxlen, const char* dividers,
                                                     int *pause_requested),
                          int *speaking_flag, char **message, const size_t maxlen,
                          const char *dividers, size_t *module_position, int *pause_requested)
{
    TModuleDoublePipe module_pipe;
    int ret;
    int status;

    set_speaking_thread_parameters();

    while(1){        
        sem_wait(semaphore);
        DBG("Semaphore on\n");

        ret = pipe(module_pipe.pc);
        if (ret != 0){
            DBG("Can't create pipe pc\n");
            *speaking_flag = 0;
            continue;
        }

        ret = pipe(module_pipe.cp);
        if (ret != 0){
            DBG("Can't create pipe cp\n");
            //            pclose(module_pipe.pc);

            *speaking_flag = 0;
            continue;
        }

        /* Create a new process so that we could send it signals */
        *process_pid = fork();

        switch(*process_pid){
        case -1:	
            DBG("Can't say the message. fork() failed!\n");
            //            pclose(module_pipe.pc);
            //            pclose(module_pipe.cp)
            *speaking_flag = 0;
            continue;

        case 0:
            /* This is the child. Make flite speak, but exit on SIGINT. */

            DBG("Starting child...\n");
            (* child_function)(module_pipe, maxlen);           
            break;

        default:
            /* This is the parent. Send data to the child. */

            *module_position = (* parent_function)(module_pipe, *message, maxlen, dividers,
                                                   pause_requested);

            DBG("Waiting for child...");
            waitpid(*process_pid, &status, 0);            

            DBG("child terminated -: status:%d signal?:%d signal number:%d.\n",
                WIFEXITED(status), WIFSIGNALED(status), WTERMSIG(status));

            *speaking_flag = 0;
        }        
    }
}

size_t
module_parent_wfork(TModuleDoublePipe dpipe, const char* message,
                    const size_t maxlen, const char* dividers, int *pause_requested)
{
    int pos = 0;
    char msg[16];
    int i;
    char *buf;
    size_t bytes;
    size_t read_bytes;


    DBG("Entering parent process, closing pipes");

    buf = (char*) malloc((maxlen+1) * sizeof(char));

    module_parent_dp_init(dpipe);

    pos = 0;
    while(1){
        DBG("  Looping...\n");

        bytes = module_get_message_part(message, buf, &pos, maxlen, dividers);
                
        if (bytes != 0){
            DBG("Sending buf to child:|%s|\n", buf);
            module_parent_dp_write(dpipe, buf, bytes);
        
            DBG("Waiting for response from flite child...\n");
            while(1){
                read_bytes = module_parent_dp_read(dpipe, msg, 8);
                if (read_bytes == 0){
                    DBG("parent: Read bytes 0, child stopped\n");                    
                    break;
                }
                if (msg[0] == 'C'){
                    DBG("Ok, received report to continue...\n");
                    break;
                }
            }
            if (*pause_requested){               
                DBG("Pause requested in parent, position %d\n", pos);                
                module_parent_dp_close(dpipe);
                *pause_requested = 0;
                return pos;
            }
        }

        if ((bytes == 0) || (read_bytes == 0)){
            DBG("End of data in parent, closing pipes");
            module_parent_dp_close(dpipe);
            break;
        }
            
    }    
}

void
module_audio_output_child(TModuleDoublePipe dpipe, const size_t nothing)
{
    char *data;  
    cst_wave *wave;
    short *samples = NULL;
    int bytes;
    int num_samples = 0;
    int ret;

    module_sigblockall();

    module_child_dp_init(dpipe);
    
    data = xmalloc(CHILD_SAMPLE_BUF_SIZE);

    ret = spd_audio_open(module_sample_wave);
    if (ret != 0){
        DBG("Can't open audio output!\n");
        module_child_dp_close(dpipe);
        exit(0);
    }

    DBG("Entering child loop\n");
    while(1){
        /* Read the waiting data */
        bytes = module_child_dp_read(dpipe, data, CHILD_SAMPLE_BUF_SIZE);
        DBG("child: Got %d bytes\n", bytes);
        if (bytes == 0){
            DBG("child: exiting, closing audio, pipes");
            spd_audio_close();
            module_child_dp_close(dpipe);
            DBG("child: good bye");
            exit(0);
        }

        /* Are we at the end? */
        if (!strncmp(data,"\r\n.\r\n", 5) 
            || !strncmp(data+bytes-5,"\r\n.\r\n", 5)){

            DBG("child: End of data caught\n");
            wave = (cst_wave*) xmalloc(sizeof(cst_wave));
            wave->type = strdup("riff");
            wave->sample_rate = 16000;
            wave->num_samples = num_samples;
            wave->num_channels = 1;
            wave->samples = samples;

            DBG("child: Sending data to audio output...\n");

            spd_audio_play_wave(wave);
            module_cstwave_free(wave);            

            DBG("child->parent: Ok, send next samples.");
            module_child_dp_write(dpipe, "C", 1);

            num_samples = 0;        
            samples = NULL;
        }else{
            samples = module_add_samples(samples, (short*) data,
                                         bytes, &num_samples);
        } 
    }        
}

static void
module_strip_punctuation_some(char *message, char *punct_chars)
{
    int len;
    char *p = message;
    guint32 u_char;
    int pchar_bytes;
    int i, n;
    assert(message != NULL);

    if (punct_chars == NULL) return;

    pchar_bytes = strlen(punct_chars);
    len = g_utf8_strlen(message, -1);

    for (i = 0; i <= len-1; i++){
        u_char = g_utf8_get_char(p);
        if (g_utf8_strchr(punct_chars, pchar_bytes, u_char)){
            for (n=0; n<=g_unichar_to_utf8(u_char, NULL)-1; n++){
                *p = ' ';
                p++;
            }
        }else{
            p = (char*) g_utf8_find_next_char(p, NULL);
        }
    }
}

static void
module_strip_punctuation_default(char *buf)
{
    assert(buf != NULL);
    module_strip_punctuation_some(buf, "~@#$%^&*+=|\\/<>[]_");
}

static short *
module_add_samples(short* samples, short* data, size_t bytes, size_t *num_samples)
{
    int i;
    short *new_samples;
    static size_t allocated;

    if (samples == NULL) *num_samples = 0;
    if (*num_samples == 0){
        allocated = CHILD_SAMPLE_BUF_SIZE;
        new_samples = (short*) xmalloc(CHILD_SAMPLE_BUF_SIZE);        
    }else{
        new_samples = samples;
    }

    if (*num_samples * sizeof(short) + bytes > allocated){
        allocated *= 2;
        new_samples = (short*) xrealloc(new_samples, allocated);
    }

    for(i=0; i <= (bytes/sizeof(short)) - 1; i++){
        new_samples[*num_samples] = data[i];
        (*num_samples)++;
    }

    return new_samples;
}

static int
module_parent_wait_continue(TModuleDoublePipe dpipe)
{
    char msg[16];
    int bytes;

    DBG("parent: Waiting for response from child...\n");
    while(1){
        bytes = module_parent_dp_read(dpipe, msg, 8);
        if (bytes == 0){
            DBG("parent: Read bytes 0, child stopped\n");
            return 1;
        }
        if (msg[0] == 'C'){
            DBG("parent: Ok, received report to continue...\n");
            return 0;
        }
    }
}

static int
module_parent_send_samples(TModuleDoublePipe dpipe, short* samples, size_t num_samples)
{
    size_t ret;
    size_t acc_ret = 0;

    while(acc_ret < num_samples * sizeof(short)){
        ret = module_parent_dp_write(dpipe, (char*) samples, 
                                     num_samples * sizeof(short) - acc_ret);
                
        if (ret == -1){
            DBG("parent: Error in sending data to child:\n   %s\n",
                strerror(errno));
            return -1;
        }
        acc_ret += ret;
    }
}

static void
module_sigblockall(void)
{
    int ret;
    sigset_t all_signals;

    sigfillset(&all_signals);

    ret = sigprocmask(SIG_BLOCK, &all_signals, NULL);
    if (ret != 0)
        DBG("flite: Can't block signals, expect problems with terminating!\n");
}

static void
module_sigunblockusr(sigset_t *some_signals)
{
    int ret;

    sigdelset(some_signals, SIGUSR1);
    ret = sigprocmask(SIG_SETMASK, some_signals, NULL);
    if (ret != 0)
        DBG("flite: Can't block signal set, expect problems with terminating!\n");
}

static void
module_sigblockusr(sigset_t *some_signals)
{
    int ret;

    sigaddset(some_signals, SIGUSR1);
    ret = sigprocmask(SIG_SETMASK, some_signals, NULL);
    if (ret != 0)
        DBG("flite: Can't block signal set, expect problems when terminating!\n");
}

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

static void
module_register_settings_voices(OutputModule *module)
{
    module->settings.voices = g_hash_table_new(g_str_hash, g_str_equal);
}

static char*
module_getvoice(OutputModule *module, char* language, EVoiceType voice)
{
    SPDVoiceDef *voices;
    char *ret;
    GHashTable *voice_table = module->settings.voices;

    if (voice_table == NULL){
        DBG("Can't get voice because voicetable is NULL\n");
        return NULL;
    }

    voices = g_hash_table_lookup(voice_table, language);
    if (voices == NULL){
        DBG("There are no voices in the table for language=%s\n", language);
        return NULL;
    }

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
        printf("Unknown voice");
        exit(1);
    }

    if (ret == NULL) ret = voices->male1;
    if (ret == NULL) printf("No voice available for this output module!");

    return ret;
}

void
set_speaking_thread_parameters()
{
    int ret;
    sigset_t all_signals;	    

    ret = sigfillset(&all_signals);
    if (ret == 0){
        ret = pthread_sigmask(SIG_BLOCK,&all_signals,NULL);
        if (ret != 0)
            DBG("flite: Can't set signal set, expect problems when terminating!\n");
    }else{
        DBG("flite: Can't fill signal set, expect problems when terminating!\n");
    }

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);				
}


int
module_write_data_ok(char *data)
{
    /* Tests */
    if(data == NULL){
        DBG("requested data NULL\n");		
        return -1;
    }

    if(data[0] == 0){
        DBG("requested data empty\n");
        return -1;
    }

    return 0;
}

module_terminate_thread(pthread_t thread)
{
    int ret;

    ret = pthread_cancel(thread);
    if (ret != 0){
        DBG("Cancelation of speak thread failed");
        return 1;
    }
    ret = pthread_join(thread, NULL);
    if (ret != 0){
        DBG("join failed!\n");
        return 1;
    }

    return 0;
}

sem_t*
module_semaphore_init()
{
    sem_t *semaphore;
    int ret;

    semaphore = (sem_t *) spd_malloc(sizeof(sem_t));
    ret = sem_init(semaphore, 0, 0);
    if (ret != 0){
        DBG("Semaphore initialization failed");
        xfree(semaphore);
        semaphore = NULL;
    }
    return semaphore;
}

static void
module_parent_dp_init(TModuleDoublePipe dpipe)
{
    close(dpipe.pc[0]);
    close(dpipe.cp[1]);
}

static void
module_parent_dp_close(TModuleDoublePipe dpipe)
{
    close(dpipe.pc[1]);
    close(dpipe.cp[0]);
}

static void
module_child_dp_init(TModuleDoublePipe dpipe)
{
    close(dpipe.pc[1]);
    close(dpipe.cp[0]);
}

static void
module_child_dp_close(TModuleDoublePipe dpipe)
{
    close(dpipe.pc[0]);
    close(dpipe.cp[1]);
}

static void
module_child_dp_write(TModuleDoublePipe dpipe, const char *msg, size_t bytes)
{
    assert(msg != NULL);
    write(dpipe.cp[1], msg, bytes);       
}

static int
module_parent_dp_write(TModuleDoublePipe dpipe, const char *msg, size_t bytes)
{
    int ret;
    assert(msg != NULL);
    DBG("GOING TO WRITE");
    ret = write(dpipe.pc[1], msg, bytes);      
    DBG("written %d bytes", ret);
    return ret;
}

static int
module_child_dp_read(TModuleDoublePipe dpipe, char *msg, size_t maxlen)
{
    int bytes;
    bytes = read(dpipe.pc[0], msg, maxlen);    
    return bytes;
}

static int
module_parent_dp_read(TModuleDoublePipe dpipe, char *msg, size_t maxlen)
{
    int bytes;
    bytes = read(dpipe.cp[0], msg, maxlen);    
    return bytes;
}

static char *
module_recode_to_iso(char *data, int bytes, char *language)
{
    char *recoded;
    
    if (language == NULL) recoded = strdup(data);

    if (!strcmp(language, "cs"))
        recoded = (char*) g_convert(data, bytes, "ISO8859-2", "UTF-8", NULL, NULL, NULL);
    else
        recoded = strdup(data);

    if(recoded == NULL) DBG("festival: Conversion to ISO conding failed\n");

    return recoded;
}

static configoption_t *
module_add_config_option(configoption_t *options, int *num_options, char *name, int type,
                  dotconf_callback_t callback, info_t *info,
                  unsigned long context)
{
    configoption_t *opts;
    int num_config_options = *num_options;

    assert(name != NULL);

    num_config_options++;
    opts = (configoption_t*) realloc(options, num_config_options * sizeof(configoption_t));
    opts[num_config_options-1].name = (char*) strdup(name);
    opts[num_config_options-1].type = type;
    opts[num_config_options-1].callback = callback;
    opts[num_config_options-1].info = info;
    opts[num_config_options-1].context = context;

    //    DBG("Added option: number:%d name: %s!!!\n\n", *num_options, name);

    *num_options = num_config_options;
    return opts;
}

static void*
module_get_ht_option(GHashTable *hash_table, const char *key)
{
    void *option;
    assert(key != NULL);

    option = g_hash_table_lookup(hash_table, key);
    if (option == NULL) DBG("Requested option by key %s not found.\n", key);

    return option;
}

static void
module_cstwave_free(cst_wave* wave)
{
    if (wave != NULL){
        if (wave->samples != NULL) free(wave->samples);
        free(wave);
    }
}

#define MOD_OPTION_1_STR(name) \
    static char *name = NULL; \
    DOTCONF_CB(name ## _cb) \
    { \
        xfree(name); \
        if (cmd->data.str != NULL) \
            name = strdup(cmd->data.str); \
        return NULL; \
    }

#define MOD_OPTION_1_INT(name) \
    static int name; \
    DOTCONF_CB(name ## _cb) \
    { \
        name = cmd->data.value; \
        return NULL; \
    }

#define MOD_OPTION_2(name, arg1, arg2) \
    typedef struct{ \
        char* arg1; \
        char* arg2; \
    }T ## name; \
    T ## name name; \
    \
    DOTCONF_CB(name ## _cb) \
    { \
        if (cmd->data.list[0] != NULL) \
            name.arg1 = strdup(cmd->data.list[0]); \
        if (cmd->data.list[1] != NULL) \
            name.arg2 = strdup(cmd->data.list[1]); \
        return NULL; \
    }

#define MOD_OPTION_2_HT(name, arg1, arg2) \
    typedef struct{ \
        char* arg1; \
        char* arg2; \
    }T ## name; \
    GHashTable *name; \
    \
    DOTCONF_CB(name ## _cb) \
    { \
        T ## name *new_item; \
        char* new_key; \
        new_item = (new_item*) malloc(sizeof(new_item)); \
        if (cmd->data.list[0] == NULL) return NULL; \
        new_item->arg1 = strdup(cmd->data.list[0]); \
        new_key = strdup(cmd->data.list[0]); \
        if (cmd->data.list[1] != NULL) \
           new_item->arg2 = strdup(cmd->data.list[1]); \
        else \
            new_item->arg2 = NULL; \
        g_hash_table_insert(name, new_key, new_item); \
        return NULL; \
    }

#define MOD_OPTION_3_HT(name, arg1, arg2, arg3) \
    typedef struct{ \
        char* arg1; \
        char* arg2; \
        char *arg3; \
    }T ## name; \
    GHashTable *name; \
    \
    DOTCONF_CB(name ## _cb) \
    { \
        T ## name *new_item; \
        char* new_key; \
        new_item = (T ## name *) malloc(sizeof(T ## name)); \
        if (cmd->data.list[0] == NULL) return NULL; \
        new_item->arg1 = strdup(cmd->data.list[0]); \
        new_key = strdup(cmd->data.list[0]); \
        if (cmd->data.list[1] != NULL) \
           new_item->arg2 = strdup(cmd->data.list[1]); \
        else \
            new_item->arg2 = NULL; \
        if (cmd->data.list[2] != NULL) \
           new_item->arg3 = strdup(cmd->data.list[2]); \
        else \
            new_item->arg3 = NULL; \
        g_hash_table_insert(name, new_key, new_item); \
        return NULL; \
    }

#define MOD_OPTION_1_STR_REG(name, default) \
    name = strdup(default); \
    *options = module_add_config_option(*options, num_options, #name, \
                                        ARG_STR, name ## _cb, NULL, 0);

#define MOD_OPTION_1_INT_REG(name, default) \
    name = default; \
    *options = module_add_config_option(*options, num_options, #name, \
                                        ARG_INT, name ## _cb, NULL, 0);

#define MOD_OPTION_MORE_REG(name) \
    *options = module_add_config_option(*options, num_options, #name, \
                                        ARG_LIST, name ## _cb, NULL, 0);

#define MOD_OPTION_HT_REG(name) \
    name = g_hash_table_new(g_str_hash, g_str_equal); \
    *options = module_add_config_option(*options, num_options, #name, \
                                        ARG_LIST, name ## _cb, NULL, 0);
