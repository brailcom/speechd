
/*
 * dc_decl.h - Dotconf functions and types for Speech Deamon
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
 * $Id: dc_decl.h,v 1.11 2003-04-17 10:16:31 hanke Exp $
 */

#include "speechd.h"

/* define dotconf callbacks */
DOTCONF_CB(cb_LogFile);
DOTCONF_CB(cb_AddModule);
DOTCONF_CB(cb_AddTable);
DOTCONF_CB(cb_DefaultModule);
DOTCONF_CB(cb_DefaultSpeed);
DOTCONF_CB(cb_DefaultPitch);
DOTCONF_CB(cb_DefaultLanguage);
DOTCONF_CB(cb_DefaultPriority);
DOTCONF_CB(cb_DefaultPunctuationMode);
DOTCONF_CB(cb_DefaultPunctuationTable);
DOTCONF_CB(cb_PunctuationSome);
DOTCONF_CB(cb_DefaultSpellingTable);
DOTCONF_CB(cb_DefaultClientName);
DOTCONF_CB(cb_DefaultVoiceType);
DOTCONF_CB(cb_DefaultSpelling);
DOTCONF_CB(cb_DefaultSpellingTable);
DOTCONF_CB(cb_DefaultCapLetRecognition);


/* define dotconf configuration options */
static const configoption_t options[] =
{
    {"LogFile", ARG_STR, cb_LogFile, 0, 0},
    {"AddModule", ARG_STR, cb_AddModule, 0, 0},
    {"AddTable", ARG_STR, cb_AddTable, 0, 0},
    {"DefaultModule", ARG_STR, cb_DefaultModule, 0, 0},
    {"DefaultSpeed", ARG_INT, cb_DefaultSpeed, 0, 0},
    {"DefaultPitch", ARG_INT, cb_DefaultPitch, 0, 0},
    {"DefaultLanguage", ARG_STR, cb_DefaultLanguage, 0, 0},
    {"DefaultPriority", ARG_INT, cb_DefaultPriority, 0, 0},
    {"DefaultPunctuationMode", ARG_STR, cb_DefaultPunctuationMode, 0, 0},
    {"DefaultPunctuationTable", ARG_STR, cb_DefaultPunctuationTable, 0, 0},
    {"PunctuationSome", ARG_STR, cb_PunctuationSome, 0, 0},
    {"DefaultClientName", ARG_STR, cb_DefaultClientName, 0, 0},
    {"DefaultVoiceType", ARG_INT, cb_DefaultVoiceType, 0, 0},
    {"DefaultSpelling", ARG_TOGGLE, cb_DefaultSpelling, 0, 0},
    {"DefaultSpellingTable", ARG_STR, cb_DefaultSpellingTable, 0, 0},
    {"DefaultCapLetRecognition", ARG_TOGGLE, cb_DefaultCapLetRecognition, 0, 0},
    /*{"ExampleOption", ARG_STR, cb_example, 0, 0},
     *      {"MultiLineRaw", ARG_STR, cb_multiline, 0, 0},
     *           {"", ARG_NAME, cb_unknown, 0, 0},
     *                {"MoreArgs", ARG_LIST, cb_moreargs, 0, 0},*/
    LAST_OPTION
};

