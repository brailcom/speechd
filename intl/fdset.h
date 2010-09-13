
/*
 * fdset.h - Settings for Speech Dispatcher
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
 *
 * $Id: fdset.h,v 1.33 2008-06-09 10:28:08 hanke Exp $
 */

#ifndef FDSET_H
#define FDSET_H

typedef enum 
    {                  /* Type of voice */
	NO_VOICE = 0,
	MALE1 = 1,
	MALE2 = 2,
	MALE3 = 3,
	FEMALE1 = 4,
	FEMALE2 = 5,
	FEMALE3 = 6,
	CHILD_MALE = 7,
	CHILD_FEMALE = 8
    }EVoiceType;

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
	RECOGN_NONE = 0,
	RECOGN_SPELL = 1,
	RECOGN_ICON = 2
    }ECapLetRecogn;

typedef enum
    {
	PUNCT_NONE = 0,
	PUNCT_ALL = 1,
	PUNCT_SOME = 2
    }EPunctMode;

typedef enum
    {
	SPELLING_OFF = 0,
	SPELLING_ON = 1
    }ESpellMode;

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

typedef struct{
    signed int rate;
    signed int pitch;
    signed int volume;
    
    EPunctMode punctuation_mode;
    ESpellMode spelling_mode;
    ECapLetRecogn cap_let_recogn;

    char* language;

    EVoiceType voice;
    char *synthesis_voice;
}SPDMsgSettings;

#endif /* not ifndef FDSET */
