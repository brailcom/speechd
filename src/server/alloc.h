
#ifndef ALLOC_H
 #define ALLOC_H

#include "speechd.h"

void* spd_malloc(long int bytes);

TSpeechDQueue* speechd_queue_alloc();					

void* fdset_list_alloc_element(void* element);

TSpeechDMessage* history_list_new_message(TSpeechDMessage *old);

void mem_free_message(TSpeechDMessage *msg);

void mem_free_fdset(TFDSetElement *set);


#endif
		
