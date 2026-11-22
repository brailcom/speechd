/*
 * options.h - Defines possible command line options
 *
 * Copyright (C) 2003 Brailcom, o.p.s.
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
 * $Id: options.h,v 1.10 2006-07-11 16:12:27 hanke Exp $
 */

#include "speechd_types.h"

extern signed int rate;
extern signed int pitch;
extern signed int pitch_range;
extern signed int volume;

extern int list_output_modules;
extern char *output_module;
extern char *sound_icon;
extern char *language;
extern char *voice_type;
extern char *punctuation_mode;
extern char *priority;
extern int pipe_mode;
extern int character;
extern int key;
extern SPDDataMode ssml_mode;
extern int spelling;
extern int wait_till_end;
extern int stop_previous;
extern int cancel_previous;
extern int list_synthesis_voices;
extern char *synthesis_voice;

extern char *application_name;
extern char *connection_name;

int options_parse(int argc, char *argv[]);
void options_print_version(void);
void options_print_help(char *argv[]);
