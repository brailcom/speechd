
#include "alloc.h"

void*
spd_malloc(long int bytes)
{
	void *mem;

	mem = malloc(bytes);
	if(mem == NULL) FATAL("Can't allocate memmory.\n");

	return mem;
}

TSpeechDQueue* 
speechd_queue_alloc()
{
	TSpeechDQueue *new;
	
	new = spd_malloc(sizeof(TSpeechDQueue));
	new->p1 = NULL;
	new->p2 = NULL;
	new->p3 = NULL;

	return(new);
}

void*
fdset_list_alloc_element(void* element)
{
	TFDSetElement *new;

	new = spd_malloc(sizeof(TFDSetElement));
	*new = *((TFDSetElement*) element);

	new->language = spd_malloc( strlen(((TFDSetElement*) element)->language) );
	strcpy(new->language, ((TFDSetElement*) element)->language);

	new->client_name = spd_malloc( strlen(((TFDSetElement*) element)->client_name) );
	strcpy(new->client_name, ((TFDSetElement*) element)->client_name);

	new->output_module = spd_malloc( strlen(((TFDSetElement*) element)->output_module) );
	strcpy(new->output_module, ((TFDSetElement*) element)->output_module);

	return (void*) new;
}

TSpeechDMessage*
history_list_new_message(TSpeechDMessage *old)
{
	TSpeechDMessage* new = NULL;

	new = (TSpeechDMessage *) spd_malloc(sizeof(TSpeechDMessage));
	new->buf = spd_malloc(old->bytes+1);
	new->settings.language = (char*) spd_malloc(strlen(old->settings.language) + 1);
	new->settings.client_name = (char*) spd_malloc(strlen(old->settings.client_name) + 1);
	new->settings.output_module = (char*) spd_malloc(strlen(old->settings.output_module) + 1);

	/* Copy the content */
	*new = *old;
	memcpy(new->buf, old->buf, old->bytes);
	(TFDSetElement) new->settings = (TFDSetElement) old->settings; 
	strcpy(new->settings.language, old->settings.language);
	strcpy(new->settings.client_name, old->settings.client_name);
	strcpy(new->settings.output_module, old->settings.output_module);

	return new;
}

THistoryClient*
history_list_create_client(int fd)
{
	THistoryClient *new;
	TFDSetElement *settings;
	GList *gl;
	
	settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &fd);
	if (settings == NULL) FATAL("Couldn't find appropiate settings for active client."); 	

	new = spd_malloc(sizeof(THistoryClient));
	new->client_name = spd_malloc(strlen(settings->client_name)+1);

	strcpy(new->client_name, settings->client_name);
	new->fd = fd;
	new->uid = settings->uid;
	new->active = 1;
	new->messages = NULL;

	return new;
}

void
mem_free_message(TSpeechDMessage *msg)
{
	free(msg->buf);
	free(msg->settings.client_name);
	free(msg->settings.language);
	free(msg->settings.output_module);
	free(msg);
}

void
mem_free_fdset(TFDSetElement *fdset){
	free(fdset->client_name);
	free(fdset->language);
	free(fdset->output_module);
	free(fdset);
}
