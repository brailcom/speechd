/*
The MIT License (MIT)

Copyright © 2026 John Settlemyer

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "kitten.h"

typedef struct {
    char *data;
    size_t size;
} GeneratePayload;

static GMutex model_mutex;
static GMutex stop_mutex;
static GMutex ahead_mutex;
GAsyncQueue *message_queue;
GAsyncQueue *wav_queue;

float ahead_by;

enum wavCommands { BEGIN, STOP, DATA };

typedef struct {
    enum wavCommands cmd;
    GArray *op;
    GString *mark;
} WavPayload;

typedef struct {
    GString *text;
    GString *mark;
} SSMLPayload;

static pthread_t kitten_generation_thread;
static pthread_t kitten_play_wav_thread;

void free_garray(gpointer data) {
    g_array_unref((GArray *)data);
}
void free_string(gpointer data) {
    g_string_free((GString *)data, TRUE);
}

void free_SSMLPayload(gpointer data) {
    SSMLPayload *tmp = (SSMLPayload*) data;
    g_string_free(tmp->mark, TRUE);
    g_string_free(tmp->text, TRUE);
    g_free(tmp);
}

void free_WavPayload(gpointer data){
    WavPayload *wp = (WavPayload*) data;
    if (wp->cmd == DATA){
        g_string_free(wp->mark, TRUE);
        g_array_unref(wp->op);
    }
    g_free(data);
}

void free_GeneratePayload(gpointer data){
    GeneratePayload *tmp = (GeneratePayload *) data;
    g_free(tmp->data);
    g_free(tmp);
}

// changes the voice, the inputs are the same as module_set
// and this should be called from module_set
int model_change_voice(const char *var, const char *val){
    g_mutex_lock(&model_mutex);

    GString *voice_name = g_string_new("");
    GString *voice_quality = g_string_new("");
    const char *allowed_voices[] = {
    "Leo", "Kiki", "Hugo", "Rosie",
    "Bruno", "Luna", "Jasper", "Bella",
    NULL // Required by g_strv_contains
    };

    // Split the string by '_', limit the split to a maximum of 2 tokens
    gchar **tokens = g_strsplit(val, "_", 2);

    if (tokens[0] != NULL && tokens[1] != NULL) {
        // Substring contains '_'
        g_string_assign(voice_name, tokens[0]);
        g_string_assign(voice_quality, tokens[1]);
    } else if (tokens[0] != NULL) {
        // No '_' present in string
        g_string_assign(voice_name, tokens[0]);
        g_string_assign(voice_quality, "Normal");
    }

    // Check if voice_name is in the allowed list if not set to Hugo.
    if (!g_strv_contains(allowed_voices, voice_name->str)) {
        g_string_assign(voice_name, "Hugo");
    }

    g_string_assign(voice, voice_name->str);

    // if the voice quality has changed then we have to reload the correct model and voices bin.
    if (!g_string_equal(voice_quality, voice_setting)) {
        g_string_assign(voice_setting, voice_quality->str);
        if (g_strcmp0(voice_quality->str, "Low") == 0) {
            char *model_filename = file_hash_get_value(files_hash_table, "Low", "model_filename");
            char *voice_filename = file_hash_get_value(files_hash_table, "Low", "voices_filename");
            reload_models_and_voices(model_filename, voice_filename);
        } else if (g_strcmp0(voice_quality->str, "High") == 0) {
            char *model_filename = file_hash_get_value(files_hash_table, "High", "model_filename");
            char *voice_filename = file_hash_get_value(files_hash_table, "High", "voices_filename");
            reload_models_and_voices(model_filename, voice_filename);
        } else {
            g_string_assign(voice_setting, "Normal");
            char *model_filename = file_hash_get_value(files_hash_table, "Normal", "model_filename");
            char *voice_filename = file_hash_get_value(files_hash_table, "Normal", "voices_filename");
            reload_models_and_voices(model_filename, voice_filename);
        }
    }

    // Free allocated memory
    g_strfreev(tokens);
    g_string_free(voice_name, true);
    g_string_free(voice_quality, true);

    g_mutex_unlock(&model_mutex);
}

// changes the speed, the inputs are the same as module_set
// and this should be called from module_set
int model_change_speed(const char *var, const char *val){
    g_mutex_lock(&model_mutex);

    // parse the string into a int
    char *endptr = NULL;
    gint64 int_val = g_ascii_strtoll(val, &endptr, 10);
    int result = (int)int_val;

    // the speed is a multiplier for kitten not a value between -100 and 100
    // so we will map where the lowest value is half speed and the highest is 3X.
    if (result < 0) {
        // Map [-100, 0] -> [0.5, 1.0]
        speed = 1.0f + ((float)result / 100.0f) * 0.5f;
    } else {
        // Map [0, 100] -> [1.0, 3.0]
        speed = 1.0f + ((float)result / 100.0f) * 2.0f;
    }
    g_mutex_unlock(&model_mutex);
}

// adds a wave to our wav_queue
void send_wav(GArray *wav, char *mark){
    WavPayload *wp = g_new(WavPayload, 1);
    wp->cmd = DATA;
    wp->op = wav;
    if (mark == NULL){
        wp->mark = g_string_new("");
    } else {
        wp->mark = g_string_new(mark);
    }
    // everything pushed to the wav_queue should get freed by
    // _play_wav_thread or the free_WavPayload on destroy.
    g_async_queue_push(wav_queue, wp);
}

// add a begin command to the wav_queue
// this will be processed into a module_report_event_begin();
void send_wav_start(){
    WavPayload *wp = g_new(WavPayload, 1);
    wp->cmd = BEGIN;
    // everything pushed to the wav_queue should get freed by
    // _play_wav_thread or the free_WavPayload on destroy.
    g_async_queue_push(wav_queue, wp);
}

// add a begin command to the wav_queue
// this will be processed into a module_report_event_end();
void send_wav_end(){
    WavPayload *wp = g_new(WavPayload, 1);
    wp->cmd = STOP;
    // everything pushed to the wav_queue should get freed by
    // _play_wav_thread or the free_WavPayload on destroy.
    g_async_queue_push(wav_queue, wp);
}

// this gets called on our audio play thread to send out our audio.
void send_samples(short *wav, int len, int rate)
{
	if (!len)
		return;

	AudioTrack track = {
		.bits = 16,
		.num_channels = 1,
		.sample_rate = rate,
		.num_samples = len,
		.samples = wav,
	};
	module_tts_output_server(&track, SPD_AUDIO_LE);
}

void ahead_add(float val){
    g_mutex_lock(&ahead_mutex);
    ahead_by += val;
    g_mutex_unlock(&ahead_mutex);
}

void ahead_set(float val){
    g_mutex_lock(&ahead_mutex);
    ahead_by = val;
    g_mutex_unlock(&ahead_mutex);
}

float ahead_get(){
    float tmp;
    g_mutex_lock(&ahead_mutex);
    tmp = ahead_by;
    g_mutex_unlock(&ahead_mutex);
    return tmp;
}

void ahead_print(){
    fprintf(stderr, "Ahead by %f seconds of audio\n", ahead_get());
}

void stop_set(bool val){
    g_mutex_lock(&stop_mutex);
    stop_generation=val;
    g_mutex_unlock(&stop_mutex);
}

bool stop_get(){
    bool tmp;
    g_mutex_lock(&stop_mutex);
    tmp = stop_generation;
    g_mutex_unlock(&stop_mutex);
    return tmp;
}

/*
we use lib xml to parse the ssml string into chunks containing
the text(usally a sentence) and the mark. These then get passed
into a gqueue for future use and returned by the function.
*/
GQueue* parse_ssml_to_gqueue(const char *data, size_t bytes){
    GQueue *output = g_queue_new();

    // use libxml to parse the ssml data.
    xmlDocPtr doc = xmlReadMemory(data, bytes, "noname.xml", NULL, 0);
    if (doc == NULL) {
        fprintf(stderr, "Failed to parse XML\n");
    }

    xmlNodePtr root = xmlDocGetRootElement(doc); // <speak> node
    xmlBufferPtr buffer = xmlBufferCreate();

    // Iterate through child nodes inside <speak>
    for (xmlNodePtr cur = root->children; cur != NULL; cur = cur->next) {
        if (cur->type == XML_TEXT_NODE) {
            // Append text content to buffer
            xmlBufferCat(buffer, cur->content);
        }
        else if (cur->type == XML_ELEMENT_NODE && xmlStrcmp(cur->name, (const xmlChar *)"mark") == 0) {

            xmlChar *mark_name = xmlGetProp(cur, (const xmlChar *)"name");

            SSMLPayload *pl = g_new(SSMLPayload,1);

            pl->mark = g_string_new(mark_name);
            pl->text = g_string_new(buffer->content);

            g_queue_push_tail(output,pl);

            // Clean up the attribute memory and reset the buffer for the next segment
            if (mark_name) xmlFree(mark_name);
            xmlBufferEmpty(buffer);

        }
    }
    // add any trailing text after the final mark.
    if (buffer->use > 0) {
        SSMLPayload *pl = g_new(SSMLPayload,1);

        pl->mark = g_string_new("");
        pl->text = g_string_new(buffer->content);

        g_queue_push_tail(output, pl);
    }

    xmlBufferFree(buffer);
    xmlFreeDoc(doc);
    xmlCleanupParser();

    return output;
}

