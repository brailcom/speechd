
/*
 * clibrary2.c - Testing LIST and associated set functions in Speech Dispatcher
 *
 * Copyright (C) 2008 Brailcom, o.p.s.
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
 * $Id: clibrary2.c,v 1.1 2008-04-09 11:41:52 hanke Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>

#include "speechd_types.h"
#include "libspeechd.h"

#ifdef THOROUGH
static sem_t semaphore;

/* Callback for Speech Dispatcher notifications */
static void end_of_speech(size_t msg_id, size_t client_id, SPDNotificationType type)
{
	sem_post(&semaphore);
}
#endif

int main()
{
	SPDConnection *conn;
	int i, j, ret;
	char **modules;
	char **voices;
	char *module;
	char *language;
	int value;
	SPDVoiceType voice_type = SPD_CHILD_MALE;
	SPDVoice **synth_voices;
#ifndef THOROUGH
	SPDVoiceType voice_types[] = {
		SPD_MALE1,
		SPD_MALE2,
		SPD_MALE3,
		SPD_FEMALE1,
		SPD_FEMALE2,
		SPD_FEMALE3,
		SPD_CHILD_MALE,
		SPD_CHILD_FEMALE,
		SPD_UNSPECIFIED
	};
#endif

	printf("Start of the test.\n");

	printf("Trying to initialize Speech Daemon...\n");
	conn = spd_open("say", NULL, NULL,
#ifdef THOROUGH
			SPD_MODE_THREADED
#else
			SPD_MODE_SINGLE
#endif
			);
	if (conn == 0) {
		printf("Speech Daemon failed\n");
		exit(1);
	}
	printf("OK\n");

#ifdef THOROUGH
	sem_init(&semaphore, 0, 0);
	conn->callback_end = conn->callback_cancel = end_of_speech;
	spd_set_notification_on(conn, SPD_END);
	spd_set_notification_on(conn, SPD_CANCEL);
#endif

	printf("Trying to get the current output module...\n");
	module = spd_get_output_module(conn);
	printf("Got module %s\n", module);
	if (module == NULL) {
		printf("Can't get current output module\n");
		exit(1);
	}
	free(module);

	printf("Trying to get the language...\n");
	language = spd_get_language(conn);
	printf("Got language %s\n", language);
	if (language == NULL) {
		printf("Can't get the language\n");
		exit(1);
	}
	free(language);

	printf("Trying to get the voice rate...\n");
	value = spd_get_voice_rate(conn);
	printf("Got rate %d\n", value);

	printf("Trying to get the voice pitch...\n");
	value = spd_get_voice_pitch(conn);
	printf("Got pitch %d\n", value);

	printf("Trying to get the current volume...\n");
	value = spd_get_volume(conn);
	printf("Got volume %d\n", value);

	printf("Trying to get the current voice type...\n");
	spd_set_voice_type(conn, voice_type);
	voice_type = spd_get_voice_type(conn);
	printf("Got voice type %d\n", voice_type);

	modules = spd_list_modules(conn);
	if (modules == NULL) {
		printf("Can't list modules\n");
		exit(1);
	}

	printf("Available output modules:\n");
	for (i = 0;; i++) {
		if (modules[i] == NULL)
			break;
		printf("     %s\n", modules[i]);
	}

	voices = spd_list_voices(conn);
	if (voices == NULL) {
		printf("Can't list voices\n");
		exit(1);
	}

	printf("Available symbolic voices:\n");
	for (i = 0;; i++) {
		if (voices[i] == NULL)
			break;
		printf("     %s\n", voices[i]);
	}
	free_spd_symbolic_voices(voices);

	for (j = 0;; j++) {
		if (modules[j] == NULL)
			break;
		ret = spd_set_output_module(conn, modules[j]);
		if (ret == -1) {
			printf("spd_set_output_module failed\n");
			exit(1);
		}
#ifdef THOROUGH
		printf("\nListing voices for %s\n", modules[j]);
		synth_voices = spd_list_synthesis_voices(conn);
		if (synth_voices == NULL) {
			printf("Can't list voices\n");
			exit(1);
		}
		printf("Available synthesis voices:\n");
		for (i = 0;; i++) {
			if (synth_voices[i] == NULL)
				break;
			printf("     name: %s language: %s variant: %s\n",
			       synth_voices[i]->name, synth_voices[i]->language,
			       synth_voices[i]->variant);
			ret =
			    spd_set_synthesis_voice(conn,
						    synth_voices[i]->name);
			if (ret == -1) {
				printf("spd_set_synthesis_voice failed\n");
				exit(1);
			}
#else
		printf("\nIterating over voice types for %s\n", modules[j]);
		for (i = 0;; i++) {
			if (voice_types[i] == SPD_UNSPECIFIED)
				break;
			printf("     symbolic voice: %d\n",
			       voice_types[i]);
			ret =
			    spd_set_voice_type(conn, voice_types[i]);
			if (ret == -1) {
				printf("spd_set_voice_type failed\n");
				exit(1);
			}
#endif

			ret = spd_say(conn, SPD_TEXT, "test");
			if (ret == -1) {
				printf("spd_say failed\n");
				exit(1);
			}
#ifdef THOROUGH
			sem_wait(&semaphore);
#else
			sleep(1);
#endif
		}
#ifdef THOROUGH
		free_spd_voices(synth_voices);
#endif
	}

	free_spd_modules(modules);

	printf("Trying to close Speech Dispatcher connection...\n");
	spd_close(conn);
	printf("OK\n");

	printf("End of the test.\n");
	exit(0);
}
