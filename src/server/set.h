
/*
 * set.h - Settings related functions for Speech Dispatcher header
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
 * $Id: set.h,v 1.10 2003-08-11 15:01:40 hanke Exp $
 */

#ifndef ALLOC_H
 #define ALLOC_H

#include "speechd.h"
#include "history.h"

int set_priority_uid(int uid, int priority);
int set_language_uid(int uid, char *language);
int set_rate_uid(int uid, int rate);
int set_pitch_uid(int uid, int pitch);
int set_punct_mode_uid(int uid, int punct);
int set_cap_let_recog_uid(int uid, int recog);
int set_spelling_uid(int uid, int spelling);
int set_output_module_self(int uid, char *output_module);

int set_priority_self(int fd, int priority);
int set_language_self(int fd, char *language);
int set_rate_self(int fd, int rate);
int set_pitch_self(int fd, int pitch);
int set_punct_mode_self(int fd, int punct);
int set_cap_let_recog_self(int fd, int recog);
int set_spelling_self(int fd, int spelling);
int set_output_module_self(int fd, char *output_module);

int set_priority_all(int priority);
int set_language_all(char *language);
int set_rate_all(int rate);
int set_pitch_all(int pitch);
int set_punct_mode_all(int punct);
int set_cap_let_recog_all(int recog);
int set_spelling_all(int spelling);
int set_output_module_all(char* output_module);

TFDSetElement* default_fd_set(void);
		
void set_param_int(int* parameter, int value);
char* set_param_str(char* parameter, char* value);

#endif		
