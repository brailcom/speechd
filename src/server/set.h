
/*
 * set.h - Settings related functions for Speech Deamon header
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
 * $Id: set.h,v 1.4 2003-03-23 21:22:14 hanke Exp $
 */

#ifndef ALLOC_H
 #define ALLOC_H

#include "speechd.h"
#include "history.h"

int set_priority(int fd, int priority);
int set_language(int fd, char *language);
int set_rate(int fd, int rate);
int set_pitch(int fd, int pitch);
int set_punct_mode(int fd, int punct);
int set_cap_let_recog(int fd, int recog);
int set_spelling(int fd, int spelling);

TFDSetElement* default_fd_set(void);
		

#endif		
		
		
		
		