/**
 * Splits a GString into chunks containing at most max_utf8_chars UTF-8 characters.
 * Returns a GPtrArray of GString pointers containing valid UTF-8 strings.
 */
GPtrArray* split_gstring_utf8(const GString *input_str, glong max_utf8_chars) {
    GPtrArray *chunks = g_ptr_array_new_with_free_func((GDestroyNotify)g_string_free);

    if (!input_str || input_str->len == 0 || max_utf8_chars <= 0) {
        return chunks;
    }

    const gchar *start = input_str->str;
    const gchar *end = input_str->str + input_str->len;

    while (start < end) {
        // Find the pointer position 'max_utf8_chars' ahead (or stop at 'end' if fewer remain)
        const gchar *next_boundary = g_utf8_offset_to_pointer(start, max_utf8_chars);

        // Ensure we don't go past the end of the string buffer
        if (next_boundary > end) {
            next_boundary = end;
        }

        // Calculate exact byte length for this UTF-8 safe slice
        gsize byte_len = next_boundary - start;

        // Create the chunk
        GString *chunk = g_string_new_len(start, byte_len);
        g_ptr_array_add(chunks, chunk);

        // Move start pointer forward
        start = next_boundary;
    }

    return chunks;
}

int model_generate_speech(const char *data, size_t bytes){
    g_mutex_lock(&model_mutex);

    //The ssml is parsed using libxml and then returned in a GQueue.
    GQueue *ssml_queue = parse_ssml_to_gqueue(data, bytes);

    fprintf(stderr, "speaking '%s'\n", data);
    fprintf(stderr, "using voice %s and speed %f\n", voice->str, speed);

    stop_set(false); // make sure that stop generation has been reset to false.

    ahead_set(0.0); // number of seconds the generation is ahead of the played audio.

    send_wav_start();

    while (!g_queue_is_empty(ssml_queue)) {
        SSMLPayload *item = g_queue_pop_head(ssml_queue);

        // Split into chunks of max 399 UTF-8 characters
        // our model can not handle anything that is larger.
        const glong MAX_CHARS = 399;
        GPtrArray *chunks = split_gstring_utf8(item->text, MAX_CHARS);

        for (guint i = 0; i < chunks->len; i++) {
            GString *chunk = g_ptr_array_index(chunks, i);

            clock_t gen = clock();
            GArray *op = kitten_speak(chunk->str);
            clock_t end = clock();
            double seconds = (double)(end - gen) / CLOCKS_PER_SEC;
            fprintf(stderr, "Generated %f seconds of audio in %f seconds\n", op->len/24000.0, seconds);

            //The op array will be freed by the _generation_thread or free_GeneratePayload on destroy.
            if (i == chunks->len){
                send_wav(op, item->mark->str); // on the last chunk send the mark also.
            } else {
                send_wav(op, NULL);
            }

            ahead_add(op->len/24000.0);
            ahead_print();
        }

        g_ptr_array_free(chunks, TRUE);
        free_SSMLPayload(item);

        // if we get more then a 90 seconds ahead then block the thread,
        // no need to blow up the cpu on super long texts.
        float last_ahead = ahead_get();
        while (last_ahead >= 90.0){
            fprintf(stderr, "ahead 90sec, blocking\n");
            sleep(20);// 20 seconds.
            last_ahead = ahead_get();

            if (stop_get())
                break;
        }

        // break the loop if we get a stop event.
        if (stop_get())
            break;
    }

    send_wav_end();

    // Cleanup
    g_queue_free_full(ssml_queue, free_SSMLPayload);

    g_mutex_unlock(&model_mutex);
    return 0;
}

