/*
 * index_marking.h -- Implements functions handling index marking
 *                    for Speech Dispatcher (header)
 * 
 * Copyright (C) 2001,2002,2003 Brailcom, o.p.s
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
 * $Id: index_marking.h,v 1.1 2003-09-20 17:31:24 hanke Exp $
 */

#include "speechd.h"

void insert_index_marks(TSpeechDMessage *msg);
char* find_next_index_mark(char *buf, int *mark, char **begin);
char* find_index_mark(TSpeechDMessage *msg, int mark);
char* strip_index_marks(char *buf);

