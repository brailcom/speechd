
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
 * $Id: festival.c,v 1.49 2004-06-28 08:05:18 hanke Exp $
 */

#include "fdset.h"
#include "fdsetconv.h"

#include "module_utils.h"
#include "module_utils_audio.c"

#include "festival_client.c"

#define MODULE_NAME     "festival"
#define MODULE_VERSION  "0.2"

DECLARE_DEBUG();

/* Thread and process control */
static pthread_t festival_speak_thread;
static pid_t festival_child_pid = 0;
static sem_t *festival_semaphore;
static int festival_speaking = 0;
static int festival_pause_requested = 0;

static char **festival_message;
static EMessageType festival_message_type;
static int festival_position = 0;
signed int festival_volume = 0;

int festival_stop = 0;

int festival_process_pid = 0;

FT_Info *festival_info;

enum{
    FCT_SOCKET = 0,
    FCT_LOCAL = 1,
} FestivalComType;

struct{
    int pipe_in[2];
    int pipe_out[2];
    int pid;
}module_p;
  
#define COM_SOCKET ((FestivalComType == FCT_SOCKET) ? 1 : 0)
#define COM_LOCAL ((FestivalComType == FCT_LOCAL) ? 1 : 0)

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
void festival_set_cap_let_recogn(ECapLetRecogn recogn);
void festival_set_volume(signed int volume);

int init_festival_standalone();
int init_festival_socket();

MOD_OPTION_1_INT(FestivalComunicationType);

MOD_OPTION_1_INT(FestivalMaxChunkLength);
MOD_OPTION_1_STR(FestivalDelimiters);
MOD_OPTION_1_STR(FestivalServerHost);
MOD_OPTION_1_STR(FestivalStripPunctChars);
MOD_OPTION_1_INT(FestivalServerPort);
MOD_OPTION_1_INT(FestivalPitchDeviation);
MOD_OPTION_1_INT(FestivalDebugSaveOutput);
MOD_OPTION_1_STR(FestivalRecodeFallback);

MOD_OPTION_1_INT(FestivalCacheOn);
MOD_OPTION_1_INT(FestivalCacheMaxKBytes);
MOD_OPTION_1_INT(FestivalCacheDistinguishVoices);
MOD_OPTION_1_INT(FestivalCacheDistinguishRate);
MOD_OPTION_1_INT(FestivalCacheDistinguishPitch);

MOD_OPTION_1_INT(FestivalReopenSocket);

typedef struct{
    size_t size;
    GHashTable *caches;
    GList *cache_counter;
}TCache;

typedef struct{
    time_t start;
    int count;
    size_t size;
    GHashTable *p_caches;
    char *key;
}TCounterEntry;

typedef struct{
    TCounterEntry *p_counter_entry;
    FT_Wave *fwave;
}TCacheEntry;

TCache FestivalCache;

int cache_init();
int cache_reset();
int cache_insert(char* key, EMessageType msgtype, FT_Wave *value);
FT_Wave* cache_lookup(const char *key, EMessageType msgtype, int add_counter);

/* Public functions */

int
module_load(void)
{

    INIT_SETTINGS_TABLES();

    REGISTER_DEBUG();

    MOD_OPTION_1_INT_REG(FestivalComunicationType, 0);

    MOD_OPTION_1_STR_REG(FestivalServerHost, "localhost");
    MOD_OPTION_1_INT_REG(FestivalServerPort, 1314);

    MOD_OPTION_1_INT_REG(FestivalDebugSaveOutput, 0);

    MOD_OPTION_1_STR_REG(FestivalRecodeFallback, "?");

    MOD_OPTION_1_INT_REG(FestivalCacheOn, 1);
    MOD_OPTION_1_INT_REG(FestivalCacheMaxKBytes, 5120);
    MOD_OPTION_1_INT_REG(FestivalCacheDistinguishVoices, 0);
    MOD_OPTION_1_INT_REG(FestivalCacheDistinguishRate, 0);
    MOD_OPTION_1_INT_REG(FestivalCacheDistinguishPitch, 0);

    /* TODO: Switch this option to 1 when the bug with the 40ms delay
     in Festival is fixed */
    MOD_OPTION_1_INT_REG(FestivalReopenSocket, 0);

    return 0;
}

