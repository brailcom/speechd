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
 * $Id: index_marking.h,v 1.2 2003-10-12 23:32:07 hanke Exp $
 */

#include "speechd.h"

#ifndef INDEX_MARKING_H
#define INDEX_MARKING_H

/* Insert index marks into a message and escape '@'.*/
void insert_index_marks(TSpeechDMessage *msg);

/* Finds next index mark in *buf and returns it's position
   in _begin_ and the position after the mark as the return
   value. The number of the mark is stored in _mark_*/
char* find_next_index_mark(char *buf, int *mark, char **begin);

/* Finds a specific index marks specified in _mark_. The
 return value is a pointer to the message after the mark. */
char* find_index_mark(TSpeechDMessage *msg, int mark);

/* Delete all index marks from _buf_ and de-escape '@'. Returns
   the new allocated text. */
char* strip_index_marks(char *buf);

#endif /* INDEX_MARKING_H */
