
/*
 * module.c - Output modules for Speech Deamon
 *
 * Copyright (C) 2001, 2002, 2003 Brailcom, o.p.s.
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
 * $Id: module.c,v 1.11 2003-05-18 20:56:35 hanke Exp $
 */

#include "speechd.h"

#define INIT_SYMBOL "module_init"
#define LOAD_SYMBOL "module_load"

OutputModule*
load_output_module(gchar* modname)
{
   GModule* gmodule;
   OutputModule *(*module_load) (void);
   OutputModule *module_info;
   char filename[PATH_MAX];

   snprintf(filename, PATH_MAX, LIB_DIR"/libsd%s.so", modname);

   gmodule = g_module_open(filename, G_MODULE_BIND_LAZY);
   if (gmodule == NULL) {
      MSG(2,"failed to load module %s\n%s\n", filename, g_module_error());
      return NULL;
   }

   if (g_module_symbol(gmodule, LOAD_SYMBOL, (gpointer) &module_load) == FALSE) {
      MSG(2,"could not find symbol \"%s\" in module %s\n%s\n", LOAD_SYMBOL, g_module_name(gmodule), g_module_error());
      return NULL;
   }
   
   module_info = module_load();
   if (module_info == NULL) {
      MSG(2, "module entry point execution failed\n");
      return NULL;
   }

   module_info->gmodule = gmodule;
   
   MSG(2,"loaded module: %s (%s)", module_info->name, module_info->description);

   return module_info;
}

int
init_output_module(OutputModule *module)
{
    int ret;
    int (*module_init) (void);

    if (g_module_symbol(module->gmodule, INIT_SYMBOL, (gpointer) &module_init) == FALSE) {
        MSG(2,"could not find symbol \"%s\" in module %s\n%s\n", INIT_SYMBOL, g_module_name(module->gmodule), g_module_error());
        return -1;
    }

    ret = module_init();

    if (ret == 0) MSG(4, "Module init ok.");
    else MSG(4, "Module init failed!");

    return ret;
}


