
/*
 * flite.c - Speech Dispatcher backend for Flite (Festival Lite)
 *
 * Copyright (C) 2001, 2002, 2003, 2007 Brailcom, o.p.s.
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * $Id: flite.c,v 1.59 2008-06-09 10:38:02 hanke Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <flite/flite.h>

#include <speechd_types.h>

#include "module_utils.h"

#define MODULE_NAME     "flite"
#define MODULE_VERSION  "0.6"

#define DEBUG_MODULE 1
DECLARE_DEBUG();

/* These aren't declared in any header */
extern cst_voice *register_cmu_us_kal16(void);
extern cst_voice *register_cmu_us_kal(void);
extern cst_voice *register_cmu_us_awb(void);
extern cst_voice *register_cmu_us_rms(void);
extern cst_voice *register_cmu_us_slt(void);

/* Thread and process control */
static int flite_speaking = 0;

static char *buf;

static signed int flite_volume = 0;

/* Internal functions prototypes */
static void flite_set_voice(const char *name);
static void flite_set_rate(signed int rate);
static void flite_set_pitch(signed int pitch);
static void flite_set_volume(signed int pitch);

/* Loaded voices */
static cst_voice *kal16_cst_voice;
static cst_voice *kal_cst_voice;
static cst_voice *awb_cst_voice;
static cst_voice *rms_cst_voice;
static cst_voice *slt_cst_voice;

static SPDVoice rms_voice = {
		.name = "rms",
		.language = "en",
};
static SPDVoice slt_voice = {
		.name = "slt",
		.language = "en",
};
static SPDVoice awb_voice = {
		.name = "awb",
		.language = "en",
};
static SPDVoice kal16_voice = {
		.name = "kal16",
		.language = "en",
};
static SPDVoice kal_voice = {
		.name = "kal",
		.language = "en",
};
static SPDVoice *voices[6]; /* One is left NULL */

/* Current voice */
static cst_voice *flite_voice;

static int flite_stop = 0;

MOD_OPTION_1_INT(FliteMaxChunkLength);
MOD_OPTION_1_STR(FliteDelimiters);

/* Public functions */

int module_load(void)
{
	INIT_SETTINGS_TABLES();

	REGISTER_DEBUG();

	MOD_OPTION_1_INT_REG(FliteMaxChunkLength, 300);
	MOD_OPTION_1_STR_REG(FliteDelimiters, ".");

	return 0;
}

int module_init(char **status_info)
{
	SPDVoice **v;
	DBG("Module init");

	module_audio_set_server();

	*status_info = NULL;

	/* Init flite and register voices */
	flite_init();

	v = voices;

#ifdef HAVE_REGISTER_CMU_US_RMS
	rms_cst_voice = register_cmu_us_rms();
	if (rms_cst_voice)
	{
		*v++ = &rms_voice;
		if (flite_voice == NULL)
			flite_voice = rms_cst_voice;
	}
#endif

#ifdef HAVE_REGISTER_CMU_US_SLT
	slt_cst_voice = register_cmu_us_slt();
	if (slt_cst_voice)
	{
		*v++ = &slt_voice;
		if (flite_voice == NULL)
			flite_voice = slt_cst_voice;
	}
#endif

#ifdef HAVE_REGISTER_CMU_US_AWB
	awb_cst_voice = register_cmu_us_awb();
	if (awb_cst_voice)
	{
		*v++ = &awb_voice;
		if (flite_voice == NULL)
			flite_voice = awb_cst_voice;
	}
#endif

#ifdef HAVE_REGISTER_CMU_US_KAL16
	kal16_cst_voice = register_cmu_us_kal16();
	if (kal16_cst_voice)
	{
		*v++ = &kal16_voice;
		if (flite_voice == NULL)
			flite_voice = kal16_cst_voice;
	}
#endif

#ifdef HAVE_REGISTER_CMU_US_KAL
	kal_cst_voice = register_cmu_us_kal();
	if (kal_cst_voice)
	{
		*v++ = &kal_voice;
		if (flite_voice == NULL)
			flite_voice = kal_cst_voice;
	}
#endif

	if (flite_voice == NULL) {
		DBG("Couldn't register a voice.\n");
		*status_info = g_strdup("Can't register a voice. "
					"Seems your FLite installation is incomplete.");
		return -1;
	}

	DBG("FliteMaxChunkLength = %d\n", FliteMaxChunkLength);
	DBG("FliteDelimiters = %s\n", FliteDelimiters);

	flite_speaking = 0;

	buf = (char *)g_malloc((FliteMaxChunkLength + 1) * sizeof(char));

	*status_info = g_strdup("Flite initialized successfully.");

	return 0;
}

SPDVoice **module_list_voices(void)
{
	return voices;
}

