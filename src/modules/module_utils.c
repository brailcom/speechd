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
 * $Id: module_utils.c,v 1.26 2003-12-18 23:41:05 hanke Exp $
 */

#include <semaphore.h>
#include <dotconf.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <dotconf.h>

#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "fdsetconv.c"

SPDMsgSettings msg_settings;
SPDMsgSettings msg_settings_old;

int current_index_mark;

/* This variable is defined later in this
   file using MOD_OPTION* macro */
int debug_on;
FILE* debug_file;

int SPDSemaphore;

configfile_t *configfile = NULL;
configoption_t *module_dc_options;
int module_num_dc_options = 0;

#define CLEAN_OLD_SETTINGS_TABLE()\
 msg_settings_old.rate = -101;\
 msg_settings_old.pitch = -101;\
 msg_settings_old.punctuation_mode = -1;\
 msg_settings_old.spelling_mode = -1;\
 msg_settings_old.cap_let_recogn = -1;\
 msg_settings_old.language = NULL;\
 msg_settings_old.voice = NO_VOICE;

#define INIT_SETTINGS_TABLES()\
 msg_settings.rate = 0;\
 msg_settings.pitch = 0;\
 msg_settings.punctuation_mode = PUNCT_NONE;\
 msg_settings.spelling_mode = SPELLING_OFF;\
 msg_settings.cap_let_recogn = RECOGN_NONE;\
 msg_settings.language = strdup("en");\
 msg_settings.voice = MALE1;\
 CLEAN_OLD_SETTINGS_TABLE()

#define CLOSE_DEBUG_FILE() \
  if (debug_file != NULL) fclose(debug_file);

#define DBG(arg...) \
  if (debug_on){ \
    time_t t; \
    struct timeval tv; \
    char *tstr; \
    t = time(NULL); \
    tstr = strdup(ctime(&t)); \
    tstr[strlen(tstr)-1] = 0; \
    gettimeofday(&tv,NULL); \
    fprintf(debug_file, "%s [%d]",tstr, (int) tv.tv_usec); \
    fprintf(debug_file, ": "); \
    fprintf(debug_file, arg); \
    fprintf(debug_file, "\n"); \
    fflush(debug_file); \
    xfree(tstr); \
  }

#define FATAL(msg) { \
     fprintf(stderr, "Fatal error in output module [%s:%d]:\n   "msg, \
             __FILE__, __LINE__); \
     exit(EXIT_FAILURE); \
   }

int     module_load         (void);
int     module_init         (void);
int     module_speak        (char *data, size_t bytes, EMessageType msgtype);
int     module_stop         (void);
size_t  module_pause        (void);
int     module_is_speaking  (void);
void    module_close        (int status);

#define UPDATE_PARAMETER(value, setter) \
  if (msg_settings_old.value != msg_settings.value) \
    { \
      msg_settings_old.value = msg_settings.value; \
      setter (msg_settings.value); \
    }

#define UPDATE_STRING_PARAMETER(value, setter) \
  if (msg_settings_old.value == NULL \
         || strcmp (msg_settings_old.value, msg_settings.value)) \
    { \
      if (msg_settings_old.value != NULL) \
      { \
        xfree (msg_settings_old.value); \
	msg_settings_old.value = NULL; \
      } \
      if (msg_settings.value != NULL) \
      { \
        msg_settings_old.value = g_strdup (msg_settings.value); \
        setter (msg_settings.value); \
      } \
    }

#define CHILD_SAMPLE_BUF_SIZE 16384

typedef struct{
    int pc[2];
    int cp[2];
}TModuleDoublePipe;

static void* xmalloc(size_t size);
static void* xrealloc(void* data, size_t size);
static void xfree(void* data);

static int module_get_message_part(const char* message, char* part,
                                   unsigned int *pos, size_t maxlen,
                                   const char* dividers);

static void set_speaking_thread_parameters();

static void module_parent_dp_init(TModuleDoublePipe dpipe);
static void module_child_dp_init(TModuleDoublePipe dpipe);
static void module_parent_dp_close(TModuleDoublePipe dpipe);
static void module_child_dp_close(TModuleDoublePipe dpipe);

