/*
The MIT License (MIT)

Copyright © 2026 John Settlemyer

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "kitten.h"

#include <dotconf.h>

bool debug_module=false;

static DOTCONF_CB(cb_voicefiles);
static DOTCONF_CB(cb_modelPath);
static DOTCONF_CB(cb_debug_module);

static const configoption_t options[] = {
	{"AddVoiceFile", ARG_LIST, cb_voicefiles, NULL, CTX_ALL},
	{"modelPath", ARG_STR, cb_modelPath, NULL, CTX_ALL},
	{"debug", ARG_STR, cb_debug_module, NULL, CTX_ALL},
	LAST_OPTION
};

bool built_voice_table = false;

/*
the models .conf can be used to configure the model/voice files.
add a line to the .conf like below this to do so.
AddVoiceFile "voice_quality" "model filename" "model sha256" "voices filename" "voices sha256"

here is an example using the same configs as the default.
https://gist.github.com/jsett/b82d19d3e2e538a29e5e79995b619a62

you can also set the modelPath to configure where to search for models/voices.
modelPath "<your path>"
*/
int module_config(const char *configfilepath)
{
	MSG(1, "opening .conf file %s\n", configfilepath);

	init_file_hashtable_and_distro_subdir();

	configfile_t *configfile;

	configfile = dotconf_create((char* )configfilepath,
				    options, NULL, CASE_INSENSITIVE);

	if (!configfile) {
		MSG(2, "Error: opening config file\n");
		return -1;
	}

	if (dotconf_command_loop(configfile) == 0)
		MSG(2, "Nothing in config file\n");

	dotconf_cleanup(configfile);

	if (!built_voice_table){
		DEBUG_PRINT("building voice table from defaults. dot conf was not set\n");
		build_hash_from_defaults(files_hash_table);
	}

	DEBUG_PRINT("finished parsing config file\n");

	return 0;
}

DOTCONF_CB(cb_voicefiles)
{
	int i;
	if (cmd->arg_count != 5){
		MSG(2, "Error: Incorrectly formated .conf line for AddVoiceFile\n");
		MSG(2, "AddVoiceFile should be formated like\n");
		MSG(2, "AddVoiceFile \"voice_quality\" \"model filename\" \"model sha256\" \"voices filename\" \"voices sha256\"\n");
	} else {

		char* quality = cmd->data.list[0];

		file_hash_add_sub_key(files_hash_table, quality, "model_filename", cmd->data.list[1]);
		file_hash_add_sub_key(files_hash_table, quality, "model_sha256", cmd->data.list[2]);
		file_hash_add_sub_key(files_hash_table, quality, "voices_filename", cmd->data.list[3]);
		file_hash_add_sub_key(files_hash_table, quality, "voices_sha256", cmd->data.list[4]);

		built_voice_table = true;
		MSG(5, "Added voice file for %s\n", cmd->data.list[1]);
	}
	return NULL;
}

DOTCONF_CB(cb_modelPath)
{
	g_string_assign(distro_target_subdir, cmd->data.str);
	MSG(5, "Info: Using '%s' for distro_target_subdir\n", distro_target_subdir->str);
	return NULL;
}

DOTCONF_CB(cb_debug_module)
{
	if (!strcmp(cmd->data.str, "TRUE")){
		MSG(5, "Debug enabled\n");
		debug_module = true;
	}
	return NULL;
}

int module_init(char **msg)
{
	MSG(1, "Initializing\n");

	int ret = init_model_thread_pool();
	if (ret == 0){
		*msg = strdup("ok!");
	} else{
		*msg = strdup("Failed Init");
	}

	return ret;
}

SPDVoice **module_list_voices(void)
{
    VOICE_LIST(DEFINE_VOICE)

    static SPDVoice* voices[] = {
        VOICE_LIST(VOICE_PTR_ITEM)
        NULL
    };

	return voices;
}

int module_set(const char *var, const char *val)
{
	DEBUG_PRINT("module_set: var '%s' to be set to '%s'\n", var, val);

    if (!strcmp(var, "synthesis_voice")) {
        // change the voice.
        model_change_voice(var, val);
		return 0;
	} else if (!strcmp(var, "rate")) {
        // change the speed.
        model_change_speed(var, val);
		return 0;
	}

	return 0;
}

int module_audio_set(const char *var, const char *val)
{
	/* Optional: interpret audio parameter */
	if (!strcmp(var, "audio_output_method")) {
		/* Only server-side audio supported */
		if (strcmp(val, "server") != 0)
			return -1;
		return 0;
	}
	return -1;
}

int module_audio_init(char **status)
{
	/* Optional: open audio */
	return 0;
}

int module_loglevel_set(const char *var, const char *val)
{
	/* Optional: accept loglevel change */
	return 0;
}

int module_debug(int enable, const char *file)
{
	/* Optional: if enable == 1, open file to dump debugging */
	/* Otherwise close it */
	return 0;
}

int module_loop(void)
{
	/* Main loop */
	MSG(1, "Main Loop\n");

	/* Let module_process run the protocol */
	int ret = module_process(STDIN_FILENO, 1);

	if (ret != 0)
		MSG(2, "Error: Broken pipe, exiting...\n");

	return ret;
}

int module_speak(char *data, size_t bytes, SPDMessageType msgtype)
{
    add_generate_speech_task(data, bytes);
    return 1; // delivery to the synthesizer is successful
}

size_t module_pause(void)
{
	/* Pause playing */
	DEBUG_PRINT("Pausing\n");

    // does not seem there is a resume function so
    // pause will be handled the same as stop.
    model_stop_generation();
	module_report_event_stop();

	return 0;
}

int module_stop(void)
{
	/* Stop any current synth */
	DEBUG_PRINT("Stopping\n");

    model_stop_generation();
	module_report_event_stop();

	return 0;
}

int module_close(void)
{
	/* Deinitialize synthesizer */
	MSG(1, "Closing\n");

    cleanup_threads();

	return 0;
}
