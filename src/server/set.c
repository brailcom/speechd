#include "robod.h"

int
set_priority(int fd, int priority){
	GList *gl;
    TFDSetElement *settings;
	
	gl = g_list_find_custom(fd_settings, (int*) fd, p_fdset_lc);
	if (gl == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings = gl->data;
	settings->priority = priority;
	return 1;
}

int
set_language(int fd, char *language){
	GList *gl;
    TFDSetElement *settings;
	
	gl = g_list_find_custom(fd_settings, (int*) fd, p_fdset_lc);
	if (gl == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings = gl->data;
	free(settings->language);
	settings->language = (char*) malloc( strlen(language) * sizeof(char) + 1);
	if (settings->language == NULL) FATAL ("Not enough memmory.");
	strcpy(settings->language, language);
	return 1;
}

int
set_client_name(int fd, char *language){
	GList *gl;
    TFDSetElement *settings;
										  
	gl = g_list_find_custom(fd_settings, (int*) fd, p_fdset_lc);
	if (gl == NULL) FATAL("Couldnt find settings for active client, internal error.");
	settings = gl->data;
	free(settings->client_name);
	settings->client_name = (char*) malloc(strlen(language) * sizeof(char) + 1);
	strcpy(settings->client_name, language);
	return 1;
}
		 
