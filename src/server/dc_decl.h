
/*
 * dc_decl.h - Dotconf functions and types for Speech Dispatcher
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
 * $Id: dc_decl.h,v 1.30 2003-07-07 09:59:45 hanke Exp $
 */

#include "speechd.h"

int table_add(char *name, char *group);

/* So that gcc doesn't comply about casts to char* */
extern char* spd_strdup(char* string);

char cur_mod_options[255];      /* Which section with parameters of output modules
                                   we are in? */

OutputModule* cur_mod;

/* define dotconf callbacks */
DOTCONF_CB(cb_Port);
DOTCONF_CB(cb_LogFile);
DOTCONF_CB(cb_LogLevel);
DOTCONF_CB(cb_SoundModule);
DOTCONF_CB(cb_SoundDataDir);
DOTCONF_CB(cb_AddTable);
DOTCONF_CB(cb_DefaultModule);
DOTCONF_CB(cb_LanguageDefaultModule);
DOTCONF_CB(cb_DefaultRate);
DOTCONF_CB(cb_DefaultPitch);
DOTCONF_CB(cb_DefaultLanguage);
DOTCONF_CB(cb_DefaultPriority);
DOTCONF_CB(cb_DefaultPunctuationMode);
DOTCONF_CB(cb_DefaultPunctuationTable);
DOTCONF_CB(cb_PunctuationSome);
DOTCONF_CB(cb_DefaultSpellingTable);
DOTCONF_CB(cb_DefaultCapLetRecognTable);
DOTCONF_CB(cb_DefaultClientName);
DOTCONF_CB(cb_DefaultVoiceType);
DOTCONF_CB(cb_DefaultSpelling);
DOTCONF_CB(cb_DefaultCapLetRecognition);
DOTCONF_CB(cb_CapLetRecognIcon);
DOTCONF_CB(cb_DefaultSpellingTable);
DOTCONF_CB(cb_DefaultCharacterTable);
DOTCONF_CB(cb_DefaultKeyTable);
DOTCONF_CB(cb_DefaultSoundTable);
DOTCONF_CB(cb_AddModule);
DOTCONF_CB(cb_fr_AddModule);
DOTCONF_CB(cb_EndAddModule);
DOTCONF_CB(cb_AddParam);
DOTCONF_CB(cb_AddVoice);
DOTCONF_CB(cb_MinDelayProgress);
DOTCONF_CB(cb_unknown);

/* define dotconf configuration options */

static configoption_t first_run_options[] =
{
    {"AddModule", ARG_STR, cb_fr_AddModule, 0, 0},
    {"", ARG_NAME, cb_unknown, 0, 0},
    LAST_OPTION
};

configoption_t *
add_config_option(configoption_t *options, int *num_config_options, char *name, int type,
                  dotconf_callback_t callback, info_t *info,
                  unsigned long context)
{
    configoption_t *opts;

    (*num_config_options)++;
    opts = (configoption_t*) realloc(options, (*num_config_options) * sizeof(configoption_t));
    opts[*num_config_options-1].name = strdup(name);
    opts[*num_config_options-1].type = type;
    opts[*num_config_options-1].callback = callback;
    opts[*num_config_options-1].info = info;
    opts[*num_config_options-1].context = context;
    return opts;
}

void
free_config_options(configoption_t *opts, int *num)
{
    int i = 0;

    if (opts == NULL) return;

    for(i==0; i<=(*num)-1; i++){
        //        spd_free(opts[i].name);
        spd_free(opts[i]);     
    }
    *num = 0;
    opts = NULL;
}

