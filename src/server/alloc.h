
#include "speechd.h"

#ifndef ALLOC_H
 #define ALLOC_H

void* spd_malloc(int bytes);
void spd_free(void *data);

char* spd_strdup(char* string);

TSpeechDQueue* speechd_queue_alloc();					

TSpeechDMessage* spd_message_copy(TSpeechDMessage *old);

void mem_free_message(TSpeechDMessage *msg);

void mem_free_fdset(TFDSetElement *set);


#endif
		
