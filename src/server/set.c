
/*
 * set.c - Settings related functions for Speech Deamon
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
 * $Id: set.c,v 1.6 2003-02-01 22:16:55 hanke Exp $
 */

#include "speechd.h"
#include "history.h"

int
set_priority(int fd, int priority){
	GList *gl;
    TFDSetElement *settings;
	
	gl = g_list_find_custom(fd_settings, (int*) fd, p_fdset_lc_fd);
	if (gl == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings = gl->data;
	assert(gl->data != NULL);
	settings->priority = priority;
	return 1;
}

int
set_language(int fd, char *language){
	GList *gl;
    TFDSetElement *settings;
	
	gl = g_list_find_custom(fd_settings, (int*) fd, p_fdset_lc_fd);
	if (gl == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings = gl->data;
	free(settings->language);
	settings->language = (char*) malloc( strlen(language) * sizeof(char) + 1);
	if (settings->language == NULL) FATAL ("Not enough memmory.");
	strcpy(settings->language, language);
	return 1;
}

int
set_client_name(int fd, char *client_name){
	GList *gl;
    TFDSetElement *settings;
	THistoryClient *hclient;
										  
	gl = g_list_find_custom(fd_settings, (int*) fd, p_fdset_lc_fd);
	if (gl == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings = gl->data;
	free(settings->client_name);
	settings->client_name = (char*) malloc(strlen(client_name) * sizeof(char) + 1);
	strcpy(settings->client_name, client_name);

    gl = g_list_find_custom(history, (int*) fd, p_cli_comp_fd);
    if (gl == NULL) return 0;
    hclient = gl->data;
	hclient->client_name = (char*) malloc(strlen(client_name) * sizeof(char) + 1);
	strcpy(hclient->client_name, client_name);
	
	return 1;
}
		 