int
module_init(void)
{
    int ret;

    INIT_DEBUG();

    DBG("module_init()");

    /* Initialize appropriate communication mechanism */
    FestivalComType = FestivalComunicationType;
    if (COM_SOCKET)
	if (init_festival_socket()) return -1;    
    if (COM_LOCAL)
	if(init_festival_standalone()) return -1;

    /* Initialize global variables */
    festival_message = (char**) xmalloc (sizeof (char*));    

    /* Initialize festival_speak thread to handle communication
     with festival in a separate thread (to be faster in communication
     with Speech Dispatcher) */
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

    DBG("module_speak()\n");

    if (data == NULL) return -1;

    if (festival_speaking){
        DBG("Speaking when requested to write\n");
        return -1;
    }

    festival_stop = 0;

    /* Check the parameters */
    if(module_write_data_ok(data) != 0) return -1;

    festival_message_type = msgtype;
    if ((msgtype == MSGTYPE_TEXT) && (msg_settings.spelling_mode == SPELLING_ON))
        festival_message_type = MSGTYPE_SPELL;

    /* If the connection crashed or language or voice
       change, we will need to set all the parameters again */
    if (COM_SOCKET){
	if (festival_connection_crashed){
	    DBG("Recovering after a connection loss");
	    CLEAN_OLD_SETTINGS_TABLE();
	    festival_info = festivalOpen(festival_info);
	    festival_connection_crashed = 0;
	}
    }

    /* If the voice was changed, re-set all the parameters */
    if ((msg_settings.voice != msg_settings_old.voice) 
        || strcmp(msg_settings.language, msg_settings_old.language)){
        CLEAN_OLD_SETTINGS_TABLE();
    }

    /* Setting voice parameters */
    DBG("Updating parameters");
    UPDATE_STRING_PARAMETER(language, festival_set_language);
    UPDATE_PARAMETER(voice, festival_set_voice);
    UPDATE_PARAMETER(rate, festival_set_rate);
    UPDATE_PARAMETER(pitch, festival_set_pitch);
    UPDATE_PARAMETER(volume, festival_set_volume);
    UPDATE_PARAMETER(punctuation_mode, festival_set_punctuation_mode);
    UPDATE_PARAMETER(cap_let_recogn, festival_set_cap_let_recogn);

    DBG("Requested data: |%s| \n", data);

    *festival_message = strdup(data);
    if (*festival_message == NULL){
        DBG("Error: Copying data unsuccesful.");
        return -1;
    }

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

    if(festival_speaking){
	/* if(COM_SOCKET) */
	if (0){
	    if (festival_info != 0)
		if ((festival_info->server_fd != -1) && FestivalReopenSocket){
		    /* TODO: Maybe use shutdown here? */
		    close(festival_info->server_fd);
		    festival_info->server_fd = -1;
		    festival_connection_crashed = 1;
		    DBG("festival socket closed by module_stop()");
		}
	}
	if (COM_LOCAL){
	    DBG("festival local stopped by sending SIGINT");
	    /* TODO: Write this function for local communication */
	    //	    festival_stop_local();
	}

	if (festival_child_pid != 0){
	    DBG("festival stopped by killing child process pid %d\n", festival_child_pid);
	    kill(festival_child_pid, SIGKILL);
	    festival_child_pid = 0;
	}
    }

    return 0;
}

