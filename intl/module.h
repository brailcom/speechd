
/*
 * module.h - Definition of a module for Speech Dispatcher
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
 * $Id: module.h,v 1.19 2003-10-08 21:27:56 hanke Exp $
 */

#include <glib.h>
#include <gmodule.h>
#include <stdlib.h>
#include "fdset.h"

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
    char* name;
    char* filename;
    char* configfilename;
    int pipe_in[2];
    int pipe_out[2];    
    pid_t pid;
    int working;
}OutputModule;
