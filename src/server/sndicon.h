
/*
 * sndicon.h - Header file for sndicon.c
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
 * $Id: sndicon.h,v 1.6 2003-07-09 14:04:59 pdm Exp $
 */

#ifndef SNDICON_H
#define SNDICON_H

#include <glib.h>

int sndicon_queue(int fd, char* language, char* prefix, char* name);
char* snd_icon_spelling_get(char* table, GHashTable *icons, char* name, int *sound);

int sndicon_icon(int fd, char *name);
int sndicon_char(int fd, char *name);
int sndicon_key(int fd, char *name);

char* sndicon_list_tables(GList *table_list);

char* sndicon_list_spelling_tables();
char* sndicon_list_sound_tables();
char* sndicon_list_char_tables();
char* sndicon_list_key_tables();
char* sndicon_list_punctuation_tables();		

#endif /* SNDICON_H */		
