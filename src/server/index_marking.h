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
 * $Id: index_marking.h,v 1.3 2004-06-28 08:11:13 hanke Exp $
 */

#include "speechd.h"

#ifndef INDEX_MARKING_H
#define INDEX_MARKING_H

/* Insert index marks into a message. */
void insert_index_marks(TSpeechDMessage *msg, int ssml_mode);

/* Find the index mark specified as _mark_ and return the
rest of the text after that index mark. */
char* find_index_mark(TSpeechDMessage *msg, int mark);

/* Delete all index marks from _buf_ and return a newly
   allocated string. */
char* strip_index_marks(char *buf, int ssml_mode);

#endif /* INDEX_MARKING_H */
