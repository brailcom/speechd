
/*
 * libspeechd.h - Shared library for easy acces to Speech Dispatcher functions (header)
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
 * $Id: libspeechd.h,v 1.6 2003-06-08 21:51:54 hanke Exp $
 */

/* Debugging */
#define LIBSPEECHD_DEBUG
static FILE* _spd_debug;
void _SPD_DBG(char *format, ...);

/* Unless there is an fatal error, it doesn't print anything */
#define FATAL(msg) { printf("Fatal error (libspeechd) [%s:%d]:"msg, __FILE__, __LINE__); exit(EXIT_FAILURE); }

/* Arguments for _spd_send_data() */
#define _SPD_WAIT_REPLY 1              /* Wait for reply */
#define _SPD_NO_REPLY 0               /* No reply requested */


/* --------------------- Public data types ------------------------ */
typedef enum{
    SPD_PUNCT_ALL = 0,
    SPD_PUNCT_NONE = 1,
    SPD_PUNCT_SOME = 2
}SPDPunctuation;

typedef enum{
    SPD_SPELL_OFF = 0,
    SPD_SPELL_ON = 1
}SPDSpelling;


/* -------------- Public functions --------------------------*/

/* Openning and closing Speech Dispatcher connection */
int spd_open(char* client_name, char* connection_name, char *user_name);
void spd_close(int connection);

/* Speaking */
int spd_say(int connection, char* priority, char* text);
int spd_sayf(int fd, char* priority, char *format, ...);

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
int spd_key(int connection, char* priority, char *key_name);
int spd_char(int connection, char* priority, char *character);
int spd_wchar(int connection, char* priority, wchar_t wcharacter);

/* Sound icons */
int spd_sound_icon(int connection, char* priority, char *icon_name);

/* Setting parameters */
int spd_set_voice_rate(int connection, int rate);
int spd_set_voice_rate_all(int connection, int rate);
int spd_set_voice_rate_uid(int connection, int rate, int uid);

int spd_set_voice_pitch(int connection, int pitch);
int spd_set_voice_pitch_all(int connection, int pitch);
int spd_set_voice_pitch_uid(int connection, int pitch, int uid);

int spd_set_punctuation(int connection, SPDPunctuation type);
int spd_set_punctuation_all(int connection, SPDPunctuation type);
int spd_set_punctuation_uid(int connection, SPDPunctuation type, int uid);

int spd_set_punctuation_important(int connection, char* punctuation_important);
int spd_set_punctuation_important_all(int connection, char* punctuation_important);
int spd_set_punctuation_important_uid(int connection, char* punctuation_important,
                                      int uid);

int spd_set_spelling(int connection, SPDSpelling type);
int spd_set_spelling_all(int connection, SPDSpelling type);
int spd_set_spelling_uid(int connection, SPDSpelling type, int uid);



/* --------------  Private functions  ------------------------*/
int _spd_set_voice_rate(int connection, int rate, char* who);
int _spd_set_voice_pitch(int connection, int pitch, char *who);
int _spd_set_punctuation(int connection, SPDPunctuation type, char* who);
int _spd_set_punctuation_important(int connection, char* punctuation_important, char* who);
int _spd_set_spelling(int connection, SPDSpelling type, char* who);

char* send_data(int fd, char *message, int wfr);
int isanum(char* str);		
char* get_rec_str(char *record, int pos);
int get_rec_int(char *record, int pos);
char* parse_response_data(char *resp, int pos);
void *xmalloc(unsigned int bytes);
void xfree(void *ptr);
