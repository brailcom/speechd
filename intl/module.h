
/*
 * module.h - Definition of a module for Speech Deamon
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
 * $Id: module.h,v 1.5 2003-02-01 22:16:55 hanke Exp $
 */
typedef struct {
   gchar    *name;
   gchar    *description;
   gchar    *filename;
   gint     (*write)    (const gchar *, gint, void*);
   gint     (*stop)     (void);
   gint     (*pause)    (void);
   gint     (*close)    (void);
   gint     (*is_speaking) (void);
} OutputModule;

typedef OutputModule entrypoint (void);

#define BUF_SIZE 4096
