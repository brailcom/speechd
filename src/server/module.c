#include "speechd.h"

#define INIT_SYMBOL "module_init"

OutputModule* load_output_module(gchar* modname) {
   GModule* gmodule;
   OutputModule *(*module_init) (void);
   OutputModule *module_info;
   char filename[PATH_MAX];

   snprintf(filename, PATH_MAX, "/usr/local/lib/libsd%s.so", modname);

   gmodule = g_module_open(filename, G_MODULE_BIND_LAZY);
   if (gmodule == NULL) {
      MSG(0,"failed to load module %s\n%s\n", filename, g_module_error());
      return NULL;
   }

   if (g_module_symbol(gmodule, INIT_SYMBOL, (gpointer) &module_init) == FALSE) {
      MSG(0,"could not find symbol \"%s\" in module %s\n%s\n", INIT_SYMBOL, g_module_name(gmodule), g_module_error());
      return NULL;
   }
   
   module_info = module_init();
   if (module_info == NULL) {
      MSG(0, "module entry point execution failed\n");
      return NULL;
   }

//   gdsl_list_insert_head(output_queues, (void *) modname); 

   MSG(2,"loaded module: %s (%s)\n", module_info->name, module_info->description);

   return module_info;
}


