/*
 * options.h - Defines possible command line options
 *
 * Copyright (C) 2003 Brailcom, o.p.s.
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
 * $Id: options.h,v 1.1 2003-07-17 11:59:44 hanke Exp $
 */

#include <getopt.h>

static struct option spd_long_options[] = {
    {"run-daemon", 0, 0, 'd'},
    {"run-single", 0, 0, 's'},
    {"log-level", 1, 0, 0},
    {"port", 1, 0, 0},
    {"help", 0, 0, 0},
    {0, 0, 0, 0}
};

static char* spd_short_options = "dshl:p:";