static void module_child_dp_write(TModuleDoublePipe dpipe, const char *msg, size_t bytes);
static int module_parent_dp_write(TModuleDoublePipe dpipe, const char *msg, size_t bytes);
static int module_parent_dp_read(TModuleDoublePipe dpipe, char *msg, size_t maxlen);
static int module_child_dp_read(TModuleDoublePipe dpipe, char *msg, size_t maxlen);

static void module_signal_end(void);

static void module_strip_punctuation_default(char *buf);
static void module_strip_punctuation_some(char *buf, char* punct_some);

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
    data = NULL;
}

char*
do_message(EMessageType msgtype)
{
    int ret;
    char *cur_line;
    GString *msg;
    int n;
    int nlines = 0;

    msg = g_string_new("");

    printf("202 OK RECEIVING MESSAGE\n");
    fflush(stdout);

    while(1){
        cur_line = NULL;
        n = 0;
        ret = getline(&cur_line, &n, stdin);
        nlines++;
        if (ret == -1) return "401 ERROR INTERNAL";

        if (!strcmp(cur_line, "..\n")){
            xfree(cur_line);
            cur_line = strdup(".\n");
        }else if (!strcmp(cur_line, ".\n")){
            /* Strip the trailing \n */
            msg->str[strlen(msg->str)-1] = 0;
            break;
        }
        g_string_append(msg, cur_line);
    }
    

    if ((msgtype != MSGTYPE_TEXT) && (nlines > 2)){
        return "305 DATA MORE THAN ONE LINE";
    }
  
    ret = module_speak(msg->str, strlen(msg->str), msgtype);

    g_string_free(msg,1);
    if (ret <= 0) return "301 ERROR CANT SPEAK";
    
    return "200 OK SPEAKING";
}

char*
do_speak(void)
{
    do_message(MSGTYPE_TEXT);
}

char*
do_sound_icon(void)
{
    do_message(MSGTYPE_SOUND_ICON);
}

char*
do_char(void)
{
    do_message(MSGTYPE_CHAR);
}

char*
do_key(void)
{
    do_message(MSGTYPE_KEY);
}

char*
do_stop(void)
{
    module_stop();
    return "201 OK STOPPED";
}

char*
do_pause(void)
{
    size_t pos;
    static char resp[128];
    pos = module_pause();
    
    snprintf(resp, 127, "202-%d\n202 OK PAUSED", pos);

    return resp;
}