// this thread loop handles the generation of audio.
void *_generation_thread(void *nothing)
{
	while (1) {
        // use a async queue to handle signalling.
        GeneratePayload *message_payload = (GeneratePayload *) g_async_queue_pop(message_queue);// this is thread safe and will block if nothing in the queue.
        fprintf(stderr,"thread loop: got message: size %d\n",message_payload->size);

        model_generate_speech(message_payload->data, message_payload->size);

        free_GeneratePayload(message_payload);
	}

	pthread_exit(NULL);
}

// this thread loop handles sendding the audio out
// since sending the samples can block,
// it needs to be on its own thread so we don't block
// the model generation thread.
void *_play_wav_thread(void *nothing)
{
	while (1) {
        // use a async queue to handle signalling.
        WavPayload *wp = (WavPayload*) g_async_queue_pop(wav_queue);// this is thread safe and will block if nothing in the queue.
        fprintf(stderr,"wav_thread: got payload\n");

        if (wp->cmd == BEGIN){
            fprintf(stderr,"wav_thread: module_report_event_begin\n");
            module_report_event_begin();
        } else if (wp->cmd == STOP){
            fprintf(stderr,"wav_thread: module_report_event_end\n");
            module_report_event_end();
        } else if (wp->cmd == DATA){
            fprintf(stderr,"wav_thread: send_samples\n");
            GArray *op = (GArray *) wp->op;
            GString *mark = wp->mark;
            send_samples((short*)op->data, op->len, 24000);
            ahead_add(-1 * ((float)op->len/24000.0));
            if (mark->len != 0){
                module_report_index_mark(mark->str);
            }
        }
        free_WavPayload(wp);
	}

	pthread_exit(NULL);
}

