
/*
 * module.h - Definition of a module for Speech Deamon
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
 * $Id: module.h,v 1.12 2003-05-18 20:54:10 hanke Exp $
 */

#include <glib.h>
#include <gmodule.h>

typedef struct
{
    char *male1;
    char *male2;
    char *male3;
    char *female1;
    char *female2;
    char *female3;
    char *child_male;
    char *child_female;
}SPDVoiceDef;

typedef struct{
    GHashTable *voices;
    GHashTable *params;
}SPDModuleSettings;

typedef struct{
    gchar    *name;
    gchar    *description;
    GModule  *gmodule;
    gint     (*write)       (gchar *, gint, void*);
    gint     (*stop)        (void);
    gchar*   (*pause)       (void);
    gint     (*is_speaking) (void);
    gint     (*close)       (void);
    SPDModuleSettings settings;
}OutputModule;

typedef OutputModule entrypoint (void);