void module_speak_sync(const gchar * data, size_t len, SPDMessageType msgtype)
{
	DBG("Requested data: |%s|\n", data);

	if (flite_speaking) {
		module_speak_error();
		DBG("Speaking when requested to write");
		return;
	}

	flite_speaking = 1;

	AudioTrack track;
#if defined(BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)
	AudioFormat format = SPD_AUDIO_BE;
#else
	AudioFormat format = SPD_AUDIO_LE;
#endif
	cst_wave *wav;
	unsigned int pos, first;
	int bytes;
	char *flite_message;

	flite_message = module_strip_ssml(data);
	/* TODO: use a generic engine for SPELL, CHAR, KEY */

	module_speak_ok();

	/* Setting voice */
	UPDATE_STRING_PARAMETER(voice.name, flite_set_voice);
	UPDATE_PARAMETER(rate, flite_set_rate);
	UPDATE_PARAMETER(volume, flite_set_volume);
	UPDATE_PARAMETER(pitch, flite_set_pitch);

	pos = 0;
	first = 1;

	module_report_event_begin();
	while (1) {
		/* Process server events in case we were told to stop in between */
		module_process(STDIN_FILENO, 0);

		if (flite_stop) {
			DBG("Stop in child, terminating");
			module_report_event_stop();
			break;
		}

		bytes =
		    module_get_message_part(flite_message, buf, &pos,
					    FliteMaxChunkLength,
					    FliteDelimiters);

		if (bytes < 0) {
			DBG("End of message");
			module_report_event_end();
			break;
		}

		if (bytes == 0) {
			DBG("No data");
			module_report_event_end();
			break;
		}

		buf[bytes] = 0;
		DBG("Returned %d bytes from get_part\n", bytes);
		DBG("Text to synthesize is '%s'\n", buf);

		DBG("Trying to synthesize text");
		wav = flite_text_to_wave(buf, flite_voice);

		if (wav == NULL) {
			DBG("Stop in child, terminating");
			module_report_event_stop();
			break;
		}

		track.num_samples = wav->num_samples;
		track.num_channels = wav->num_channels;
		track.sample_rate = wav->sample_rate;
		track.bits = 16;
		track.samples = wav->samples;

		if (first) {
			module_strip_head_silence(&track);
			first = 0;
		}
		if (flite_message[pos] == 0)
			module_strip_tail_silence(&track);

		DBG("Got %d samples", track.num_samples);
		if (track.samples != NULL) {
			DBG("Sending part of the message");
			module_tts_output_server(&track, format);
		}
		delete_wave(wav);
	}
	g_free(flite_message);
	flite_speaking = 0;
	flite_stop = 0;
}

int module_stop(void)
{
	DBG("flite: stop()\n");

	if (flite_speaking)
		flite_stop = 1;

	return 0;
}

size_t module_pause(void)
{
	DBG("pause requested\n");
	if (flite_speaking) {
		DBG("Flite doesn't support pause, stopping\n");

		module_stop();

		return -1;
	} else {
		return 0;
	}
}

int module_close(void)
{

	DBG("flite: close()\n");

	DBG("Stopping speech");
	if (flite_speaking) {
		module_stop();
	}

	g_free(kal16_cst_voice);
	kal16_cst_voice = NULL;
	g_free(kal_cst_voice);
	kal_cst_voice = NULL;
	g_free(awb_cst_voice);
	awb_cst_voice = NULL;
	g_free(rms_cst_voice);
	rms_cst_voice = NULL;
	g_free(slt_cst_voice);
	slt_cst_voice = NULL;
	g_free(buf);
	buf = NULL;

	return 0;
}

/* Internal functions */

static void flite_set_voice(const char *name)
{
	if (!strcmp(name, "kal16"))
		flite_voice = kal16_cst_voice;
	else if (!strcmp(name, "kal"))
		flite_voice = kal_cst_voice;
	else if (!strcmp(name, "awb"))
		flite_voice = awb_cst_voice;
	else if (!strcmp(name, "rms"))
		flite_voice = rms_cst_voice;
	else if (!strcmp(name, "slt"))
		flite_voice = slt_cst_voice;
	DBG("Unknown voice %s", name);
}

static void flite_set_rate(signed int rate)
{
	float stretch = 1;

	assert(rate >= -100 && rate <= +100);
	if (rate < 0)
		stretch -= ((float)rate) / 50;
	if (rate > 0)
		stretch -= ((float)rate) / 175;
	feat_set_float(flite_voice->features, "duration_stretch", stretch);
}

static void flite_set_volume(signed int volume)
{
	assert(volume >= -100 && volume <= +100);
	flite_volume = volume;
}

static void flite_set_pitch(signed int pitch)
{
	float f0;

	assert(pitch >= -100 && pitch <= +100);
	f0 = (((float)pitch) * 0.8) + 100;
	feat_set_float(flite_voice->features, "int_f0_target_mean", f0);
}
