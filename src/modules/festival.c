/* Speechd module for festival (software synthetizer)
 * CVS revision: $Id: festival.c,v 1.2 2003-03-12 22:23:05 hanke Exp $
 * Author: Hynek Hanke <hanke@volny.cz> */

#define VERSION "0.0.1"

#include <stdio.h>
#include <glib.h>

#include "module.h"
#include "fdset.h"

gint       festival_write      (const gchar *data, gint len, TFDSetElement *);
gint       festival_stop       (void);
gint       festival_is_speaking (void);
gint       festival_close    (void);

/* fill the module_info structure with pointers to this modules functions */
OutputModule modinfo_festival = {
   "festival",
   "Software synthesizer Festival",
   NULL, /* filename */
   festival_write,
   festival_stop,
   festival_is_speaking,
   festival_close
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

gint festival_is_speaking(void) {
   return 0;
}

gint festival_close(void) {
   return 0;
}