#define ADD_CONFIG_OPTION(name, arg_type) \
   options = add_config_option(options, num_options, #name, arg_type, cb_ ## name, 0, 0);

#define GLOBAL_FDSET_OPTION_CB_STR(name, arg) \
   DOTCONF_CB(cb_ ## name) \
   { \
       assert(cmd->data.str != NULL); \
       GlobalFDSet.arg = strdup(cmd->data.str); \
       return NULL; \
   }    

#define GLOBAL_FDSET_OPTION_CB_INT(name, arg) \
   DOTCONF_CB(cb_name) \
   { \
       GlobalFDSet. ## arg = cmd->data.value; \
       return NULL; \
   }    

#define ADD_LAST_OPTION() \
   options = add_config_option(options, num_options, "", 0, NULL, NULL, 0);

configoption_t*
load_config_options(int *num_options)
{
    configoption_t *options = NULL;
   
    ADD_CONFIG_OPTION(Port, ARG_INT);
    ADD_CONFIG_OPTION(LogFile, ARG_STR);
    ADD_CONFIG_OPTION(LogLevel, ARG_INT);
    ADD_CONFIG_OPTION(SoundModule, ARG_STR);
    ADD_CONFIG_OPTION(SoundDataDir, ARG_STR);
    ADD_CONFIG_OPTION(AddTable, ARG_STR);
    ADD_CONFIG_OPTION(DefaultModule, ARG_STR);
    ADD_CONFIG_OPTION(LanguageDefaultModule, ARG_LIST);
    ADD_CONFIG_OPTION(DefaultRate, ARG_INT);  
    ADD_CONFIG_OPTION(DefaultPitch, ARG_INT);
    ADD_CONFIG_OPTION(DefaultLanguage, ARG_STR);
    ADD_CONFIG_OPTION(DefaultPriority, ARG_STR);
    ADD_CONFIG_OPTION(DefaultPunctuationMode, ARG_STR);
    ADD_CONFIG_OPTION(DefaultPunctuationTable, ARG_STR);
    ADD_CONFIG_OPTION(PunctuationSome, ARG_STR);
    ADD_CONFIG_OPTION(DefaultClientName, ARG_STR);
    ADD_CONFIG_OPTION(DefaultVoiceType, ARG_STR);
    ADD_CONFIG_OPTION(DefaultSpelling, ARG_TOGGLE);
    ADD_CONFIG_OPTION(DefaultSpellingTable, ARG_STR);
    ADD_CONFIG_OPTION(DefaultCapLetRecognTable, ARG_STR);
    ADD_CONFIG_OPTION(CapLetRecognIcon, ARG_STR);
    ADD_CONFIG_OPTION(DefaultCharacterTable, ARG_STR);
    ADD_CONFIG_OPTION(DefaultKeyTable, ARG_STR);
    ADD_CONFIG_OPTION(DefaultSoundTable, ARG_STR);
    ADD_CONFIG_OPTION(DefaultCapLetRecognition, ARG_STR)
    ADD_CONFIG_OPTION(AddModule, ARG_STR);
    ADD_CONFIG_OPTION(EndAddModule, ARG_NONE);
    ADD_CONFIG_OPTION(AddParam, ARG_LIST);
    ADD_CONFIG_OPTION(AddVoice, ARG_LIST);
    ADD_CONFIG_OPTION(MinDelayProgress, ARG_INT);
    /*{"ExampleOption", ARG_STR, cb_example, 0, 0},
     *      {"MultiLineRaw", ARG_STR, cb_multiline, 0, 0},
     *           {"", ARG_NAME, cb_unknown, 0, 0}, */
        //    LAST_OPTION
    return options;
}

void
load_default_global_set_options()
{
    GlobalFDSet.priority = 3;
    GlobalFDSet.punctuation_mode = PUNCT_NONE;
    GlobalFDSet.punctuation_some = strdup("(){}[]<>@#$%^&*");
    GlobalFDSet.punctuation_table = strdup("spelling_short");
    GlobalFDSet.spelling = 0;
    GlobalFDSet.spelling_table = strdup("spelling_short");
    GlobalFDSet.cap_let_recogn_table = strdup("spelling_short");
    GlobalFDSet.char_table = strdup("spelling_short");
    GlobalFDSet.key_table = strdup("keys");
    GlobalFDSet.snd_icon_table = strdup("sound_icons");
    GlobalFDSet.rate = 0;
    GlobalFDSet.pitch = 0;
    GlobalFDSet.client_name = strdup("unknown:unknown:unknown");
    GlobalFDSet.language = strdup("en");
    GlobalFDSet.output_module = NULL;
    GlobalFDSet.voice_type = MALE1;
    GlobalFDSet.cap_let_recogn = 0;
    GlobalFDSet.cap_let_recogn_sound = NULL;
    GlobalFDSet.min_delay_progress = 2000;

    spd_log_level = 3;
    logfile = stderr;
    sound_module = NULL;

    SPEECHD_PORT = SPEECHD_DEFAULT_PORT;
    SOUND_DATA_DIR = strdup(SND_DATA);
}

DOTCONF_CB(cb_Port)
{
    if (cmd->data.value <= 0){
        MSG(3, "Unvalid port specified in configuration");
        return NULL;
    }

    if (cmd->data.value > 0)
        SPEECHD_PORT = cmd->data.value;        

    return NULL;
}

DOTCONF_CB(cb_LogFile)
{
    assert(cmd->data.str != NULL);
    if (!strncmp(cmd->data.str,"stdout",6)){
        logfile = stdout;
        return NULL;
    }
    if (!strncmp(cmd->data.str,"stderr",6)){
        logfile = stderr;
        return NULL;
    }
    logfile = fopen(cmd->data.str, "w");
    if (logfile == NULL){
        printf("Error: can't open logging file! Using stdout.\n");
        logfile = stdout;
    }
    MSG(2,"Speech Dispatcher Logging to file %s", cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_LogLevel)
{
    int level = cmd->data.value;

    if(level < 0 || level > 5){
        MSG(1,"Log level must be betwen 1 and 5.");
        return NULL;
    }

    spd_log_level = level;

    MSG(4,"Speech Dispatcher Logging with priority %d", spd_log_level);
    return NULL;
}


DOTCONF_CB(cb_SoundModule)
{
    sound_module = load_output_module(cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_SoundDataDir)
{
    assert(cmd->data.str != NULL);
    SOUND_DATA_DIR = strdup(cmd->data.str);
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
    char *group;
    int group_set = 0;
    char *bline;
	
    MSG(4,"Reading sound icons file...");
	
    language = NULL;
    tablename = NULL;
    bline = (char*) spd_malloc(256);
    filename = (char*) spd_malloc(256);
	
    sprintf(filename, SYS_CONF"/%s", cmd->data.str);
    MSG(4, "Reading table from file %s", filename);
    if((icons_file = fopen(filename, "r"))==NULL){
        MSG(2,"Table %s file specified in speechd.conf doesn't exist!", filename);
        return NULL;	  
    }

    while(1){
        line = (char*) spd_malloc(256 * sizeof(char));
        if(fgets(line, 254, icons_file) == NULL){
            MSG(2, "Specified table %s empty or missing language, table or group identification.", filename);
            spd_free(line);
            return NULL;
        }

        if(strlen(line) <= 2) continue;
        if ((line[0] == '#') && (line[1] == ' ')) continue;

        key = strtok(line,":\r\n");
        if(key == NULL) continue;
        g_strstrip(key);

        value = strtok(NULL,"\r\n");
        if(value == NULL){
            if(!strcmp(key,"definition")){
                if(tablename == NULL){
                    MSG(2, "Table name must preceed definition of symbols in [%s]!", filename);
                    return NULL;
                }
                if(language == NULL){
                    MSG(2, "Table language must preceed definition of symbols in [%s]!", filename);
                    return NULL;
                }
                if(group_set == 0){             
                    MSG(2, "Table group(s) must preceed definition of symbols in [%s]!", filename);
                    return NULL;
                }
                break;
            }            
            continue;
        }

        g_strstrip(value);

        if(!strcmp(key,"language"))  language = value;
        if(!strcmp(key,"table")) tablename = value;
        if(!strcmp(key,"group")){
            if(tablename == NULL){
                MSG(2, "Table name must preceed group definitions in [%s]!", filename);
                return NULL;
            }
            if(table_add(tablename, value) == 0) group_set = 1;
        }

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
        g_strstrip(helper);
        strcpy(bline, helper);
        character = strtok(helper, "\"");

	    if (character == NULL){
            /* probably a blank line or commentary */
            continue;
        }

        if(!strcmp(character,"\\")){
            strcpy(character,"\"");
            strtok(NULL, "="); /* skip the next quotes */
        }
        if(!strcmp(character,"\\\\")){
            strcpy(character,"\\");
        }

        key = malloc((strlen(character) + strlen(tablename) + 2) * sizeof(char));
        sprintf(key, "%s_%s", tablename, character);

        helper = strtok(NULL, "\r\n");
        g_strstrip(helper);
        value = strtok(helper, "=\r\n");

        if(value == NULL){
            MSG(2, "Bad syntax (no value for a given key) [%s] in %s", bline, filename);
            return NULL;
        }

        g_strstrip(value);
        g_hash_table_insert(icons_hash, key, value);
		MSG(5,"Pair |%s+%s| inserted", key, value);
    }

    return NULL;
}

GLOBAL_FDSET_OPTION_CB_STR(DefaultModule, output_module);
GLOBAL_FDSET_OPTION_CB_STR(DefaultLanguage, language);
GLOBAL_FDSET_OPTION_CB_STR(PunctuationSome, punctuation_some);
GLOBAL_FDSET_OPTION_CB_STR(DefaultPunctuationTable, punctuation_table);
GLOBAL_FDSET_OPTION_CB_STR(DefaultSpellingTable, spelling_table);
GLOBAL_FDSET_OPTION_CB_STR(DefaultCharacterTable, char_table);
GLOBAL_FDSET_OPTION_CB_STR(DefaultKeyTable, key_table);
GLOBAL_FDSET_OPTION_CB_STR(DefaultSoundTable, snd_icon_table);
GLOBAL_FDSET_OPTION_CB_STR(DefaultCapLetRecognTable, cap_let_recogn_table);
GLOBAL_FDSET_OPTION_CB_STR(CapLetRecognIcon, cap_let_recogn_sound);

GLOBAL_FDSET_OPTION_CB_STR(DefaultClientName, client_name);

DOTCONF_CB(cb_DefaultRate)
{
    int rate = cmd->data.value;

    if (rate < -100 || rate > +100) MSG(3, "Default rate out of range.");
    GlobalFDSet.rate = cmd->data.value;
    return NULL;
}

DOTCONF_CB(cb_DefaultPitch)
{
    int pitch = cmd->data.value;
    if (pitch < -100 || pitch > +100) MSG(3, "Default pitch out of range.");
    GlobalFDSet.pitch = pitch;
    return NULL;
}

DOTCONF_CB(cb_DefaultPriority)
{
    char *priority_s;
    assert(cmd->data.str != NULL);

    priority_s = g_ascii_strdown(cmd->data.str, strlen(cmd->data.str));

    if (!strcmp(priority_s, "important")) GlobalFDSet.priority = 1;
    else if (!strcmp(priority_s, "text")) GlobalFDSet.priority = 2;
    else if (!strcmp(priority_s, "message")) GlobalFDSet.priority = 3;
    else if (!strcmp(priority_s, "notification")) GlobalFDSet.priority = 4;
    else if (!strcmp(priority_s, "progress")) GlobalFDSet.priority = 5;
    else{
        MSG(2, "Unknown default priority specified in configuration.");
        return NULL;
    }

    spd_free(priority_s);

    return NULL;
}

DOTCONF_CB(cb_DefaultVoiceType)
{
    char *voice_s;

    assert(cmd->data.str != NULL);

    voice_s = g_ascii_strup(cmd->data.str, strlen(cmd->data.str));

    if (!strcmp(voice_s, "MALE1")) GlobalFDSet.voice_type = MALE1;
    else if (!strcmp(voice_s, "MALE2")) GlobalFDSet.voice_type = MALE2;
    else if (!strcmp(voice_s, "MALE3")) GlobalFDSet.voice_type = MALE3;
    else if (!strcmp(voice_s, "FEMALE1")) GlobalFDSet.voice_type = FEMALE1;
    else if (!strcmp(voice_s, "FEMALE2")) GlobalFDSet.voice_type = FEMALE2;
    else if (!strcmp(voice_s, "FEMALE3")) GlobalFDSet.voice_type = FEMALE3;
    else if (!strcmp(voice_s, "CHILD_MALE")) GlobalFDSet.voice_type = CHILD_MALE;
    else if (!strcmp(voice_s, "CHILD_FEMALE")) GlobalFDSet.voice_type = CHILD_FEMALE;
    else{
        MSG(2, "Unknown default voice specified in configuration, using default.");
        GlobalFDSet.voice_type = MALE1;
        return NULL;
    }

    MSG(4, "Default voice type set to %d", GlobalFDSet.voice_type);

    spd_free(voice_s);

    return NULL;
}

DOTCONF_CB(cb_DefaultPunctuationMode)
{
    char *pmode = cmd->data.str;

    assert(pmode != NULL);
    if(!strcmp(pmode, "all")) GlobalFDSet.punctuation_mode = 1;
    else if(!strcmp(pmode, "none")) GlobalFDSet.punctuation_mode = 0;
    else if(!strcmp(pmode, "some")) GlobalFDSet.punctuation_mode = 2;
    else MSG(2,"Unknown punctuation mode specified in configuration.");

    return NULL;
}

DOTCONF_CB(cb_DefaultSpelling)
{
    GlobalFDSet.spelling = cmd->data.value;
    return NULL;
}


DOTCONF_CB(cb_DefaultCapLetRecognition)
{
    char *mode;
    assert(cmd->data.str != NULL);
    mode = g_ascii_strdown(cmd->data.str, strlen(cmd->data.str));

    if (!strcmp(mode, "none")) GlobalFDSet.cap_let_recogn = RECOGN_NONE;
    else if(!strcmp(mode, "spell")) GlobalFDSet.cap_let_recogn = RECOGN_SPELL;
    else if(!strcmp(mode, "icon")) GlobalFDSet.cap_let_recogn = RECOGN_ICON;
    else{
        MSG(3, "Invalid capital letters recognition mode specified in configuration");
        return NULL;
    }

    return NULL;
}

DOTCONF_CB(cb_MinDelayProgress)
{
    if (cmd->data.value < 0) MSG(2, "Time specified in MinDelayProgress not valid");
    GlobalFDSet.min_delay_progress = cmd->data.value;
    return NULL;
}

DOTCONF_CB(cb_AddModule)
{
    if (cmd->data.str == NULL) FATAL("No output module name specified");
    cur_mod = g_hash_table_lookup(output_modules, cmd->data.str);
    if (cur_mod == NULL) FATAL("Internal error in finding output modules");

    return NULL;
}

DOTCONF_CB(cb_LanguageDefaultModule)
{
    char *key;
    char *value;

    if(cmd->data.list[0] == NULL) FATAL("No language specified for LanguageDefaultModule");
    if(cmd->data.list[0] == NULL) FATAL("No module specified for LanguageDefaultModule");
    
    key = strdup(cmd->data.list[0]);
    value = strdup(cmd->data.list[1]);

    g_hash_table_insert(language_default_modules, key, value);

    return NULL;
}

DOTCONF_CB(cb_fr_AddModule)
{
    char *name;

    if (cmd->data.str == NULL) FATAL("No output module name specified");
    cur_mod = load_output_module(cmd->data.str);

    if (cur_mod == NULL) FATAL("Couldn't load specified output module");
    assert(cur_mod->name != NULL);

    name = strdup(cmd->data.str);
    g_hash_table_insert(output_modules, name, cur_mod);

    return NULL;
}

DOTCONF_CB(cb_EndAddModule)
{
    if(cur_mod == NULL){
        FATAL("Configuration: Trying to end a BeginModuleOptions section that was never opened");       
    }

    if (init_output_module(cur_mod) == -1){
        MSG(1,"Couldn't initialize output module %s", cur_mod->name);
        g_hash_table_remove(output_modules, cur_mod->name);
        spd_module_free(cur_mod);
    }

    cur_mod = NULL;
    return NULL;
}

DOTCONF_CB(cb_unknown)
{
    //    MSG(4,"Unknown option, doesn't matter");
    return NULL;
}

DOTCONF_CB(cb_AddParam)
{
    char *key;
    char *value;

    if (cur_mod == NULL){
        MSG(2,"Output module parameter not inside an output modules section");
        return NULL;
    }

    if (cmd->data.list[0] == NULL){
        MSG(2,"Missing parameter name.");
        return NULL;
    }

    if (cmd->data.list[1] == NULL){
        MSG(2,"Missing option name for parameter %s.", cmd->data.list[0]);
        return NULL;
    }

    if (cur_mod->settings.voices == NULL){
        return NULL;
    }

    MSG(3,"Adding parameter: %s=%s", cmd->data.list[0],
        cmd->data.list[1]);
    
    key = spd_strdup(cmd->data.list[0]);
    value = spd_strdup(cmd->data.list[1]);

    g_hash_table_insert(cur_mod->settings.params, key, value);

    return NULL;
}

char*
set_voice(char *value){
    char *ret;
    if (value == NULL) return NULL;
    ret = (char*) spd_malloc((strlen(value) + 1) * sizeof(char));
    strcpy(ret, value);
    return ret;
}

DOTCONF_CB(cb_AddVoice)
{
    SPDVoiceDef *voices;
    char *language = cmd->data.list[0];
    char *symbolic = cmd->data.list[1];
    char *voicename = cmd->data.list[2];
    char *key;
    SPDVoiceDef *value;

   if (cur_mod == NULL){
        MSG(2,"Output module parameter not inside an output modules section");
        return NULL;
    }

    if (language == NULL){
        MSG(2,"Missing language.");
        return NULL;
    }

    if (symbolic == NULL){
        MSG(2,"Missing symbolic name.");
        return NULL;
    }

    if (voicename == NULL){
        MSG(2,"Missing voice name for %s", cmd->data.list[0]);
        return NULL;
    }

    if (cur_mod->settings.voices == NULL){
        return NULL;
    }

    MSG(3,"Adding voice: [%s] %s=%s", language,
        symbolic, voicename);

    voices = g_hash_table_lookup(cur_mod->settings.voices, language);
    if (voices == NULL){
        key = spd_strdup(language);
        value = (SPDVoiceDef*) spd_malloc(sizeof(SPDVoiceDef));
        g_hash_table_insert(cur_mod->settings.voices, key, value);
        voices = value;
    }
    
    if (!strcmp(symbolic, "male1")) voices->male1 = set_voice(voicename);
    else if (!strcmp(symbolic, "male2")) voices->male2 = set_voice(voicename);
    else if (!strcmp(symbolic, "male3")) voices->male3 = set_voice(voicename);
    else if (!strcmp(symbolic, "female1")) voices->female1 = set_voice(voicename);
    else if (!strcmp(symbolic, "female2")) voices->female2 = set_voice(voicename);
    else if (!strcmp(symbolic, "female3")) voices->female3 = set_voice(voicename);
    else if (!strcmp(symbolic, "child_male")) voices->child_male = set_voice(voicename);
    else if (!strcmp(symbolic, "child_female")) voices->child_female = set_voice(voicename);
    else{
        MSG(2, "Unrecognized symbolic voice name.");
        return NULL;
    }

    return NULL;
}

int
table_add(char *name, char *group)
{
    char *p;

    if((name == NULL)||(strlen(name)<=1)){
        MSG(2,"Invalid table name!");
        return 0;
    }
    if((group == NULL)||(strlen(group)<=1)){
        MSG(2,"Invalid table group for %s!", name);
        return 0;
    }

    p = spd_strdup(name);

    if (!strcmp(group,"sound_icons"))
        tables.sound_icons = g_list_append(tables.sound_icons, p);
    else if(!strcmp(group,"spelling"))
        tables.spelling = g_list_append(tables.spelling, p);
    else if(!strcmp(group, "characters"))
        tables.characters = g_list_append(tables.characters, p);
    else if(!strcmp(group, "keys"))
        tables.keys = g_list_append(tables.keys, p);
    else if(!strcmp(group, "punctuation"))
        tables.punctuation = g_list_append(tables.punctuation, p);
    else if(!strcmp(group, "cap_let_recogn"))
        tables.cap_let_recogn = g_list_append(tables.cap_let_recogn, p);
    else{
        MSG(2,"Invalid table group for %s!", name);
        return 1;
    }
    return 0;
} 
