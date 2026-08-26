/*
The MIT License (MIT)

Copyright © 2026 John Settlemyer

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "kitten.h"

#include <dotconf.h>

static DOTCONF_CB(cb_voicefiles);

static const configoption_t options[] = {
	{"AddVoiceFile", ARG_LIST, cb_voicefiles, NULL, CTX_ALL},
	LAST_OPTION
};

bool built_voice_table = false;

/*
the models .conf can be used to configure the model/voice files/downloads.
add a line to the .conf like below this to do so.
AddVoiceFile "voice_quality" "model url" "model filename" "model size bytes" "model sha256" "voices url" "voices filename" "voices size bytes" "voices sha256"

here is an example using the same configs as the default.
https://gist.github.com/jsett/6146eb2803f8780a1830ed816bdc94ef
*/
int module_config(const char *configfilepath)
{
	fprintf(stderr, "opening .conf file %s\n", configfilepath);

	init_file_hashtable();

	configfile_t *configfile;

	configfile = dotconf_create((char* )configfilepath,
				    options, NULL, CASE_INSENSITIVE);

	if (!configfile) {
		fprintf(stderr, "Error opening config file\n");
		return -1;
	}

	if (dotconf_command_loop(configfile) == 0)
		fprintf(stderr, "Nothing in config file\n");

	dotconf_cleanup(configfile);

	if (!built_voice_table){
		fprintf(stderr, "building voice table from defaults. dot conf was not set\n");
		build_hash_from_defaults(files_hash_table);
	}

	fprintf(stderr, "finished parsing config file\n");

	return 0;
}

DOTCONF_CB(cb_voicefiles)
{
	int i;
	if (cmd->arg_count != 9){
		fprintf(stderr, "arg_count: %d\n", cmd->arg_count);
		fprintf(stderr, "Incorrectly formated .conf line for AddVoiceFile\n");
		fprintf(stderr, "AddVoiceFile should be formated like\n");
		fprintf(stderr, "AddVoiceFile \"voice_quality\" \"model url\" \"model filename\" \"model size bytes\" \"model sha256\" \"voices url\" \"voices filename\" \"voices size bytes\" \"voices sha256\"\n");
	} else {

		char* quality = cmd->data.list[0];

		file_hash_add_sub_key(files_hash_table, quality, "model_url", cmd->data.list[1]);
		file_hash_add_sub_key(files_hash_table, quality, "model_filename", cmd->data.list[2]);
		file_hash_add_sub_key(files_hash_table, quality, "model_size", cmd->data.list[3]);
		file_hash_add_sub_key(files_hash_table, quality, "model_sha256", cmd->data.list[4]);
		file_hash_add_sub_key(files_hash_table, quality, "voices_url", cmd->data.list[5]);
		file_hash_add_sub_key(files_hash_table, quality, "voices_filename", cmd->data.list[6]);
		file_hash_add_sub_key(files_hash_table, quality, "voices_size", cmd->data.list[7]);
		file_hash_add_sub_key(files_hash_table, quality, "voices_sha256", cmd->data.list[8]);

		built_voice_table = true;
		fprintf(stderr, "Added voice file for %s\n", cmd->data.list[2]);
	}
	return NULL;
}

int module_init(char **msg)
{
	fprintf(stderr, "initializing\n");

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
	fprintf(stderr,"got var '%s' to be set to '%s'\n", var, val);

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
	fprintf(stderr, "main loop\n");

	/* Let module_process run the protocol */
	int ret = module_process(STDIN_FILENO, 1);

	if (ret != 0)
		fprintf(stderr, "Broken pipe, exiting...\n");

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
	fprintf(stderr, "pausing\n");

    // does not seem there is a resume function so
    // pause will be handled the same as stop.
    model_stop_generation();
	module_report_event_stop();

	return 0;
}

int module_stop(void)
{
	/* Stop any current synth */
	fprintf(stderr, "stopping\n");

    model_stop_generation();
	module_report_event_stop();

	return 0;
}

int module_close(void)
{
	/* Deinitialize synthesizer */
	fprintf(stderr, "closing\n");

    cleanup_threads();

	return 0;
}
