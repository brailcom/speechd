
/*
 * def.h - Some global definitions for Speech Deamon
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
 * $Id: def.h,v 1.7 2003-03-23 21:34:30 hanke Exp $
 */

/* some constants common for speech server and client part */

#ifndef SPEECHD_DEF_I
	#define SPEECHD_DEF_I

#define SPEECH_PORT 9876

#define LOG_LEVEL 2

#define SPEECHD_DEBUG 0

#define MSG(level,args...) { if (level <= LOG_LEVEL) printf(args); }

#define FATAL(msg) { printf("speechd [%s:%d]: "msg"\n", __FILE__, __LINE__); exit(EXIT_FAILURE); }

#define DIE(msg)   { perror("speechd: "msg); exit(EXIT_FAILURE); }


#endif
