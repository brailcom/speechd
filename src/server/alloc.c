
#include "alloc.h"

void*
spd_malloc(size_t bytes)
{
	void *mem;

	mem = malloc(bytes);
	if(mem == NULL) FATAL("Can't allocate memmory.\n");

	return mem;
}

void
spd_free(void* data)
{
    if (data != NULL) free(data);
}

char*
spd_strdup(char* string)
{
    char *newstr;
    
    if (string == NULL) return NULL;

    newstr = strdup(string);
    if (newstr == NULL) 
        FATAL("Can't duplicate a string of characters, not enough memmory");

    return newstr;
}

void
spd_module_free(OutputModule *module)
{
    if (module->settings.voices != NULL)
        g_hash_table_destroy(module->settings.voices);

    /* This must be the last action here as it
       destroys the whole module pointer */
    g_module_close(module->gmodule);
}

TSpeechDQueue* 
speechd_queue_alloc()
{
	TSpeechDQueue *new;
	
	new = spd_malloc(sizeof(TSpeechDQueue));

        /* Initialize all the queues to be empty */
	new->p1 = NULL;
	new->p2 = NULL;
	new->p3 = NULL;
        new->p4 = NULL;
        new->p5 = NULL;

	return(new);
}

TSpeechDMessage*
spd_message_copy(TSpeechDMessage *old)
{
	TSpeechDMessage* new = NULL;

        if (old == NULL) return NULL;

	new = (TSpeechDMessage *) spd_malloc(sizeof(TSpeechDMessage));

	*new = *old;

	new->buf = spd_malloc((old->bytes+1) * sizeof(char));
	memcpy(new->buf, old->buf, old->bytes);
	new->buf[new->bytes] = 0;

	new->settings = old->settings; 

	new->settings.language = spd_strdup(old->settings.language);
	new->settings.client_name = spd_strdup(old->settings.client_name);
	new->settings.output_module = spd_strdup(old->settings.output_module);

	return new;
}

void 
mem_free_fdset(TFDSetElement *fdset)
{
    /* Don't forget that only these items are filled in
       in a TSpeechDMessage */
    spd_free(fdset->client_name);
    spd_free(fdset->language);
    spd_free(fdset->output_module);
}

void
mem_free_message(TSpeechDMessage *msg)
{
    if (msg == NULL) return;
    spd_free(msg->buf);
    mem_free_fdset(&(msg->settings));
    spd_free(msg);
}


