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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * $Id: index_marking.h,v 1.5 2006-07-11 16:12:27 hanke Exp $
 */

#include "speechd.h"

#ifndef INDEX_MARKING_H
#define INDEX_MARKING_H

#define SD_MARK_BODY_LEN 6
#define SD_MARK_BODY "__spd_"
#define SD_MARK_HEAD "<mark name=\""SD_MARK_BODY
#define SD_MARK_TAIL "\"/>"

/* Insert index marks into a message. */
void insert_index_marks(TSpeechDMessage * msg, SPDDataMode ssml_mode);

/* Find the index mark specified as _mark_ and return the
rest of the text after that index mark. */
char *find_index_mark(TSpeechDMessage * msg, int mark);

/* Delete all index marks from _buf_ and return a newly
   allocated string. */
char *strip_index_marks(char *buf, SPDDataMode ssml_mode);

#endif /* INDEX_MARKING_H */
