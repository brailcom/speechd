
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
 * $Id: festival.c,v 1.46 2004-02-10 21:17:26 hanke Exp $
 */

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
signed int festival_volume = 0;

char *festival_coding;

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
void festival_set_cap_let_recogn(ECapLetRecogn recogn);
void festival_set_volume(signed int volume);

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
FT_Wave* cache_lookup(char *key, EMessageType msgtype, int add_counter);

/* Public functions */

int
module_load(void)
{

    INIT_SETTINGS_TABLES();

    REGISTER_DEBUG();

    MOD_OPTION_1_INT_REG(FestivalMaxChunkLength, 300);
    MOD_OPTION_1_STR_REG(FestivalDelimiters, ".");

    MOD_OPTION_1_STR_REG(FestivalServerHost, "localhost");
    MOD_OPTION_1_INT_REG(FestivalServerPort, 1314);

    MOD_OPTION_1_INT_REG(FestivalDebugSaveOutput, 0);

    MOD_OPTION_1_STR_REG(FestivalStripPunctChars, "");

    MOD_OPTION_1_STR_REG(FestivalRecodeFallback, "?");

    MOD_OPTION_1_INT_REG(FestivalCacheOn, 1);
    MOD_OPTION_1_INT_REG(FestivalCacheMaxKBytes, 5120);
    MOD_OPTION_1_INT_REG(FestivalCacheDistinguishVoices, 0);
    MOD_OPTION_1_INT_REG(FestivalCacheDistinguishRate, 0);
    MOD_OPTION_1_INT_REG(FestivalCacheDistinguishPitch, 0);

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
    if (festival_info == NULL) return -2;

    DBG("FestivalServerHost = %s\n", FestivalServerHost);
    DBG("FestivalServerPort = %d\n", FestivalServerPort);
    
    festival_message = (char**) xmalloc (sizeof (char*));    
    festival_semaphore = module_semaphore_init();
    if (festival_semaphore == NULL) return -1;

    festival_coding = g_strdup("ISO-8859-1");

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
    UPDATE_PARAMETER(volume, festival_set_volume);
    UPDATE_PARAMETER(punctuation_mode, festival_set_punctuation_mode);
    UPDATE_PARAMETER(cap_let_recogn, festival_set_cap_let_recogn);

    DBG("Requested data: |%s| (recoding to %s)\n", data, festival_coding);

    *festival_message = (char*) g_convert_with_fallback(data, bytes, festival_coding, "UTF-8",
                                              FestivalRecodeFallback, NULL, NULL, NULL);
    if (*festival_message == NULL){
        DBG("Error: Recoding unsuccessful.");
        return -1;
    }
    DBG("Requested data after recoding: |%s|\n", *festival_message);
    if(msgtype == MSGTYPE_TEXT){
        module_strip_punctuation_some(*festival_message, FestivalStripPunctChars);
        DBG("Requested after stripping punct: |%s|\n", *festival_message);
    }else{
        DBG("Non-textual event: |%s|\n", *festival_message);
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
    
    module_terminate_thread(festival_speak_thread);
   
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

    cache_init();

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
            bytes = module_get_message_part(message, buf, &pos, FestivalMaxChunkLength, dividers);
        else{
            bytes = module_get_message_part(message, buf, &pos, FestivalMaxChunkLength, "");
        }
        if (current_index_mark != -1) pause = current_index_mark;
        if (bytes == 0){
            if (msgtype == MSGTYPE_TEXT)
                bytes = module_get_message_part(message, buf, &pos, FestivalMaxChunkLength, dividers);
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
                    case MSGTYPE_CHAR:
                    case MSGTYPE_KEY:
                    case MSGTYPE_SOUND_ICON:
                        fwave = cache_lookup(buf, msgtype, 1);
                        if (fwave != NULL){
                            if (fwave->num_samples != 0){
                                ret = module_parent_send_samples(dpipe, fwave->samples,
                                                                 fwave->num_samples, fwave->sample_rate,
								 festival_volume);
                                if (ret == -1) terminate = 1;
                                if(FestivalDebugSaveOutput){
                                    char filename_debug[256];
                                    sprintf(filename_debug, "/tmp/debug-festival-%d.snd", debug_count++);
                                    save_FT_Wave_snd(fwave, filename_debug);
                                }
               
                                DBG("parent (1): Sent %d bytes\n", ret);            
                                ret = module_parent_dp_write(dpipe, "\r\nOK_SPEECHD_DATA_SENT\r\n", 24);
                                if (ret == -1) terminate = 1;

                                if (terminate != 1) terminate = module_parent_wait_continue(dpipe);
                                DBG("End of data in parent (1), closing pipes [%d:%d]", terminate, bytes);
                                module_parent_dp_close(dpipe);
                            }
                            return 0;
                        }
                    }

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
                DBG("Sending buf to child in wav: %d bytes\n",
                    (fwave->num_samples) * sizeof(short));
                if (fwave->num_samples != 0){
                    ret = module_parent_send_samples(dpipe, fwave->samples,
                                                     fwave->num_samples, fwave->sample_rate,
						     festival_volume);
                    if (ret == -1) terminate = 1;
                    if(FestivalDebugSaveOutput){
                        char filename_debug[256];
                        sprintf(filename_debug, "/tmp/debug-festival-%d.snd", debug_count++);
                        save_FT_Wave_snd(fwave, filename_debug);
                    }
               
                    DBG("parent: Sent %d bytes\n", ret);            
                    ret = module_parent_dp_write(dpipe, "\r\nOK_SPEECHD_DATA_SENT\r\n", 24);
                    if (ret == -1) terminate = 1;

                    if (terminate != 1) terminate = module_parent_wait_continue(dpipe);
                }

                if (msgtype == MSGTYPE_CHAR || msgtype == MSGTYPE_KEY ||
                    msgtype == MSGTYPE_SOUND_ICON){
                    DBG("Storing record for %s in cache\n", buf);
                    cache_insert(strdup(buf), msgtype, fwave);
                }else{
                    delete_FT_Wave(fwave);
                }
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
    FestivalSetLanguage(festival_info, language, &festival_coding);
}

void
festival_set_voice(EVoiceType voice)
{
    char* voice_name;

    voice_name = EVoice2str(voice);
    FestivalSetVoice(festival_info, voice_name, &festival_coding);
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
}

int
cache_destroy()
{
    g_hash_table_destroy(FestivalCache.caches);
    g_list_foreach(FestivalCache.cache_counter, cache_free_counter_entry, NULL);
    g_list_free(FestivalCache.cache_counter);
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
}

/* List scores of all entries in the cache*/
void
cache_debug_foreach_list_score(gpointer a, gpointer user)
{
    const TCounterEntry *A = a;
    time_t t;

    t = time(NULL);
    DBG("key: %s      -> score %f (count: %d, dtime: %d)", A->key, ((float) A->count  / (float)(t - A->start)),
        A->count, (t - A->start));
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

    if (type = MSGTYPE_CHAR) ktype = 'c';
    if (type == MSGTYPE_KEY) ktype = 'k';
    if (type == MSGTYPE_SOUND_ICON) ktype = 's';

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
    if (cache_lookup(key, msgtype, 0) != NULL){
        delete_FT_Wave(fwave);
        return 0;
    }

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

    /* Fill the Counter Entry structure that will later allow us to remove
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
    entry->fwave = fwave;

    FestivalCache.size += centry->size;
    g_hash_table_insert(cache, strdup(key), entry);

    return 0;
}

/* Retrieve wave from the cache */
FT_Wave*
cache_lookup(char *key, EMessageType msgtype, int add_counter)
{
    FT_Wave *fwave;
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

    DBG("Cache: corresponding wave found", key);    

    return entry->fwave;
}

#include "module_main.c"
