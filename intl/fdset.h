
/*
 * fdset.h - Settings for Speech Deamon
 *
 * Copyright (C) 2001,2002,2003 Ceska organizace pro podporu free software
 * (Czech Free Software Organization), Prague 2, Vysehradska 3/255, 128 00,
 * <freesoft@freesoft.cz>
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
 * $Id: fdset.h,v 1.4 2003-02-01 22:16:55 hanke Exp $
 */

typedef enum {                  // type of voice
        MALE = 0,               // (numbers divisible by 2 should mean male)
        FEMALE = 1,
        CHILD_MALE = 2,
        CHILD_FEMALE = 3
}EVoiceType;

typedef struct{
        int fd;
		int uid;
        int paused;
        int priority;           // priority between 1 and 3
        int punctuation_mode;   // this will of course not be integer
        int speed;              // speed: 100 = normal, ???
        int pitch;              // pitch: ???
        char *client_name;
        char *language;         // language: default = english
        char *output_module;    // output module: festival, odmluva, epos,...
        EVoiceType voice_type;  // see EVoiceType definition
        int spelling;           // spelling mode: 0 -- off, 1 -- on
        int cap_let_recogn;     // cap. letters recogn.: 0 -- off, 1 -- on
}TFDSetElement;

