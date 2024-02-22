/*
 * open_jtalk.c - Speech Dispatcher backend for Open JTalk
 *
 * Copyright (C) 2020-2021, 2024 Samuel Thibault <samuel.thibault@ens-lyon.org>
 * Copyright (C) 2023 Chinamu Kawano <tinaxd@protonmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Samuel Thibault AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <sys/stat.h>

#include "module_main.h"
#include "module_utils.h"

#define MODULE_NAME "open_jtalk"
#define MODULE_VERSION "0.1"

#define WAV_START_BITS_PER_SAMPLE 34
#define WAV_START_NUM_CHANNELS 22
#define WAV_START_SAMPLE_RATE 24
#define WAV_START_SIZE_OF_SAMPLES 40
#define WAV_START_SAMPLES 44

DECLARE_DEBUG();

MOD_OPTION_1_STR(OpenjtalkDictionaryDirectory);
MOD_OPTION_1_STR(OpenjtalkVoice);

int module_init(char **msg)
{
	fprintf(stderr, "initializing\n");

	*msg = strdup("ok!");

	return 0;
}

int module_load(void)
{
	INIT_SETTINGS_TABLES();
	REGISTER_DEBUG();

	MOD_OPTION_1_STR_REG(OpenjtalkDictionaryDirectory,
			     "/var/lib/mecab/dic/open-jtalk");
	MOD_OPTION_1_STR_REG(OpenjtalkVoice,
			     "/usr/share/hts-voice/nitech-jp-atr503-m001/nitech_jp_atr503_m001.htsvoice");

	DBG("OpenjtalkDictionaryDirectory: %s", OpenjtalkDictionaryDirectory);
	DBG("OpenjtalkVoice: %s", OpenjtalkVoice);

	module_audio_set_server();

	return 0;
}

SPDVoice **module_list_voices(void)
{
	/* TODO: Allow users to choose htsvoice */
	SPDVoice **ret = malloc(2 * sizeof(*ret));

	ret[0] = malloc(sizeof(*(ret[0])));
	ret[0]->name = strdup("Default");
	ret[0]->language = strdup("ja");
	ret[0]->variant = NULL;

	ret[1] = NULL;

	return ret;
}

void module_speak_sync(const char *data, size_t bytes, SPDMessageType msgtype)
{
	module_speak_ok();

	DBG("speaking '%s'", data);

	module_report_event_begin();

	/* Strip SSML (Open JTalk does not support it.) */
	char *plain_data = module_strip_ssml(data);

	char template[] = "/tmp/speechd-openjtalk-XXXXXX";
	umask(0077);
	int tmp_fd = mkstemp(template);
	if (tmp_fd == -1) {
		DBG("temporary .wav file creation failed");
		goto FINISH;
	}

	/* do not need fd */
	close(tmp_fd);

	/* construct command line */
	char *cmd;
	if (asprintf(&cmd,
		     "open_jtalk -x %s -m %s -ow %s",
		     OpenjtalkDictionaryDirectory, OpenjtalkVoice,
		     template) == -1) {
		DBG("failed to construct command line");
		goto TEMPLATE_FINISH;
	}

	FILE *oj_fp = popen(cmd, "w");
	free(cmd);
	if (oj_fp == NULL) {
		DBG("failed to execute open_jtalk");
		goto TEMPLATE_FINISH;
	}

	fprintf(oj_fp, "%s", plain_data);

	/* wait for finish */
	int status = pclose(oj_fp);
	if (status != 0) {
		DBG("open_jtalk exited with non-zero code");
		goto TEMPLATE_FINISH;
	}

	/* play the output wav */
	DBG("output to %s", template);

	AudioTrack track = {
		.bits = 0,
		.num_channels = 0,
		.sample_rate = 0,
		.num_samples = 0,
		.samples = NULL
	};
	AudioFormat format = SPD_AUDIO_LE;

	FILE *audio_fp = fopen(template, "rb");
	if (audio_fp == NULL) {
		DBG("failed to open wav file");
		goto TEMPLATE_FINISH;
	}
	DBG("opened wav file");

	fseek(audio_fp, WAV_START_BITS_PER_SAMPLE, SEEK_SET);
	size_t ret = fread(&track.bits, 2, 1, audio_fp);
	if (ret != 1) {
		DBG("failed to read track.bits");
		goto FP_FINISH;
	}
	DBG("read track.bits");

	fseek(audio_fp, WAV_START_NUM_CHANNELS, SEEK_SET);
	ret = fread(&track.num_channels, 2, 1, audio_fp);
	if (ret != 1) {
		DBG("failed to read track.num_channels");
		goto FP_FINISH;
	}
	DBG("read track.num_channels");

	fseek(audio_fp, WAV_START_SAMPLE_RATE, SEEK_SET);
	ret = fread(&track.sample_rate, 4, 1, audio_fp);
	if (ret != 1) {
		DBG("failed to read track.sample_rate");
		goto FP_FINISH;
	}
	DBG("read track.sample_rate");

	fseek(audio_fp, WAV_START_SIZE_OF_SAMPLES, SEEK_SET);
	ret = fread(&track.num_samples, 4, 1, audio_fp);
	if (ret != 1) {
		DBG("failed to read track.num_samples");
		goto FP_FINISH;
	}
	track.num_samples =
	    track.num_samples / (track.num_channels) / (track.bits / 8);
	DBG("read track.num_samples");
	DBG("bits: %d num_channels: %d sample_rate: %d num_samples: %d",
	    track.bits, track.num_channels, track.sample_rate,
	    track.num_samples);

	fseek(audio_fp, WAV_START_SAMPLES, SEEK_SET);
	track.samples =
	    malloc(track.num_samples * track.num_channels * track.bits / 8);
	ret =
	    fread(track.samples, track.bits / 8,
		  track.num_samples * track.num_channels, audio_fp);
	if (ret != track.num_samples * track.num_channels) {
		DBG("failed to read track.samples");
		free(track.samples);
		goto FP_FINISH;
	}
	DBG("read track.samples");

	module_tts_output_server(&track, format);

	free(track.samples);

FP_FINISH:
	fclose(audio_fp);

TEMPLATE_FINISH:
	unlink(template);

FINISH:
	free(plain_data);

	module_report_event_end();
}

size_t module_pause(void)
{
	/* not supported */
	DBG("pausing (not supported)");

	return -1;
}

int module_stop(void)
{
	/* not supported */
	DBG("stopping (not supported)");

	return 0;
}

int module_close(void)
{
	DBG("closing");

	return 0;
}
