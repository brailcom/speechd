
/*
 * history.h - History functions for Speech Deamon header
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
 * $Id: history.h,v 1.6 2003-04-14 02:07:14 hanke Exp $
 */

#ifndef ALLOC_H
 #define ALLOC_H

#include "speechd.h"

char* history_get_client_list();
char* history_get_message_list(guint client_id, int from, int num);
char* history_get_last(int fd);
char* history_cursor_set_last(int fd, guint client_id);
char* history_cursor_set_first(int fd, guint client_id);
char* history_cursor_set_pos(int fd, guint client_id, int pos);
char* history_cursor_next(int fd);
char* history_cursor_prev(int fd);
char* history_cursor_get(int fd);
char* history_say_id(int fd, int id);

/* Internal functions */

GList* get_messages_by_client(int uid);

#endif

