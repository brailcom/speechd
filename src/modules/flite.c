
/*
 * flite.c - Speech Deamon backend for Flite (Festival Lite)
 *
 * Copyright (C) 2001,2002,2003 Ceska organizace pro podporu free software
 * (Czech Free Software Organization), Prague 2, Vysehradska 3/255, 128 00,
 * <freesoft@freesoft.cz>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: flite.c,v 1.5 2003-02-01 22:16:55 hanke Exp $
 */

#define VERSION "0.0.1"

#include <stdio.h>
#include <glib.h>

#include "module.h"
#include "fdset.h"

#define FATAL(dd);

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
   char comm[BUF_SIZE+256];
   FILE *temp;

   printf("flite: write()\n");

   printf("settings: \n");
   printf("  fd: %d\n", set->fd);
//   printf("  language: %s\n", set->language);

   for (i=0; i<len-1; i++) {
      printf("%c",data[i]);
   }
   printf("\n");
//	exit(0);
   data[len-1] = 0;

   sprintf(comm,"flite -t \"%s\" &", data);
   system(comm);
   
//   if((temp = fopen("/tmp/flite_message", "w")) == NULL)
//      FATAL("Output module flite couldn't open temporary file");
//   fprintf(temp,"%s\n\r",data);
//   fclose(temp);
//   system("flite /tmp/flite_message &");
   printf("leaving\n\r");
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
