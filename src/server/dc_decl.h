
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
 * $Id: dc_decl.h,v 1.39 2003-10-01 06:44:25 hanke Exp $
 */

#include "speechd.h"

static TFDSetClientSpecific *cl_spec_section;

int table_add(char *name, char *group);

/* So that gcc doesn't comply about casts to char* */
extern char* spd_strdup(char* string);

/* define dotconf callbacks */
DOTCONF_CB(cb_Port);
DOTCONF_CB(cb_LogFile);
DOTCONF_CB(cb_CustomLogFile);
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
DOTCONF_CB(cb_DefaultPauseContext);
DOTCONF_CB(cb_AddModule);
DOTCONF_CB(cb_MinDelayProgress);
DOTCONF_CB(cb_MaxHistoryMessages);
DOTCONF_CB(cb_BeginClient);
DOTCONF_CB(cb_EndClient);
DOTCONF_CB(cb_unknown);

/* define dotconf configuration options */

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
       if (!cl_spec_section) \
           GlobalFDSet.arg = strdup(cmd->data.str); \
       else \
           cl_spec_section->val.arg = strdup(cmd->data.str); \
       return NULL; \
   }    

#define GLOBAL_FDSET_OPTION_CB_INT(name, arg) \
   DOTCONF_CB(cb_ ## name) \
   { \
       if (!cl_spec_section) \
           GlobalFDSet.arg = cmd->data.value; \
       else \
           cl_spec_section->val.arg = cmd->data.value; \
       return NULL; \
   }    

#define ADD_LAST_OPTION() \
   options = add_config_option(options, num_options, "", 0, NULL, NULL, 0);

configoption_t*
load_config_options(int *num_options)
{
    configoption_t *options = NULL;

    cl_spec_section = NULL;
   
    ADD_CONFIG_OPTION(Port, ARG_INT);
    ADD_CONFIG_OPTION(LogFile, ARG_STR);
    ADD_CONFIG_OPTION(CustomLogFile, ARG_LIST);
    ADD_CONFIG_OPTION(LogLevel, ARG_INT);
    ADD_CONFIG_OPTION(SoundModule, ARG_LIST);
    ADD_CONFIG_OPTION(SoundDataDir, ARG_STR);
    ADD_CONFIG_OPTION(AddTable, ARG_STR);
    ADD_CONFIG_OPTION(DefaultModule, ARG_STR);
    ADD_CONFIG_OPTION(LanguageDefaultModule, ARG_LIST);
    ADD_CONFIG_OPTION(DefaultRate, ARG_INT);  
    ADD_CONFIG_OPTION(DefaultPitch, ARG_INT);
    ADD_CONFIG_OPTION(DefaultLanguage, ARG_STR);
    ADD_CONFIG_OPTION(DefaultPriority, ARG_STR);
    ADD_CONFIG_OPTION(MaxHistoryMessages, ARG_INT);   
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
    ADD_CONFIG_OPTION(DefaultCapLetRecognition, ARG_STR);
    ADD_CONFIG_OPTION(DefaultPauseContext, ARG_INT);  
    ADD_CONFIG_OPTION(AddModule, ARG_LIST);
    ADD_CONFIG_OPTION(MinDelayProgress, ARG_INT);
    ADD_CONFIG_OPTION(BeginClient, ARG_STR);
    ADD_CONFIG_OPTION(EndClient, ARG_NONE);
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
    GlobalFDSet.spelling_mode = 0;
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
    GlobalFDSet.voice = MALE1;
    GlobalFDSet.cap_let_recogn = 0;
    GlobalFDSet.cap_let_recogn_sound = NULL;
    GlobalFDSet.min_delay_progress = 2000;
    GlobalFDSet.pause_context = 0;

    MaxHistoryMessages = 10000;

    if (!spd_log_level_set) spd_log_level = 3;    
    if (!spd_port_set) spd_port = SPEECHD_DEFAULT_PORT;

    sound_module = NULL;
    logfile = stderr;
    custom_logfile = NULL;
    
    SOUND_DATA_DIR = strdup(SND_DATA);
}

DOTCONF_CB(cb_Port)
{
    if (cmd->data.value <= 0){
        MSG(3, "Unvalid port specified in configuration");
        return NULL;
    }

    if (!spd_port_set){
        spd_port = cmd->data.value;        
    }

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
    logfile = fopen(cmd->data.str, "a");
    if (logfile == NULL){
        fprintf(stderr, "Error: can't open logging file! Using stdout.\n");
        logfile = stdout;
    }
    
    MSG(2,"Speech Dispatcher Logging to file %s", cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_CustomLogFile)
{
    char *kind;
    char *file;

    if(cmd->data.list[0] == NULL) FATAL("No log kind specified in CustomLogFile");
    if(cmd->data.list[1] == NULL) FATAL("No log file specified in CustomLogFile");
    kind = strdup(cmd->data.list[0]);
    assert(kind != NULL);
    file = strdup(cmd->data.list[1]);
    assert(file != NULL);

    custom_log_kind = kind;
    if (!strncmp(file,"stdout",6)){
        custom_logfile = stdout;
        return NULL;
    }
    if (!strncmp(file,"stderr",6)){
        custom_logfile = stderr;
        return NULL;
    }
    custom_logfile = fopen(file, "a");
    if (custom_logfile == NULL){
        fprintf(stderr, "Error: can't open custom log file, using stdout\n");
        custom_logfile = stdout;
    }
    
    MSG(2,"Speech Dispatcher custom logging to file %s", file);
    return NULL;
}

DOTCONF_CB(cb_LogLevel)
{
    int level = cmd->data.value;

    if(level < 0 || level > 5){
        MSG(1,"Log level must be betwen 1 and 5.");
        return NULL;
    }

    if (!spd_log_level_set){
        spd_log_level = level;
    }

    MSG(4,"Speech Dispatcher Logging with priority %d", spd_log_level);
    return NULL;
}

DOTCONF_CB(cb_SoundModule)
{    
    sound_module = load_output_module(cmd->data.list[0], cmd->data.list[1], cmd->data.list[2], FILTERING_NONE);
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
        if (!strcmp(language, "generic")) generic_lang_icons = icons_hash;
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
    if (!cl_spec_section)
        GlobalFDSet.rate = cmd->data.value;
    else
        cl_spec_section->val.rate = rate;
    return NULL;
}

DOTCONF_CB(cb_DefaultPitch)
{
    int pitch = cmd->data.value;
    if (pitch < -100 || pitch > +100) MSG(3, "Default pitch out of range.");
    if (!cl_spec_section)
        GlobalFDSet.pitch = pitch;
    else
        cl_spec_section->val.pitch = pitch;
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
    EVoiceType voice;

    assert(cmd->data.str != NULL);

    voice_s = g_ascii_strup(cmd->data.str, strlen(cmd->data.str));

    if (!strcmp(voice_s, "MALE1")) voice = MALE1;
    else if (!strcmp(voice_s, "MALE2")) voice = MALE2;
    else if (!strcmp(voice_s, "MALE3")) voice = MALE3;
    else if (!strcmp(voice_s, "FEMALE1")) voice = FEMALE1;
    else if (!strcmp(voice_s, "FEMALE2")) voice = FEMALE2;
    else if (!strcmp(voice_s, "FEMALE3")) voice = FEMALE3;
    else if (!strcmp(voice_s, "CHILD_MALE")) voice = CHILD_MALE;
    else if (!strcmp(voice_s, "CHILD_FEMALE")) voice = CHILD_FEMALE;
    else{
        MSG(2, "Unknown default voice specified in configuration, using default.");
        voice = MALE1;
    }

    if (!cl_spec_section){
        MSG(4, "Default voice type set to %d", GlobalFDSet.voice);
        GlobalFDSet.voice = voice;
    }else{
        cl_spec_section->val.voice = voice;
    }
    spd_free(voice_s);

    return NULL;
}

DOTCONF_CB(cb_MaxHistoryMessages)
{
    int msgs = cmd->data.value;
    if (msgs > 0) MSG(3, "configuration: MaxHistoryMessages must be a positive number.");
    MaxHistoryMessages = msgs;
    return NULL;
}

DOTCONF_CB(cb_DefaultPunctuationMode)
{
    char *pmode = cmd->data.str;
    int mode;

    assert(pmode != NULL);
    if(!strcmp(pmode, "all")) mode = 1;
    else if(!strcmp(pmode, "none")) mode = 0;
    else if(!strcmp(pmode, "some")) mode = 2;
    else MSG(2,"Unknown punctuation mode specified in configuration.");

    if (!cl_spec_section)
        GlobalFDSet.punctuation_mode = mode;
    else
        cl_spec_section->val.punctuation_mode = mode;

    return NULL;
}

GLOBAL_FDSET_OPTION_CB_INT(DefaultSpelling, spelling_mode);

DOTCONF_CB(cb_DefaultCapLetRecognition)
{
    char *mode;
    ECapLetRecogn clr;

    assert(cmd->data.str != NULL);
    mode = g_ascii_strdown(cmd->data.str, strlen(cmd->data.str));

    if (!strcmp(mode, "none")) clr = RECOGN_NONE;
    else if(!strcmp(mode, "spell")) clr = RECOGN_SPELL;
    else if(!strcmp(mode, "icon")) clr = RECOGN_ICON;
    else{
        MSG(3, "Invalid capital letters recognition mode specified in configuration");
        return NULL;
    }

    if (!cl_spec_section)
        GlobalFDSet.cap_let_recogn = clr;
    else
        cl_spec_section->val.cap_let_recogn = clr;

    return NULL;
}

DOTCONF_CB(cb_MinDelayProgress)
{
    if (cmd->data.value < 0) MSG(2, "Time specified in MinDelayProgress not valid");
    GlobalFDSet.min_delay_progress = cmd->data.value;
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

GLOBAL_FDSET_OPTION_CB_INT(DefaultPauseContext, pause_context);

DOTCONF_CB(cb_AddModule)
{
    char *module_name;
    char *module_prgname;
    char *module_cfgfile;
    char *mod_fil_s;
    EFilteringType module_filtering;

    OutputModule *cur_mod;

    if (cmd->data.list[0] != NULL) module_name = strdup(cmd->data.list[0]);
    else FATAL("No output module name specified in configuration under AddModule");

    mod_fil_s = cmd->data.list[1];
    if (mod_fil_s == NULL) module_filtering = FILTERING_NONE;
    else if (!strcmp(mod_fil_s, "filter_none")) module_filtering = FILTERING_NONE;
    else if (!strcmp(mod_fil_s, "filter_self")) module_filtering = FILTERING_SELF;
    else if (!strcmp(mod_fil_s, "filter_generic")) module_filtering = FILTERING_GENERIC;
            
    module_prgname = cmd->data.list[2];
    module_cfgfile = cmd->data.list[3];

    cur_mod = load_output_module(module_name, module_prgname, module_cfgfile, module_filtering);
    if (cur_mod == NULL){
        MSG(3, "Couldn't load specified output module");
        return NULL;
    }

    MSG(5,"Module name=%s being inserted into hash table", cur_mod->name);
    assert(cur_mod->name != NULL);
    g_hash_table_insert(output_modules, module_name, cur_mod);

    return NULL;
}

#define SET_PAR(name, value) cl_spec->val.name = value;
#define SET_PAR_STR(name) cl_spec->val.name = NULL;
DOTCONF_CB(cb_BeginClient)
{
    TFDSetClientSpecific *cl_spec;

    if (cl_spec_section != NULL)
        FATAL("Configuration: Already in client specific section, can't open a new one!");

    if (cmd->data.str == NULL)
        FATAL("Configuration: You must specify some client's name for BeginClient");

    cl_spec = (TFDSetClientSpecific*) spd_malloc(sizeof(TFDSetClientSpecific));
    cl_spec->pattern = spd_strdup(cmd->data.str);
    cl_spec_section = cl_spec;

    SET_PAR(rate, -101)
    SET_PAR(pitch, -101)
    SET_PAR(punctuation_mode, -1)
    SET_PAR(spelling_mode, -1)
    SET_PAR(voice, -1)
    SET_PAR(cap_let_recogn, -1)
    SET_PAR(pause_context, -1);
    SET_PAR_STR(punctuation_some)
    SET_PAR_STR(punctuation_table)
    SET_PAR_STR(spelling_table)
    SET_PAR_STR(char_table)
    SET_PAR_STR(key_table)
    SET_PAR_STR(snd_icon_table)
    SET_PAR_STR(language)
    SET_PAR_STR(output_module)
    SET_PAR_STR(cap_let_recogn_table)
    SET_PAR_STR(cap_let_recogn_sound)
    
    return NULL;
}
#undef SET_PAR
#undef SET_PAR_STR

DOTCONF_CB(cb_EndClient)
{
    if (cl_spec_section == NULL)
        FATAL("Configuration: Already outside the client specific section!");
    
    client_specific_settings = g_list_append(client_specific_settings, cl_spec_section);
    return NULL;
}

DOTCONF_CB(cb_unknown)
{
    //    MSG(4,"Unknown option, doesn't matter");
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