#define SET_PARAM_NUM(name, cond) \
 if(!strcmp(cur_item, #name)){ \
     number = strtol(cur_value, &tptr, 10); \
     if(!(cond)){ err = 2; continue; } \
     if (tptr == cur_value){ err = 2; continue; } \
     msg_settings.name = number; \
 }

#define SET_PARAM_STR(name) \
 if(!strcmp(cur_item, #name)){ \
     xfree(msg_settings.name); \
     if(!strcmp(cur_value, "NULL")) msg_settings.name = NULL; \
     else msg_settings.name = strdup(cur_value); \
 }

#define SET_PARAM_STR_C(name, fconv) \
 if(!strcmp(cur_item, #name)){ \
     ret = fconv(cur_value); \
     if (ret != -1) msg_settings.name = ret; \
     else err = 2; \
 }

char*
do_set(void)
{
    char *cur_item = NULL;
    char *cur_value = NULL;
    char *line = NULL;
    int ret;
    int n;
    int number; char *tptr;
    int err = 0;                /* Error status */

    printf("203 OK RECEIVING SETTINGS\n");
    fflush(stdout);

    while(1){
        line = NULL; n = 0;
        ret = getline(&line, &n, stdin);
        if (ret == -1){ err=1; break; }
        if (!strcmp(line, ".\n")){
            xfree(line);
            break;
        }
        if (!err){
            cur_item = strtok(line, "=");
            if (cur_item == NULL){ err=1; continue; }
            cur_value = strtok(NULL, "\n");
            if (cur_value == NULL){ err=1; continue; }
            
            SET_PARAM_NUM(rate, ((number>=-100) && (number<=100)))
            else SET_PARAM_NUM(pitch, ((number>=-100) && (number<=100)))
            else SET_PARAM_STR_C(punctuation_mode, str2EPunctMode)
            else SET_PARAM_STR_C(spelling_mode, str2ESpellMode)
            else SET_PARAM_STR_C(cap_let_recogn, str2ECapLetRecogn)
            else SET_PARAM_STR_C(voice, str2EVoice)
            else SET_PARAM_STR(language)
            else err=2;             /* Unknown parameter */
        }
        xfree(line);
    }

    if (err == 0) return "203 OK SETTINGS RECEIVED";
    if (err == 1) return "302 ERROR BAD SYNTAX";
    if (err == 2) return "303 ERROR INVALID PARAMETER OR VALUE";
    
    return "401 ERROR INTERNAL"; /* Can't be reached */
}
#undef SET_PARAM_NUM
#undef SET_PARAM_STR

char*
do_speaking(void)
{
    if (module_is_speaking()) return "205-1\n205 OK SPEAKING STATUS SENT";
    else return "205-0\n205 OK SPEAKING STATUS SENT";
}

/* This has to return int (although it doesn't return) so that we could
 * call it from PROCESS_CMD() macro like the other commands that return
 * something */
int
do_quit(void)
{
    printf("210 OK QUIT\n");    
    fflush(stdout);
    module_close(0);
}

static int
module_get_message_part(const char* message, char* part, unsigned int *pos, size_t maxlen,
                        const char* dividers)
{    
    int i, n;
    int num_dividers;
    int im;
    int len;

    assert(part != NULL);
    assert(message != NULL);

    len = strlen(message);

    if (message[*pos] == 0) return -1;
    
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

        // DBG("pos: %d", *pos);

        if ((message[*pos] != '@') && ((len-1-i) > 2)){
            if ((message[*pos+1] == ' ') || (message[*pos+1] == '\n')
                || (message[*pos+1] == '\r') || (message[*pos+1] == '@')){
                for(n = 0; n <= num_dividers; n++){
                    if ((part[i] == dividers[n])){                        
                        part[i+1] = 0;                
                        current_index_mark = -1;
                        (*pos)++;
                        return i+1;            
                    }           
                }
                if ((message[*pos] == '\n') && (message[*pos+1] == '\n')){
                    part[i+1] = 0;                
                    current_index_mark = -1;
                    (*pos)++;
                    return i+1;
                }
                if((len-1-i) > 4){
                    if(((message[*pos] == '\r') && (message[*pos+1] == '\n'))
                       && ((message[*pos+2] == '\r') && (message[*pos+3] == '\n'))){
                        part[i+1] = 0;                
                        current_index_mark = -1;
                        (*pos)++;
                        return i+1;
                    }
                }
            }
        }

        if ((message[*pos] == '@') && ((len-1-i) > 2)){
            if (message[*pos+1] == '@'){
                (*pos) += 2;      /* Skip both '@' */
                continue;
            }else{            
                char mark_number[8] = "";
                int t = 0;

                DBG("Found index mark\n");

                 if (i>0){
                     part[i] = 0;
                     current_index_mark = -1;
                     return i+1;            
                 }

                (*pos)++;
                for(; i <= maxlen-1;){
                    if (message[*pos] == 0){
                        DBG("ERROR: Index mark opened but not terminated!");
                        current_index_mark = -1;
                        return i;
                    }
                    if (message[*pos] != '@'){
                        mark_number[t++] = message[*pos];
                    }else{
                        (*pos)++;
                        mark_number[t] = 0;
                        break;
                    }
                    (*pos)++;
                }
                
                DBG("Retrieved mark number string: |%s|\n", mark_number);
                {
                    char *tailptr;
                    current_index_mark = strtol(mark_number, &tailptr, 0);
                    if (tailptr == mark_number){
                        DBG("Invalid index mark -- Not a number!\n");
                    }
                }

                return i;
            }
        }        

        (*pos)++;
    }
    part[i] = 0;

    return i;
}

static void
module_strip_punctuation_some(char *message, char *punct_chars)
{
    int len;
    char *p = message;
    guint32 u_char;
    int pchar_bytes;
    int i;
    assert(message != NULL);

    if (punct_chars == NULL) return;

    pchar_bytes = strlen(punct_chars);
    len = strlen(message);
    for (i = 0; i <= len-1; i++){
        if (strchr(punct_chars, *p)){
            if (*p == '@'){
                if (((len-1-i)>0) && (*(p+1) == '@')){
                    *p = ' ';
                    *(p+1) = ' ';
                }
            }else{
                DBG("Substitution %d: char -%c- for whitespace\n", i, *p);
                *p = ' ';
            }
        }
        p++;
    }
}

static void
module_strip_punctuation_default(char *buf)
{
    assert(buf != NULL);
    module_strip_punctuation_some(buf, "~#$%^&*+=|<>[]_");
}

typedef void (*TChildFunction)(TModuleDoublePipe dpipe, const size_t maxlen);
typedef size_t (*TParentFunction)(TModuleDoublePipe dpipe, const char* message,
                                  const EMessageType msgtype, const size_t maxlen,
                                  const char* dividers, int *pause_requested);
void
module_speak_thread_wfork(sem_t *semaphore, pid_t *process_pid, 
                          TChildFunction child_function,
                          TParentFunction parent_function,
                          int *speaking_flag, char **message, const EMessageType *msgtype,
                          const size_t maxlen, const char *dividers, size_t *module_position,
                          int *pause_requested)
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
            close(module_pipe.pc[0]);     close(module_pipe.pc[1]);
            *speaking_flag = 0;
            continue;
        }

        /* Create a new process so that we could send it signals */
        *process_pid = fork();

        switch(*process_pid){
        case -1:	
            DBG("Can't say the message. fork() failed!\n");
            close(module_pipe.pc[0]);     close(module_pipe.pc[1]);
            close(module_pipe.cp[0]);     close(module_pipe.cp[1]);
            *speaking_flag = 0;
            continue;

        case 0:
            /* This is the child. Make flite speak, but exit on SIGINT. */

            DBG("Starting child...\n");
            (* child_function)(module_pipe, maxlen);           
            exit(0);

        default:
            /* This is the parent. Send data to the child. */

            *module_position = (* parent_function)(module_pipe, *message, *msgtype,
                                                   maxlen, dividers, pause_requested);

            DBG("Waiting for child...");
            waitpid(*process_pid, &status, 0);            
            
            *speaking_flag = 0;

            module_signal_end();

            DBG("child terminated -: status:%d signal?:%d signal number:%d.\n",
                WIFEXITED(status), WIFSIGNALED(status), WTERMSIG(status));
        }        
    }
}

