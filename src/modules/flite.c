/* Speechd module for flite (software synthetizer)
 * CVS revision: $Id: flite.c,v 1.1 2002-06-30 00:23:32 hanke Exp $
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
        char *output_module;    // output module: flite, odmluva, epos,...
        EVoiceType voice_type;  // see EVoiceType definition
        int spelling;           // spelling mode: 0 -- off, 1 -- on
        int cap_let_recogn;     // cap. letters recogn.: 0 -- off, 1 -- on
}TFDSetElement;


gint       flite_write      (const gchar *data, gint len, TFDSetElement *);
gint       flite_stop       (void);
gint       flite_pause      (void);
gint       flite_release    (void);
gint       flite_is_speaking (void);

/* fill the module_info structure with pointers to this modules functions */
OutputModule modinfo_flite = {
   "flite",
   "Software synthetizer Flite",
   NULL, /* filename */
   flite_write,
   flite_stop,
   flite_pause,
   flite_release,
   flite_is_speaking
};

/* entry point of this module */
OutputModule *module_init(void) {
   printf("flite: init_module()\n");

   /*modinfo.name = g_strdup("flite"),
   modinfo_flite.description = g_strdup_printf(
	"Flite software synthesizer, version %s",VERSION);*/
   return &modinfo_flite;
}

/* module operations */
gint flite_write(const gchar *data, gint len, TFDSetElement* set) {
   int i;
//   char comm[BUF_SIZE+256];
   FILE *temp;

   printf("flite: write()\n");

   printf("settings: \n");
   printf("  fd: %d\n", set->fd);
//   printf("  language: %s\n", set->language);

//   for (i=0; i<len; i++) {
//      printf("%c",data[i]);
//   }
//   printf("\n");
	
   data[len] = 0;

   temp = fopen("/tmp/flite_message", "w");
   fprintf(temp,"%s",data);
   fclose(temp);
   system("flite /tmp/flite_message &");
   return len;
}

gint flite_stop(void) {
   printf("flite: stop()\n");
   system("killall flite");
   return 1;
}

gint flite_pause(void) {
   printf("flite: pause()\n");
   return 1;
}

gint flite_release(void) {
   printf("flite: release()\n");
   return 1;
}

gint flite_is_speaking(void) {
   int ret;
   ret = system("pgrep flite > /dev/null"); 
   if (ret == 0) return 1;
   return 0;
}
