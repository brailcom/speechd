
/*
 * module.c - Output modules for Speech Dispatcher
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
 * $Id: module.c,v 1.17 2003-09-07 11:28:37 hanke Exp $
 */

#include <sys/wait.h>
#include "speechd.h"

#define INIT_SYMBOL "module_init"
#define LOAD_SYMBOL "module_load"

void
destroy_module(OutputModule *module)
{
    spd_free(module->name);
    spd_free(module->filename);
    spd_free(module->configfilename);
    spd_free(module);
}

OutputModule*
load_output_module(char* mod_name, char* mod_prog, char* mod_cfgfile,
                   EFilteringType mod_filt)
{
    OutputModule *module;
    int fr;
    char *arg1;
    char *arg2;
    int cfg = 0;
    int ret;
    char buf[256];

    if (mod_name == NULL) return NULL;
    
    module = (OutputModule*) spd_malloc(sizeof(OutputModule));

    module->name = (char*) spd_strdup(mod_name);
    module->filename = (char*) spd_get_path(mod_prog, MODULEBINDIR);    
    module->configfilename = (char*) spd_get_path(mod_cfgfile, MODULECONFDIR);
    module->filtering = mod_filt;

    if (!strcmp(mod_name, "testing")){
        module->pipe_in[1] = 1; /* redirect to stdin */
        module->pipe_out[0] = 0; /* redirect to stdout */
        return module;
    }

    if (  (pipe(module->pipe_in) != 0) 
          || ( pipe(module->pipe_out) != 0)  ){
        MSG(3, "Can't open pipe! Module not loaded.");
        return NULL;
    }     

    arg1 = g_strdup_printf("%d", speaking_sem_id);
    if (mod_cfgfile){
        arg2 = g_strdup_printf("%s", module->configfilename);
        cfg=1;
    }

    MSG(5,"Initializing output module %s with binary %s and configuration %s",
        module->name, module->filename, module->configfilename);
    
    fr = fork();
    switch(fr){
    case -1: printf("Can't fork, error! Module not loaded."); return NULL;
    case 0:
        ret = dup2(module->pipe_in[0], 0);
        close(module->pipe_in[0]);
        close(module->pipe_in[1]);
 
        ret = dup2(module->pipe_out[1], 1);
        close(module->pipe_out[1]);
        close(module->pipe_out[0]);        
        
        if (cfg == 0){
            if (execlp(module->filename, "", arg1, (char *) 0) == -1){                                
                exit(1);
            }
        }else{
            if (execlp(module->filename, "", arg1, arg2, (char *) 0) == -1){
                exit(1);
            }            
        }
        assert(0);
    default:
        module->pid = fr;
        close(module->pipe_in[0]);
        close(module->pipe_out[1]);

        usleep(100);            /* So that the other child has at least time to fail
                                   with the execlp */
        ret = waitpid(module->pid, NULL, WNOHANG);
        if (ret != 0){
            MSG(2, "Can't load output module %s. Bad filename in configuration?", module->name);
            destroy_module(module);
            return NULL;
        }
        module->working = 1;
        MSG(2, "Module %s loaded.", module->name);        

        return module;
    }

    assert(0);
}

int
unload_output_module(OutputModule *module)
{
    assert(module != NULL);

    MSG(1,"module name=%s", module->name);

    output_close(module);

    close(module->pipe_in[1]);
    close(module->pipe_out[0]);

    destroy_module(module);    

    return 0;
}

int
reload_output_module(OutputModule *old_module)
{
    OutputModule *new_module;

    assert(old_module != NULL); assert(old_module->name != NULL);

    if (old_module->working) output_module_is_speaking(old_module);
    if (old_module->working) return 0;

    MSG(3, "Reloading output module %s", old_module->name);    

    output_close(old_module);
    close(old_module->pipe_in[1]);
    close(old_module->pipe_out[0]);

    new_module = load_output_module(old_module->name, old_module->filename,
                                    old_module->configfilename, old_module->filtering);
    if (new_module == NULL){
        MSG(3, "Can't load module %s while reloading modules.", old_module->name);
        return -1;
    }

    g_hash_table_replace(output_modules, new_module->name, new_module);
    
    destroy_module(old_module);

    return 0;
}
