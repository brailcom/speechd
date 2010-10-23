
/*
 * speechd_types.h - types for Speech Dispatcher
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SPEECHD_TYPES_H
#define SPEECHD_TYPES_H

typedef enum {
    SPD_PUNCT_ALL = 0,
    SPD_PUNCT_NONE = 1,
    SPD_PUNCT_SOME = 2
} SPDPunctuation;

typedef enum {
    SPD_CAP_NONE = 0,
    SPD_CAP_SPELL = 1,
    SPD_CAP_ICON = 2
} SPDCapitalLetters;

typedef enum {
    SPD_SPELL_OFF = 0,
    SPD_SPELL_ON = 1
} SPDSpelling;

typedef enum {
    SPD_MALE1 = 1,
    SPD_MALE2 = 2,
    SPD_MALE3 = 3,
    SPD_FEMALE1 = 4,
    SPD_FEMALE2 = 5,
    SPD_FEMALE3 = 6,
    SPD_CHILD_MALE = 7,
    SPD_CHILD_FEMALE = 8
} SPDVoiceType;

typedef enum
    {
	SORT_BY_TIME = 0,
	SORT_BY_ALPHABET = 1
    }ESort;

typedef enum
    {
	MSGTYPE_TEXT = 0,
	MSGTYPE_SOUND_ICON = 1,
	MSGTYPE_CHAR = 2,
	MSGTYPE_KEY = 3,
	MSGTYPE_SPELL = 99
    }EMessageType;

typedef enum
    {
	NOTIFY_NOTHING = 0,
	NOTIFY_BEGIN = 1,
	NOTIFY_END = 2,
	NOTIFY_IM = 4,
	NOTIFY_CANCEL = 8,
	NOTIFY_PAUSE = 16,
	NOTIFY_RESUME = 32
    }ENotification;

typedef struct {
  char* name;
  char* language;
  char* dialect;
}VoiceDescription;

#endif /* not ifndef SPEECHD_TYPES */