size_t
module_parent_wfork(TModuleDoublePipe dpipe, const char* message, EMessageType msgtype,
                    const size_t maxlen, const char* dividers, int *pause_requested)
{
    int pos = 0;
    char msg[16];
    int i;
    char *buf;
    int bytes;
    size_t read_bytes;


    DBG("Entering parent process, closing pipes");

    buf = (char*) malloc((maxlen+1) * sizeof(char));

    module_parent_dp_init(dpipe);

    pos = 0;
    while(1){
        DBG("  Looping...\n");

        bytes = module_get_message_part(message, buf, &pos, maxlen, dividers);

        DBG("Returned %d bytes from get_part\n", bytes);

        if (*pause_requested && (current_index_mark!=-1)){               
            DBG("Pause requested in parent, position %d\n", current_index_mark);                
            module_parent_dp_close(dpipe);
            *pause_requested = 0;
            return current_index_mark;
        }             
   
        if (bytes > 0){
            DBG("Sending buf to child:|%s| %d\n", buf, bytes);
            module_parent_dp_write(dpipe, buf, bytes);
        
            DBG("Waiting for response from child...\n");
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
        }

        if ((bytes == -1) || (read_bytes == 0)){
            DBG("End of data in parent, closing pipes");
            module_parent_dp_close(dpipe);
            break;
        }
            
    }    
    return 0;
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
    DBG("going to write %d bytes", bytes);
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

    semaphore = (sem_t *) malloc(sizeof(sem_t));
    if (semaphore == NULL) return NULL;
    ret = sem_init(semaphore, 0, 0);
    if (ret != 0){
        DBG("Semaphore initialization failed");
        xfree(semaphore);
        semaphore = NULL;
    }
    return semaphore;
}

