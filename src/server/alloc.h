
#include "speechd.h"

#ifndef ALLOC_H
#define ALLOC_H

/* Copy a message */
TSpeechDMessage *spd_message_copy(TSpeechDMessage * old);

/* Free a message */
void mem_free_message(TSpeechDMessage * msg);

/* Free a settings element */
void mem_free_fdset(TFDSetElement * set);

#endif
