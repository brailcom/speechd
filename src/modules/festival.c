/* Speechd module for festival (software synthetizer)
 * CVS revision: $Id: festival.c,v 1.1 2002-06-30 00:19:25 hanke Exp $
 * Author: Hynek Hanke <hanke@volny.cz> */

#define VERSION "0.0.1"

#include <stdio.h>
#include <glib.h>

#include "module.h"

typedef enum {                  // type of voice
        MALE = 0,               // (numbers divisible by 2 should mean male)
        FEMALE = 1,
        CHILD_MALE = 2,
        CHILD_FEMALE = 3
}EVoiceType;

typedef struct{
        int fd;
        int priority;           // priority between 1 and 3
        int punctuation_mode;   // this will of course not be integer
        int speed;              // speed: 100 = normal, ???
        float pitch;            // pitch: ???
        char *language;         // language: default = english
        char *output_module;    // output module: festival, odmluva, epos,...
        EVoiceType voice_type;  // see EVoiceType definition
        int spelling;           // spelling mode: 0 -- off, 1 -- on
        int cap_let_recogn;     // cap. letters recogn.: 0 -- off, 1 -- on
}TFDSetElement;


gint       festival_write      (const gchar *data, gint len, TFDSetElement *);
gint       festival_stop       (void);
gint       festival_pause      (void);
gint       festival_release    (void);
gint       festival_is_speaking (void);

/* fill the module_info structure with pointers to this modules functions */
OutputModule modinfo_festival = {
   "festival",
   "Software synthesizer Festival",
   NULL, /* filename */
   festival_write,
   festival_stop,
   festival_pause,
   festival_release,
   festival_is_speaking
};

/* entry point of this module */
OutputModule *module_init(void) {
   printf("festival: init_module()\n");

   /*modinfo.name = g_strdup("festival"),
   modinfo_festival.description = g_strdup_printf(
	"Festival software synthesizer, version %s",VERSION);*/
   return &modinfo_festival;
}

/* module operations */
gint festival_write(const gchar *data, gint len, TFDSetElement* set) {
   int i;
   char comm[256];

   printf("festival: write()\n");

   for (i=0; i<len; i++) {
      printf("%c ",data[i]);
   }
   printf("\n");
	
   data[len] = 0;
   sprintf(comm,"echo \"(SayText\\\"%s\\\")\" | festival_client &",data);
	system(comm);

   return len;
}

gint festival_stop(void) {
   printf("festival: stop()\n");
   system("killall festival_client");
   return 1;
}

gint festival_pause(void) {
   printf("festival: pause()\n");
   return 1;
}

gint festival_release(void) {
   printf("festival: release()\n");
   return 1;
}

gint festival_is_speaking(void) {
   return 0;
}