DOTCONF_CB(cb_LogFile)
{
    assert(cmd->data.str != NULL);
    if (!strncmp(cmd->data.str,"stdout",6)){
        logfile = (FILE*) malloc(sizeof(FILE*));
        logfile = stdout;
        return NULL;
    }
    if (!strncmp(cmd->data.str,"stderr",6)){
        logfile = (FILE*) malloc(sizeof(FILE*));
        logfile = stderr;
        return NULL;
    }
    logfile = fopen(cmd->data.str, "w");
    if (logfile == NULL){
        printf("Error: can't open logging file! Using stdout.\n");
        logfile = (FILE*) malloc(sizeof(FILE*));
        logfile = stdout;
    }
    MSG(2,"Speech Deamon Logging to file %s", cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_AddModule)
{
    OutputModule *om;
    om = load_output_module(cmd->data.str);
    g_hash_table_insert(output_modules, om->name, om);
    return NULL;
}

DOTCONF_CB(cb_AddTable)
{
    FILE *icons_file;
    char *language;
    char *tablename;
    GHashTable *icons_hash;
    char *helper;
    char *key;
    char *character;
    char *value;
    char *filename;
    char *line;
	
    MSG(4,"Reading sound icons file...");
	
    language = NULL;
    tablename = NULL;
    line = (char*) spd_malloc(256);
    filename = (char*) spd_malloc(256);
	
    sprintf(filename,SYS_CONF"/%s", cmd->data.str);
    MSG(4, "Reading table from file %s", filename);
    if((icons_file = fopen(filename, "r"))==NULL){
        MSG(2,"Table %s file specified in speechd.conf doesn't exist!", filename);
        return NULL;	  
    }

    while(1){
        line = (char*) spd_malloc(256 * sizeof(char));
        fgets(line, 254, icons_file);
        if(line == NULL){
            MSG(2, "Specified table %s empty or missing language and table identification.", filename);
            return NULL;
        }
        if(strlen(line) <= 2) continue;
        if ((line[0] == '#') && (line[1] == ' ')) continue;

        key = strtok(line,":\r\n");
        if(key == NULL) continue;
        value = strtok(NULL,"=\r\n");
        if(value == NULL) continue;
        g_strstrip(key);
        g_strstrip(value);

        if(!strcmp(key,"language")) language = value;
        if(!strcmp(key,"table")) tablename = value;

        if((language != NULL) && (tablename!=NULL)) break;
    }
    MSG(4, "Parsing icons data: lang:%s name:%s", language, tablename);
    if(strlen(language)<2){
        MSG(2,"Invalid language code in table.");
        return NULL;
    }

    icons_hash = g_hash_table_lookup(snd_icon_langs, language);
    if(!icons_hash){
        icons_hash = g_hash_table_new(g_str_hash, g_str_equal);
        g_hash_table_insert(snd_icon_langs, language, icons_hash);
    }
    
    while(1){
        helper = (char*) spd_malloc(256*sizeof(char));
        if(fgets(helper, 255, icons_file) == NULL) break;
        character = strtok(helper,"=\r\n");
	if (character == NULL) continue;
        g_strstrip(character);
        key = malloc((strlen(character) + strlen(tablename) + 2) * sizeof(char));
        sprintf(key, "%s_%s", tablename, character);
        value = strtok(NULL,"\r\n");
        if(value == NULL) continue;
        g_strstrip(value);
        g_hash_table_insert(icons_hash, key, value);
    }

    return NULL;
}

DOTCONF_CB(cb_DefaultModule)
{
    GlobalFDSet.output_module = malloc(sizeof(char) * strlen(cmd->data.str) + 1);
    strcpy(GlobalFDSet.output_module,cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_DefaultSpeed)
{
    GlobalFDSet.speed = cmd->data.value;
    return NULL;
}

DOTCONF_CB(cb_DefaultPitch)
{
    GlobalFDSet.pitch = cmd->data.value;
    return NULL;
}

DOTCONF_CB(cb_DefaultLanguage)
{
    GlobalFDSet.language = malloc(sizeof(char) * strlen(cmd->data.str) + 1);
    strcpy(GlobalFDSet.language,cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_DefaultPriority)
{
    GlobalFDSet.priority = cmd->data.value;
    return NULL;
}

DOTCONF_CB(cb_DefaultPunctuationMode)
{
    assert(cmd->data.str != NULL);
    if(!strcmp(cmd->data.str, "all")) GlobalFDSet.punctuation_mode = 1;
    else if(!strcmp(cmd->data.str, "none")) GlobalFDSet.punctuation_mode = 0;
    else if(!strcmp(cmd->data.str, "some")) GlobalFDSet.punctuation_mode = 2;
    return NULL;
}

DOTCONF_CB(cb_PunctuationSome)
{
    assert(cmd->data.str != NULL);
    GlobalFDSet.punctuation_some = (char*) spd_malloc(sizeof(char) * (strlen(cmd->data.str) + 1));
    strcpy(GlobalFDSet.punctuation_some, cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_DefaultPunctuationTable)
{
    GlobalFDSet.punctuation_table = (char*) spd_malloc((strlen(cmd->data.str) + 1) * sizeof(char));
    strcpy(GlobalFDSet.punctuation_table, cmd->data.str);
    return NULL;
}


DOTCONF_CB(cb_DefaultClientName)
{
    GlobalFDSet.client_name = malloc(sizeof(char) * (strlen(cmd->data.str) + 1));
    strcpy(GlobalFDSet.client_name,cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_DefaultVoiceType)
{
    GlobalFDSet.voice_type = (EVoiceType)cmd->data.value;
    return NULL;
}

DOTCONF_CB(cb_DefaultSpelling)
{
    GlobalFDSet.spelling = cmd->data.value;
    return NULL;
}

DOTCONF_CB(cb_DefaultSpellingTable)
{
    GlobalFDSet.spelling_table = (char*) spd_malloc((strlen(cmd->data.str) + 1) * sizeof(char));
    strcpy(GlobalFDSet.spelling_table,cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_DefaultCapLetRecognition)
{
    GlobalFDSet.cap_let_recogn = cmd->data.value;
    return NULL;
}