// this pushs a message that will be spoken
// this should be called from module_speak
int add_generate_speech_task(const char* data, size_t bytes) {
    GeneratePayload *message_payload;
    message_payload = g_new(GeneratePayload, 1);

    message_payload->data = g_memdup2(data, bytes);
    message_payload->size = bytes;
    // everything pushed to the message_queue should get freed by
    // _generation_thread or the free_GeneratePayload on destroy.
    g_async_queue_push(message_queue, message_payload);

    return 0;
}

// this will stop our generation
// its called from module_pause and module_stop
int model_stop_generation(){
    stop_set(true);
    return 0;
}

// This is the same as spd_pthread_create, I move it in here because including $(common_SOURCES) was creating circular dependences for me.
int kitty_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg)
{
	int retsig, ret;
	sigset_t all_signals;
	sigset_t old_signals;

	retsig = sigfillset(&all_signals);
	if (retsig != 0)
		fprintf(stderr, "Can't fill signal set (%d), expect problems when terminating!\n", retsig);
	else {
		retsig = pthread_sigmask(SIG_BLOCK, &all_signals, &old_signals);
		if (retsig != 0)
			fprintf(stderr, "Can't set signal set (%d), expect problems when terminating!\n", retsig);
	}

	ret = pthread_create(thread, attr, start_routine, arg);

	if (retsig == 0)
		pthread_sigmask(SIG_SETMASK, &old_signals, NULL);

	return ret;
}

int init_model_thread_pool(){
    fprintf(stderr, "init_model_thread_pool();\n");
    GError *pool_error = NULL;

    g_mutex_init(&model_mutex);
    g_mutex_init(&stop_mutex);
    g_mutex_init(&ahead_mutex);

    message_queue = g_async_queue_new_full((GDestroyNotify) free_GeneratePayload);
    wav_queue = g_async_queue_new_full((GDestroyNotify) free_WavPayload);

    // lock the model mutex while loading
    g_mutex_lock(&model_mutex);

    // default the voice to hugo
    voice = g_string_new("Hugo");
    // default to the micro model it seems to be the best combo of quality and speed for me.
    voice_setting = g_string_new("Normal");

    if (init_paths() == -1){
        g_mutex_unlock(&model_mutex);
        return -1;
    }

    if (kitty_pthread_create(&kitten_generation_thread, NULL, _generation_thread, NULL) != 0){
        fprintf(stderr, "Error: Creating _generation_thread()\n");
        g_mutex_unlock(&model_mutex);
        return -1;
    }

    if (kitty_pthread_create(&kitten_play_wav_thread, NULL, _play_wav_thread, NULL) != 0){
        fprintf(stderr, "Error: Creating _play_wav_thread()\n");
        g_mutex_unlock(&model_mutex);
        return -1;
    }

    if (init_voice_style(voices_path->str) == -1){
        fprintf(stderr, "Error: Initializing voice style\n");
        g_mutex_unlock(&model_mutex);
        return -1;
    };

    if (init_model(model_path->str) == -1){
        fprintf(stderr, "Error Initializing model/onnx\n");
        g_mutex_unlock(&model_mutex);
        return -1;
    }

    g_mutex_unlock(&model_mutex);

    return 0;
}

int cleanup_threads(){

    // another thread might be using this data
    // wait tell its finished before cleaning up.
    g_mutex_lock(&model_mutex);

    cleanup_voice_style();
    cleanup_model();

    g_string_free(model_dir, TRUE);
    g_string_free(model_path, TRUE);
    g_string_free(voices_path, TRUE);
    g_string_free(voice_setting, TRUE);
    g_string_free(voice, TRUE);
    g_string_free(distro_target_subdir, TRUE);

    g_mutex_unlock(&model_mutex);

    //free the mutex's and queues.
    g_mutex_clear(&model_mutex);
    g_mutex_clear(&stop_mutex);
    g_mutex_clear(&ahead_mutex);

    g_async_queue_unref(wav_queue);
    g_async_queue_unref(message_queue);

    g_hash_table_destroy(files_hash_table);
}