size_t
module_pause(void)
{
    DBG("pause requested\n");
    if(festival_speaking){
        DBG("Sending request for pause to child\n");
        festival_pause_requested = 1;
        DBG("Waiting in pause for child to terminate\n");
        while(festival_speaking) usleep(10000);       
        DBG("Paused at mark: %d", current_index_mark);
	festival_pause_requested = 0;
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

    // DBG("festivalClose()");
    // festivalClose(festival_info);
    
    module_terminate_thread(festival_speak_thread);
   
    delete_FT_Info(festival_info);

    /* TODO: Solve this */
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

    cache_init();

    module_speak_thread_wfork(festival_semaphore, &festival_child_pid,
                              module_audio_output_child, _festival_parent, 
                              &festival_speaking, festival_message, &festival_message_type,
                              -1, NULL,
                              &festival_position, &festival_pause_requested);
    festival_speaking = 0;

    DBG("festival: speaking thread ended.......\n");    

    pthread_exit(NULL);
}	


#define CLEAN_UP(code) \
        { \
          delete_FT_Wave(fwave); \
          module_parent_dp_close(dpipe); \
          return(code); \
        }               

size_t
_festival_parent(TModuleDoublePipe dpipe, const char* message,
		 EMessageType msgtype, const size_t maxlen, const char* dividers,
		 int *pause_requested)
{
    int ret;
    int bytes;
    FT_Wave *fwave = NULL;
    int terminate = 0;
    int r = -1;
    static int debug_count = 0;
    int l;
    int h;
    char *tptr;

    char *callback;

    DBG("Entering parent process, closing pipes");

    bytes = strlen(message);

    module_parent_dp_init(dpipe);

    if (COM_SOCKET)
	FestivalEnableMultiMode(festival_info, "t");

    DBG("Synthesizing: |%s|", message);
    if (bytes > 0){
	switch(msgtype)
	    {
	    case MSGTYPE_CHAR:
	    case MSGTYPE_KEY:
	    case MSGTYPE_SOUND_ICON:
		DBG("Cache mechanisms...");
		fwave = cache_lookup(message, msgtype, 1);
		if (fwave != NULL){
		    if (fwave->num_samples != 0){
			if(FestivalDebugSaveOutput){
			    char filename_debug[256];
			    sprintf(filename_debug, "/tmp/debug-festival-%d.snd", debug_count++);
			    save_FT_Wave_snd(fwave, filename_debug);
			}
			ret = module_parent_send_samples(dpipe, fwave->samples,
							 fwave->num_samples, fwave->sample_rate,
							 festival_volume);
			if (ret == -1) CLEAN_UP(-1);
			
			DBG("parent (1): Sent %d bytes\n", ret);            
			ret = module_parent_dp_write(dpipe, "\r\nOK_SPEECHD_DATA_SENT\r\n", 24);
			if (ret == -1) CLEAN_UP(-1);
			
			ret = module_parent_wait_continue(dpipe);
			if (ret != 0) CLEAN_UP(-1);
			
			DBG("End of data in parent (1), closing pipes [%d:%d]", terminate, bytes);

			CLEAN_UP(0);
		    }else
			CLEAN_UP(0);
		}
	    case MSGTYPE_TEXT: break;
	    case MSGTYPE_SPELL: break;				    
	    }
	
	switch(msgtype)
	    {
	    case MSGTYPE_TEXT: r = festivalStringToWaveRequest(festival_info, message); break;
	    case MSGTYPE_SOUND_ICON: r = festivalSoundIcon(festival_info, message); break;
	    case MSGTYPE_CHAR: r = festivalCharacter(festival_info, message); break;
	    case MSGTYPE_KEY: r = festivalKey(festival_info, message); break;
	    case MSGTYPE_SPELL: r = festivalSpell(festival_info, message); break;
	    }
	if (r < 0){
	    DBG("Couldn't process the request to say the object.");
	    CLEAN_UP(-1);
	}
    }
    

    while(1){

	DBG("Retrieving data\n");

	/* (speechd-next) */
	fwave = festivalGetDataMulti(festival_info, &callback, &festival_stop, FestivalReopenSocket);	

	/* TODO: As for now, callbacks are ignored */
	if (callback != NULL){
	    DBG("Callback detected: %s", callback);
	    l = strlen(callback);
	    if (l<4) continue;	/* some other index mark */
	    if (strncmp(callback, "sdm_", 3)) continue;	/* some other index mark */
	    h = strtol(callback+3, &tptr, 10);
	    if (tptr != callback+3) current_index_mark = h;
	    DBG("Current index mark is set to %d", current_index_mark);
	    if(*pause_requested){
		DBG("Pause requested, not sending more data to child");
		CLEAN_UP(0);
	    }else{
		continue;
	    }
	}

	if (fwave == NULL){
	    DBG("End of sound samples for this message, returning");
	    CLEAN_UP(0);
	}
	
	if (msgtype == MSGTYPE_CHAR || msgtype == MSGTYPE_KEY ||
	    msgtype == MSGTYPE_SOUND_ICON){
	    DBG("Storing record for %s in cache\n", message);
	    //	    cache_insert(strdup(message), msgtype, fwave);
	}
	
	DBG("Sending message to child in wav: %d bytes\n",
	    (fwave->num_samples) * sizeof(short));
	
	if (fwave->num_samples != 0){
	    
	    if(FestivalDebugSaveOutput){
		char filename_debug[256];
		sprintf(filename_debug, "/tmp/debug-festival-%d.snd", debug_count++);
		save_FT_Wave_snd(fwave, filename_debug);
	    }
	    
	    ret = module_parent_send_samples(dpipe, fwave->samples,
					     fwave->num_samples, fwave->sample_rate,
					     festival_volume);
	    if (ret == -1) CLEAN_UP(-1);
	    DBG("parent: Sent %d bytes\n", ret);            
	    
	    ret = module_parent_dp_write(dpipe, "\r\nOK_SPEECHD_DATA_SENT\r\n", 24);
	    if (ret == -1) CLEAN_UP(-1);
	    ret = module_parent_wait_continue(dpipe);
	    if (ret != 0) CLEAN_UP(-1);
	}            

    }
}

void
festival_set_language(char* language)
{   
    FestivalSetLanguage(festival_info, language, NULL);
}

void
festival_set_voice(EVoiceType voice)
{
    char* voice_name;

    voice_name = EVoice2str(voice);
    FestivalSetVoice(festival_info, voice_name, NULL);
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
festival_set_volume(signed int volume)
{
  festival_volume = volume;
}

void
festival_set_punctuation_mode(EPunctMode punct)
{
    char *punct_mode;
    punct_mode = EPunctMode2str(punct);
    FestivalSetPunctuationMode(festival_info, punct_mode);
    xfree(punct_mode);
}

void
festival_set_cap_let_recogn(ECapLetRecogn recogn)
{
    char *recogn_mode;

    if (recogn == RECOGN_NONE) recogn_mode = NULL;
    else recogn_mode = ECapLetRecogn2str(recogn);
    FestivalSetCapLetRecogn(festival_info, recogn_mode, NULL);
    xfree(recogn_mode);
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



/* --- Cache related functions --- */

void
cache_destroy_entry(gpointer data)
{
    TCacheEntry *entry = data;
    xfree(entry->fwave);
    xfree(entry);
}

void
cache_destroy_table_entry(gpointer data)
{
    g_hash_table_destroy(data);
}

void
cache_free_counter_entry(gpointer data, gpointer user_data)
{
    xfree(((TCounterEntry*)data)->key);
    xfree(data);
}

int
cache_init()
{

    if (FestivalCacheOn == 0) return 0;

    FestivalCache.size = 0;
    FestivalCache.caches = g_hash_table_new_full(g_str_hash, g_str_equal, free,
                                                 cache_destroy_table_entry);
    FestivalCache.cache_counter = NULL;
    DBG("Cache: initialized");
    return 0;
}

int
cache_destroy()
{
    g_hash_table_destroy(FestivalCache.caches);
    g_list_foreach(FestivalCache.cache_counter, cache_free_counter_entry, NULL);
    g_list_free(FestivalCache.cache_counter);
    return 0;
}

int
cache_reset()
{
    /* TODO: it could free everything in the cache and go from start,
       but currently it isn't called by anybody */
    return 0;
}

/* Compare two cache entries according to their score (how many
   times the entry was requested divided by the time it exists
   in the database) */
gint
cache_counter_comp(gconstpointer a, gconstpointer b)
{
    const TCounterEntry *A = a;
    const TCounterEntry *B = b;
    time_t t;
    float ret;

    t = time(NULL);
    ret =  (((float) A->count  / (float)(t - A->start)) 
             - ((float) B->count / (float)(t - B->start)));

    if (ret > 0) return -1;
    if (ret == 0) return 0;
    if (ret < 0) return 1;

    return 0;
}

/* List scores of all entries in the cache*/
void
cache_debug_foreach_list_score(gpointer a, gpointer user)
{
    const TCounterEntry *A = a;
    time_t t;

    t = time(NULL);
    DBG("key: %s      -> score %f (count: %d, dtime: %d)", A->key, ((float) A->count  / (float)(t - A->start)),
        (int) A->count, (int) (t - A->start));
}

/* Remove 1/3 of the least used (according to cache_counter_comp) entries 
   (measured by size) */
int
cache_clean(size_t new_element_size)
{
    size_t req_size;
    GList *gl;
    TCounterEntry *centry;

    DBG("Cache: cleaning, cache size %d kbytes (>max %d).", FestivalCache.size/1024,
        FestivalCacheMaxKBytes);

    req_size = 2*FestivalCache.size/3;

    FestivalCache.cache_counter = g_list_sort(FestivalCache.cache_counter, cache_counter_comp);
    g_list_foreach(FestivalCache.cache_counter, cache_debug_foreach_list_score, NULL);

    while((FestivalCache.size + new_element_size) > req_size){
        gl = g_list_last(FestivalCache.cache_counter);
        if (gl == NULL) break;
        if (gl->data == NULL){
            DBG("Error: Cache: gl->data in cache_clean is NULL, but shouldn't be.");
            return -1;
        }
        centry = gl->data;
        FestivalCache.size -= centry->size;       
        DBG("Cache: Removing element with key '%s'", centry->key);
        if (FestivalCache.size < 0){
            DBG("Error: Cache: FestivalCache.size < 0, this shouldn't be.");
            return -1;
        }
        /* Remove the data itself from the hash table */
        g_hash_table_remove(centry->p_caches, centry->key);
        /* Remove the associated entry in the counter list */
        cache_free_counter_entry(centry, NULL);
        FestivalCache.cache_counter = g_list_delete_link(FestivalCache.cache_counter, gl);
    }

    return 0;
}

/* Generate a key for searching between the different hash tables */
char*
cache_gen_key(EMessageType type)
{
    char *key;
    char ktype;    
    int kpitch = 0, krate = 0, kvoice = 0;

    if (msg_settings.language == NULL) return NULL;

    if (FestivalCacheDistinguishVoices) kvoice = msg_settings.voice;
    if (FestivalCacheDistinguishPitch) kpitch = msg_settings.pitch;
    if (FestivalCacheDistinguishRate) krate = msg_settings.rate;

    if (type == MSGTYPE_CHAR) ktype = 'c';
    else if (type == MSGTYPE_KEY) ktype = 'k';
    else if (type == MSGTYPE_SOUND_ICON) ktype = 's';
    else{
	DBG("Invalid message type for cache_gen_key()");
	return NULL;
    }

    key = g_strdup_printf("%c_%s_%d_%d_%d", ktype, msg_settings.language, kvoice,
                                krate, kpitch);

    return key;
}

/* Insert one entry into the cache */
int
cache_insert(char* key, EMessageType msgtype, FT_Wave *fwave)
{
    GHashTable *cache;
    TCacheEntry *entry;
    TCounterEntry *centry;
    char *key_table;

    if (FestivalCacheOn == 0) return 0;

    if (key == NULL) return -1;

    /* Check if the entry isn't present already */
    if (cache_lookup(key, msgtype, 0) != NULL)
        return 0;

    key_table = cache_gen_key(msgtype);

    DBG("Cache: Inserting wave with key:'%s' into table '%s'", key, key_table);

    /* Clean less used cache entries if the size would exceed max. size */
    if ((FestivalCache.size + fwave->num_samples * sizeof(short))
        > (FestivalCacheMaxKBytes*1024))
        if (cache_clean(fwave->num_samples * sizeof(short)) != 0) return -1;
    
    /* Select the right table according to language, voice, etc. or create a new one*/
    cache = g_hash_table_lookup(FestivalCache.caches, key_table);
    if (cache == NULL){
        cache = g_hash_table_new(g_str_hash, g_str_equal);
        g_hash_table_insert(FestivalCache.caches, key_table, cache);
    }else{
        xfree(key_table);
    }

    /* Fill the CounterEntry structure that will later allow us to remove
       the less used entries from cache */
    centry = (TCounterEntry*) xmalloc(sizeof(TCounterEntry));
    centry->start = time(NULL);
    centry->count = 1;
    centry->size = fwave->num_samples * sizeof(short);
    centry->p_caches = cache;
    centry->key = strdup(key);
    FestivalCache.cache_counter = g_list_append(FestivalCache.cache_counter, centry);

    entry = (TCacheEntry*) xmalloc(sizeof(TCacheEntry));
    entry->p_counter_entry = centry;
    entry->fwave = (void*) xmalloc(centry->size);
    memcpy(entry->fwave, fwave, centry->size);

    FestivalCache.size += centry->size;
    g_hash_table_insert(cache, strdup(key), entry);

    return 0;
}

/* Retrieve wave from the cache */
FT_Wave*
cache_lookup(const char *key, EMessageType msgtype, int add_counter)
{
    GHashTable *cache;
    TCacheEntry *entry;
    char *key_table;

    if (FestivalCacheOn == 0) return NULL;
    if (key == NULL) return NULL;

    key_table = cache_gen_key(msgtype);

    if (add_counter) DBG("Cache: looking up a wave with key '%s' in '%s'", key, key_table);

    if (key_table == NULL) return NULL;
    cache = g_hash_table_lookup(FestivalCache.caches, key_table);
    xfree(key_table);
    if (cache == NULL) return NULL;   

    entry = g_hash_table_lookup(cache, key);
    if (entry == NULL) return NULL;
    entry->p_counter_entry->count++;

    DBG("Cache: corresponding wave found: %s", key);    

    return entry->fwave;
}


int
init_festival_standalone()
{
    int ret;
    int fr;

    if ( (pipe(module_p.pipe_in) != 0) 
          || ( pipe(module_p.pipe_out) != 0)  ){
        DBG("Can't open pipe! Module not loaded.");
        return -1;
    }     

    DBG("Starting Festival as a child process");

    fr = fork();
    switch(fr){
    case -1: DBG("ERROR: Can't fork! Module not loaded."); return -1;
    case 0:
        ret = dup2(module_p.pipe_in[0], 0);
        close(module_p.pipe_in[0]);
        close(module_p.pipe_in[1]);
 
        ret = dup2(module_p.pipe_out[1], 1);
        close(module_p.pipe_out[1]);
        close(module_p.pipe_out[0]);        
        
	/* TODO: fix festival hardcoded path */
	if (execlp("festival", "", (char *) 0) == -1)
	    exit(1);	
                
    default:
        festival_process_pid = fr;
        close(module_p.pipe_in[0]);
        close(module_p.pipe_out[1]);

        usleep(100);            /* So that the other child has at least time to fail
                                   with the execlp */
        ret = waitpid(module_p.pid, NULL, WNOHANG);
        if (ret != 0){
            DBG("Can't execute festival. Bad filename in configuration?");            
            return -1;
        }
	
        return 0;
    }
    
    assert(0);
}

int
init_festival_socket()
{

    /* Init festival and register a new voice */
    festival_info = festivalDefaultInfo();
    festival_info->server_host = FestivalServerHost;
    festival_info->server_port = FestivalServerPort;
    
    festival_info = festivalOpen(festival_info);
    if (festival_info == NULL) return -1;
    
    DBG("FestivalServerHost = %s\n", FestivalServerHost);
    DBG("FestivalServerPort = %d\n", FestivalServerPort);

    return 0;
}

int
stop_festival_local()
{
    if (festival_process_pid > 0)
	kill(festival_process_pid, SIGINT);
    return 0;
}


#include "module_main.c"
