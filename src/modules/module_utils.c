/*
 * module_utils.c - Module utilities
 *           Functions to help writing output modules for Speech Dispatcher
 * Copyright (C) 2003 Brailcom, o.p.s.
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
 * $Id: module_utils.c,v 1.1 2003-06-01 21:17:39 hanke Exp $
 */

static char*
module_getparam_str(GHashTable *table, char* param_name)
{
    char *param;
    param = g_hash_table_lookup(table, param_name);
    return param;
}

static int
module_getparam_int(GHashTable *table, char* param_name)
{
    char *param_str;
    int param;
    
    param_str = module_getparam_str(table, param_name);
    if (param_str == NULL) return -1;
    
    {
      char *tailptr;
      param = strtol(param_str, &tailptr, 0);
      if (tailptr == param_str) return -1;
    }
    
    return param;
}

static char*
module_getvoice(GHashTable *table, char* language, EVoiceType voice)
{
    SPDVoiceDef *voices;
    char *ret;

    voices = g_hash_table_lookup(table, language);
    if (voices == NULL) return NULL;

    switch(voice){
    case MALE1: 
        ret = voices->male1; break;
    case MALE2: 
        ret = voices->male2; break;
    case MALE3: 
        ret = voices->male3; break;
    case FEMALE1: 
        ret = voices->female1; break;
    case FEMALE2: 
        ret = voices->female2; break;
    case FEMALE3: 
        ret = voices->female3; break;
    case CHILD_MALE: 
        ret = voices->child_male; break;
    case CHILD_FEMALE: 
        ret = voices->child_female; break;
    default:
        printf("Unknown voice");
        exit(1);
    }

    if (ret == NULL) ret = voices->male1;
    if (ret == NULL) printf("No voice available for this output module!");

    return ret;
}
