/* define dotconf callbacks */
DOTCONF_CB(cb_AddModule);
DOTCONF_CB(cb_DefaultModule);
DOTCONF_CB(cb_DefaultSpeed);
DOTCONF_CB(cb_DefaultPitch);
DOTCONF_CB(cb_DefaultLanguage);
DOTCONF_CB(cb_DefaultPriority);
DOTCONF_CB(cb_DefaultPunctuationMode);
DOTCONF_CB(cb_DefaultClientName);
DOTCONF_CB(cb_DefaultVoiceType);
DOTCONF_CB(cb_DefaultSpelling);
DOTCONF_CB(cb_DefaultCapLetRecognition);


/* define dotconf configuration options */
static const configoption_t options[] =
{
		   {"AddModule", ARG_STR, cb_AddModule, 0, 0},
		   {"DefaultModule", ARG_STR, cb_DefaultModule, 0, 0},
		   {"DefaultSpeed", ARG_INT, cb_DefaultSpeed, 0, 0},
		   {"DefaultPitch", ARG_INT, cb_DefaultPitch, 0, 0},
		   {"DefaultLanguage", ARG_STR, cb_DefaultLanguage, 0, 0},
		   {"DefaultPriority", ARG_INT, cb_DefaultPriority, 0, 0},
		   {"DefaultPunctutationMode", ARG_STR, cb_DefaultPunctuationMode, 0, 0},
		   {"DefaultClientName", ARG_STR, cb_DefaultClientName, 0, 0},
		   {"DefaultVoiceType", ARG_INT, cb_DefaultVoiceType, 0, 0},
		   {"DefaultSpelling", ARG_TOGGLE, cb_DefaultSpelling, 0, 0},
		   {"DefaultCapLetRecognition", ARG_TOGGLE, cb_DefaultCapLetRecognition, 0, 0},
		      /*{"ExampleOption", ARG_STR, cb_example, 0, 0},
			   *      {"MultiLineRaw", ARG_STR, cb_multiline, 0, 0},
			   *           {"", ARG_NAME, cb_unknown, 0, 0},
			   *                {"MoreArgs", ARG_LIST, cb_moreargs, 0, 0},*/
		      LAST_OPTION
};

DOTCONF_CB(cb_AddModule)
{
	OutputModule *om;
	om = load_output_module(cmd->data.str);
	g_hash_table_insert(output_modules, om->name, om);
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
	GlobalFDSet.punctuation_mode = cmd->data.value;
	return NULL;
}

DOTCONF_CB(cb_DefaultClientName)
{
	GlobalFDSet.client_name = malloc(sizeof(char) * strlen(cmd->data.str) + 1);
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

DOTCONF_CB(cb_DefaultCapLetRecognition)
{
	GlobalFDSet.cap_let_recogn = cmd->data.value;
	return NULL;
}
