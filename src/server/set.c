
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
 * $Id: set.c,v 1.9 2003-03-24 22:50:03 hanke Exp $
 */


#include "set.h"

int
set_priority(int fd, int priority)
{
    TFDSetElement *settings;
	
	settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &fd);
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings->priority = priority;
	return 1;
}

int
set_rate(int fd, int rate)
{
    TFDSetElement *settings;
	
	settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &fd);
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings->speed = rate;
	return 1;
}

int
set_pitch(int fd, int pitch)
{
    TFDSetElement *settings;

	settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &fd);	
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings->pitch = pitch;
	return 1;
}

int
set_punct_mode(int fd, int punct)
{
    TFDSetElement *settings;
	
	settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &fd);
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings->punctuation_mode = punct;
	return 1;
}

int
set_cap_let_recog(int fd, int recog)
{
    TFDSetElement *settings;

	settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &fd);	
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings->cap_let_recogn = recog;
	return 1;
}

int
set_spelling(int fd, int spelling)
{
    TFDSetElement *settings;

	settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &fd);	
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings->spelling = spelling;
	return 1;
}

int
set_language(int fd, char *language){
    TFDSetElement *settings;

	settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &fd);	
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
	free(settings->language);
	settings->language = (char*) malloc(strlen(language) * sizeof(char) + 1);
	if (settings->language == NULL) FATAL ("Not enough memmory.");
	strcpy(settings->language, language);
	return 1;
}

int
set_client_name(int fd, char *client_name)
{
    TFDSetElement *settings;
	THistoryClient *hclient;
	GList *gl;
	
	settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &fd);		
	if (settings == NULL) FATAL("Couldnt find settings for active client, internal error.");
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
		 
TFDSetElement*
default_fd_set(void)
{
	TFDSetElement *new;

	new = (TFDSetElement*) spd_malloc(sizeof(TFDSetElement));
	new->language = (char*) spd_malloc(64);			/* max 63 characters */
	new->output_module = (char*) spd_malloc(64);
	new->client_name = (char*) spd_malloc(128);		/* max 127 characters */
   
	new->paused = 0;

	/* Fill with the global settings values */
	new->priority = GlobalFDSet.priority;
	new->punctuation_mode = GlobalFDSet.punctuation_mode;
	new->speed = GlobalFDSet.speed;
	new->pitch = GlobalFDSet.pitch;
	strcpy(new->language, GlobalFDSet.language);
	strcpy(new->output_module, GlobalFDSet.output_module);
	strcpy(new->client_name, GlobalFDSet.client_name); 
	new->voice_type = GlobalFDSet.voice_type;
	new->spelling = GlobalFDSet.spelling;         
	new->cap_let_recogn = GlobalFDSet.cap_let_recogn;

	return(new);
}
