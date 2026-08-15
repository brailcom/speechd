/*
The MIT License (MIT)

Copyright © 2026 John Settlemyer

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "kitten.h"

int module_config(const char *configfile)
{
	/* Optional: Open and parse configfile */
	fprintf(stderr, "opening %s\n", configfile);

	return 0;
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
