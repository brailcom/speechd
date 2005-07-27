
/*
 * libspeechd.h - Shared library for easy acces to Speech Dispatcher functions (header)
 *
 * Copyright (C) 2001, 2002, 2003, 2004 Brailcom, o.p.s.
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
 * $Id: libspeechd.h,v 1.19 2005-07-27 15:47:07 hanke Exp $
 */


#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

/* Debugging */
FILE* spd_debug;

/* Unless there is an fatal error, it doesn't print anything */
#define SPD_FATAL(msg) { printf("Fatal error (libspeechd) [%s:%d]:"msg, __FILE__, __LINE__); exit(EXIT_FAILURE); }

/* Arguments for spd_send_data() */
#define SPD_WAIT_REPLY 1              /* Wait for reply */
#define SPD_NO_REPLY 0               /* No reply requested */


/* --------------------- Public data types ------------------------ */

typedef enum{
    SPD_PUNCT_ALL = 0,
    SPD_PUNCT_NONE = 1,
    SPD_PUNCT_SOME = 2
}SPDPunctuation;

typedef enum{
    SPD_CAP_NONE = 0,
    SPD_CAP_SPELL = 1,
    SPD_CAP_ICON = 2
}SPDCapitalLetters;

typedef enum{
    SPD_SPELL_OFF = 0,
    SPD_SPELL_ON = 1
}SPDSpelling;

typedef enum{
    SPD_MALE1 = 1,
    SPD_MALE2 = 2,
    SPD_MALE3 = 3,
    SPD_FEMALE1 = 4,
    SPD_FEMALE2 = 5,
    SPD_FEMALE3 = 6,
    SPD_CHILD_MALE = 7,
    SPD_CHILD_FEMALE = 8
}SPDVoiceType;

typedef enum{
    SPD_IMPORTANT = 1,
    SPD_MESSAGE = 2,
    SPD_TEXT = 3,
    SPD_NOTIFICATION = 4,
    SPD_PROGRESS = 5
}SPDPriority;

/* -------------- Public functions --------------------------*/

/* Openning and closing Speech Dispatcher connection */
int spd_open(const char* client_name, const char* connection_name, const char *user_name);
void spd_close(int connection);

/* Speaking */
int spd_say(int connection, SPDPriority priority, const char* text);
int spd_sayf(int fd, SPDPriority priority, const char *format, ...);

/* Speech flow */
int spd_stop(int connection);
int spd_stop_all(int connection);
int spd_stop_uid(int connection, int target_uid);

int spd_cancel(int connection);
int spd_cancel_all(int connection);
int spd_cancel_uid(int connection, int target_uid);

int spd_pause(int connection);
int spd_pause_all(int connection);
int spd_pause_uid(int connection, int target_uid);

int spd_resume(int connection);
int spd_resume_all(int connection);
int spd_resume_uid(int connection, int target_uid);

/* Characters and keys */
int spd_key(int connection, SPDPriority priority, const char *key_name);
int spd_char(int connection, SPDPriority priority, const char *character);
int spd_wchar(int connection, SPDPriority priority, wchar_t wcharacter);

/* Sound icons */
int spd_sound_icon(int connection, SPDPriority priority, const char *icon_name);

/* Setting parameters */
int spd_set_voice_type(int connection, SPDVoiceType type);
int spd_set_voice_type_all(int connection, SPDVoiceType type);
int spd_set_voice_type_uid(int connection, SPDVoiceType type, unsigned int uid);

int spd_set_voice_rate(int connection, signed int rate);
int spd_set_voice_rate_all(int connection, signed int rate);
int spd_set_voice_rate_uid(int connection, signed int rate, unsigned int uid);

int spd_set_voice_pitch(int connection, signed int pitch);
int spd_set_voice_pitch_all(int connection, signed int pitch);
int spd_set_voice_pitch_uid(int connection, signed int pitch, unsigned int uid);

int spd_set_volume(int connection, signed int volume);
int spd_set_volume_all(int connection, signed int volume);
int spd_set_volume_uid(int connection, signed int volume, unsigned int uid);

int spd_set_punctuation(int connection, SPDPunctuation type);
int spd_set_punctuation_all(int connection, SPDPunctuation type);
int spd_set_punctuation_uid(int connection, SPDPunctuation type, unsigned int uid);

int spd_set_capital_letters(int connection, SPDCapitalLetters type);
int spd_set_capital_letters_all(int connection, SPDCapitalLetters type);
int spd_set_capital_letters_uid(int connection, SPDCapitalLetters type, unsigned int uid);

int spd_set_spelling(int connection, SPDSpelling type);
int spd_set_spelling_all(int connection, SPDSpelling type);
int spd_set_spelling_uid(int connection, SPDSpelling type, unsigned int uid);

int spd_set_language(int connection, const char* language);
int spd_set_language_all(int connection, const char* language);
int spd_set_language_uid(int connection, const char* language, unsigned int uid);

int spd_set_output_module(int connection, const char* output_module);
int spd_set_output_module_all(int connection, const char* output_module);
int spd_set_output_module_uid(int connection, const char* output_module, unsigned int uid);

/* Direct SSIP communication */
int spd_execute_command(int connection, char* command);
char* spd_send_data(int fd, const char *message, int wfr);



#ifdef __cplusplus
}
#endif /* __cplusplus */
