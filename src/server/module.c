
/*
 * module.c - Output modules for Speech Deamon
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
 * $Id: module.c,v 1.9 2003-03-25 22:48:53 hanke Exp $
 */

#include "speechd.h"

#define INIT_SYMBOL "module_init"

OutputModule* load_output_module(gchar* modname) {
   GModule* gmodule;
   OutputModule *(*module_init) (void);
   OutputModule *module_info;
   char filename[PATH_MAX];

   snprintf(filename, PATH_MAX, LIB_DIR"/libsd%s.so", modname);

   gmodule = g_module_open(filename, G_MODULE_BIND_LAZY);
   if (gmodule == NULL) {
      MSG(2,"failed to load module %s\n%s\n", filename, g_module_error());
      return NULL;
   }

   if (g_module_symbol(gmodule, INIT_SYMBOL, (gpointer) &module_init) == FALSE) {
      MSG(2,"could not find symbol \"%s\" in module %s\n%s\n", INIT_SYMBOL, g_module_name(gmodule), g_module_error());
      return NULL;
   }
   
   module_info = module_init();
   if (module_info == NULL) {
      MSG(2, "module entry point execution failed\n");
      return NULL;
   }

   module_info->gmodule = gmodule;
   
   MSG(2,"loaded module: %s (%s)", module_info->name, module_info->description);

   return module_info;
}


