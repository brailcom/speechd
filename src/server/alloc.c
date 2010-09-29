
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "alloc.h"

TFDSetElement
spd_fdset_copy(TFDSetElement old)
{
    TFDSetElement new;

    new = old;
    new.language = g_strdup(old.language);
    new.synthesis_voice = g_strdup(old.synthesis_voice);
    new.client_name = g_strdup(old.client_name);
    new.output_module = g_strdup(old.output_module);
    new.index_mark = g_strdup(old.index_mark);
    new.audio_output_method = g_strdup(old.audio_output_method);
    new.audio_oss_device = g_strdup(old.audio_oss_device);
    new.audio_alsa_device = g_strdup(old.audio_alsa_device);
    new.audio_nas_server = g_strdup(old.audio_nas_server);
    new.audio_pulse_server = g_strdup(old.audio_pulse_server);

    return new;
    
    
}

TSpeechDMessage*
spd_message_copy(TSpeechDMessage *old)
{
	TSpeechDMessage* new = NULL;

        if (old == NULL) return NULL;

	new = (TSpeechDMessage *) g_malloc(sizeof(TSpeechDMessage));

	*new = *old;
	new->buf = g_malloc((old->bytes+1) * sizeof(char));
	memcpy(new->buf, old->buf, old->bytes);
	new->buf[new->bytes] = 0;	
	new->settings = spd_fdset_copy(old->settings); 

	return new;
}

void 
mem_free_fdset(TFDSetElement *fdset)
{
    /* Don't forget that only these items are filled in
       in a TSpeechDMessage */
    g_free(fdset->client_name);
    g_free(fdset->language);
    g_free(fdset->synthesis_voice);
    g_free(fdset->output_module);
    g_free(fdset->index_mark);
    g_free(fdset->audio_output_method);
    g_free(fdset->audio_oss_device);
    g_free(fdset->audio_alsa_device);
    g_free(fdset->audio_nas_server);
    g_free(fdset->audio_pulse_server);
}

void
mem_free_message(TSpeechDMessage *msg)
{
    if (msg == NULL) return;
    g_free(msg->buf);
    mem_free_fdset(&(msg->settings));
    g_free(msg);
}