static char *
module_recode_to_iso(char *data, int bytes, char *language, char *fallback)
{
    char *recoded;
    
    if (language == NULL) recoded = strdup(data);

    if (!strcmp(language, "cs"))
        recoded = (char*) g_convert_with_fallback(data, bytes, "ISO8859-2", "UTF-8",
                                                  fallback, NULL, NULL, NULL);
    else
        recoded = (char*) g_convert_with_fallback(data, bytes, "ISO8859-1", "UTF-8",
                                                  fallback, NULL, NULL, NULL);

    if(recoded == NULL) DBG("festival: Conversion to ISO coding failed\n");

    return recoded;
}

static int
semaphore_post(int sem_id)
{
    static struct sembuf sem_b;
    int err;

    sem_b.sem_num = 0;
    sem_b.sem_op = 1;          /* V() */
    sem_b.sem_flg = SEM_UNDO;
    return semop(sem_id, &sem_b, 1);
}

void
module_signal_end(void)
{
    semaphore_post(SPDSemaphore);
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
    opts = (configoption_t*) realloc(options, (num_config_options+1) * sizeof(configoption_t));
    opts[num_config_options-1].name = (char*) strdup(name);
    opts[num_config_options-1].type = type;
    opts[num_config_options-1].callback = callback;
    opts[num_config_options-1].info = info;
    opts[num_config_options-1].context = context;

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


configoption_t *
add_config_option(configoption_t *options, int *num_config_options, char *name, int type,
                  dotconf_callback_t callback, info_t *info,
                  unsigned long context)
{
    configoption_t *opts;

    (*num_config_options)++;
    opts = (configoption_t*) realloc(options, (*num_config_options) * sizeof(configoption_t));
    opts[*num_config_options-1].name = strdup(name);
    opts[*num_config_options-1].type = type;
    opts[*num_config_options-1].callback = callback;
    opts[*num_config_options-1].info = info;
    opts[*num_config_options-1].context = context;
    return opts;
}


/* --- MODULE DOTCONF OPTIONS DEFINITION AND REGISTRATION --- */

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

/* TODO: Switch this to real float, not /100 integer,
   as soon as DotConf supports floats */
#define MOD_OPTION_1_FLOAT(name) \
    static float name; \
    DOTCONF_CB(name ## _cb) \
    { \
        name = ((float) cmd->data.value) / ((float) 100); \
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
    module_dc_options = module_add_config_option(module_dc_options, \
                                     &module_num_dc_options, #name, \
                                     ARG_STR, name ## _cb, NULL, 0);

#define MOD_OPTION_1_INT_REG(name, default) \
    name = default; \
    module_dc_options = module_add_config_option(module_dc_options, \
                                     &module_num_dc_options, #name, \
                                     ARG_INT, name ## _cb, NULL, 0);

/* TODO: Switch this to real float, not /100 integer,
   as soon as DotConf supports floats */
#define MOD_OPTION_1_FLOAT_REG(name, default) \
    name = default; \
    module_dc_options = module_add_config_option(module_dc_options, \
                                     &module_num_dc_options, #name, \
                                     ARG_INT, name ## _cb, NULL, 0);

#define MOD_OPTION_MORE_REG(name) \
    module_dc_options = module_add_config_option(module_dc_options, \
                                     &module_num_dc_options, #name, \
                                     ARG_LIST, name ## _cb, NULL, 0);

#define MOD_OPTION_HT_REG(name) \
    name = g_hash_table_new(g_str_hash, g_str_equal); \
    module_dc_options = module_add_config_option(module_dc_options, \
                                     &module_num_dc_options, #name, \
                                     ARG_LIST, name ## _cb, NULL, 0);


/* --- DEBUGGING SUPPORT  ---*/

#define DECLARE_DEBUG() \
    static char debug_filename[]; \
    MOD_OPTION_1_INT(Debug); \
    MOD_OPTION_1_STR(DebugFile);

#define REGISTER_DEBUG() \
    MOD_OPTION_1_INT_REG(Debug, 0); \
    MOD_OPTION_1_STR_REG(DebugFile, "/tmp/debug-module");

#define INIT_DEBUG() \
{ \
    debug_on = Debug; \
    if (debug_on){ \
        debug_file = fopen(DebugFile, "w"); \
        if (debug_file == NULL){ \
           printf("Can't open debug file!"); \
           debug_file = stdout; \
        } \
    }else{ \
        debug_file = NULL; \
    } \
}
