
/*
 * speechd.c - Speech Deamon server program
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
 * $Id: speechd.c,v 1.14 2003-03-23 21:14:16 hanke Exp $
 */

#include <signal.h>
#include "speechd.h"

/* Declare dotconf functions and data structures*/
#include "dc_decl.h"

/* Declare functions to allocate and create important data
 * structures */
#include "alloc.h"

int server_socket;
int max_uid = 0;

gint (*p_msg_nto_speak)() = message_nto_speak;
gint (*p_hc_lc)() = hc_list_compare;
gint (*p_msg_lc)() = message_list_compare_fd;

gboolean
speechd_client_terminate(gpointer key, gpointer value, gpointer user)
{
	TFDSetElement *set;

	set = (TFDSetElement *) value;
	if (set == NULL){
		MSG(2, "Empty connection, internal error\n");
		if(SPEECHD_DEBUG) FATAL("Internal error");
		return TRUE;
	}	

	MSG(2,"    Closing connection on fd %d\n", set->fd);
	if(close(set->fd) == -1) MSG(1, "close() failed: %s\n", strerror(errno));
    FD_CLR(set->fd, &readfds);

	mem_free_fdset(set);
	return TRUE;
}

gboolean
speechd_modules_terminate(gpointer key, gpointer value, gpointer user)
{
	OutputModule *module;

	module = (OutputModule *) value;
	if (module == NULL){
		MSG(2,"Empty module, internal error");
		if(SPEECHD_DEBUG) FATAL("Internal error");
		return TRUE;
	}
	g_module_close((GModule*) (module->gmodule));
	
	return TRUE;
}
	
void
speechd_quit(int sig)
{
	printf("Terminating...\n");
	MSG(3,"  Closing open connections...\n");

	/* We will browse through all the connections and close them. */
	g_hash_table_foreach_remove(fd_settings, speechd_client_terminate, NULL);
	g_hash_table_destroy(fd_settings);
	
	MSG(3,"  Closing open output modules...\n");
	/*  Call the close() function of each registered output module. */
	g_hash_table_foreach_remove(output_modules, speechd_modules_terminate, NULL);
	g_hash_table_destroy(output_modules);
	
	MSG(3,"  Closing server connection...\n");
	if(close(server_socket) == -1) MSG(1, "close() failed: %s\n", strerror(errno));

	fflush(NULL);
	exit(0);	
}

void
speechd_init()
{
	configfile_t *configfile = NULL;
	char *configfilename = SYS_CONF"/speechd.conf" ;
	int ret;

	msgs_to_say = 0;

	/* Initialize Speech Deamon priority queue */
	MessageQueue = (TSpeechDQueue*) speechd_queue_alloc();
	if (MessageQueue == NULL) FATAL("Couldn't alocate memmory for MessageQueue.");

	/* Initialize lists */
	MessagePausedList = NULL;
	history = NULL;
	history_settings = NULL;

	/* Initialize hash tables */
	fd_settings = g_hash_table_new(g_int_hash,g_int_equal);
	assert(fd_settings != NULL);
	
	snd_icon_langs = g_hash_table_new(g_str_hash,g_str_equal);
	assert(snd_icon_langs != NULL);

	output_modules = g_hash_table_new(g_str_hash, g_str_equal);
	assert(output_modules != NULL);

	/* Initialize arrays */
	awaiting_data = (GArray*) g_array_sized_new(1, 1, sizeof(int), 16);
	assert(awaiting_data != NULL);

	o_bytes = (GArray*) g_array_sized_new(1, 1, sizeof(long int), 16);
	assert(o_bytes != NULL);

	/* Perform some functionality tests */
	if (g_module_supported() == FALSE)
		DIE("Loadable modules not supported by current platform.\n");

	if(_POSIX_VERSION < 199506L)
		DIE("This system doesn't support POSIX.1c threads\n");

	/* Initialize mutexes */
	ret = pthread_mutex_init(&element_free_mutex, NULL);
	if(ret != 0)
		DIE("Mutex initialization failed");
	
	/* Load configuration from the config file*/
	MSG(5,"parsing configuration file \"%s\"\n", configfilename);
	configfile = dotconf_create(configfilename, options, 0, CASE_INSENSITIVE);
	if (!configfile) DIE ("Error opening config file\n");
	if (dotconf_command_loop(configfile) == 0) DIE("Error reading config file\n");
	dotconf_cleanup(configfile);
	
	/* Check for output modules */
	if (g_hash_table_size(output_modules) == 0){
		DIE("No output modules were loaded - aborting...");
	}else{
		MSG(3,"speech server started with %d output module%s\n",
			g_hash_table_size(output_modules),
			g_hash_table_size(output_modules) > 1 ? "s" : "" );
	}
}

/* activity is on server_socket (request for a new connection) */
int
speechd_connection_new(int server_socket)
{
	TFDSetElement *new_fd_set;
	THistSetElement *new_hist_set;
	THistoryClient *hnew_element;
	struct sockaddr_in client_address;
	unsigned int client_len = sizeof(client_address);
	int client_socket;
	int v;
	int *p;
	   
	client_socket = accept(server_socket, (struct sockaddr *)&client_address,
	  	&client_len);

	if(client_socket == -1){
		MSG(2,"Can't handle connection request of a new client");
		return -1;
	}
	
	/* We add the associated client_socket to the descriptor set. */
	FD_SET(client_socket, &readfds);
	if (client_socket > fdmax) fdmax = client_socket;
	MSG(3,"   adding client on fd %d\n", client_socket);

	g_array_set_size(awaiting_data, fdmax+2);
	/* Mark this client as ,,receiving commands'' */
	v = 0;
	g_array_insert_val(awaiting_data, client_socket, v);
											   		   
	/* Create a record in fd_settings */
	new_fd_set = (TFDSetElement *) default_fd_set();
	if (new_fd_set == NULL){
		MSG(2,"Failed to create a record in fd_settings for the new client");
		if(fdmax == client_socket) fdmax--;
		FD_CLR(client_socket, &readfds);
		return -1;
	}
	new_fd_set->fd = client_socket;
	new_fd_set->uid = max_uid;
	p = (int*) spd_malloc(sizeof(int));
	*p = client_socket;
	g_hash_table_insert(fd_settings, p, new_fd_set);
	max_uid++;				
			
	/* Create the corresponding history list and history settings record */
	hnew_element = (THistoryClient*) history_list_create_client(client_socket);
	if (hnew_element == NULL)
		MSG(2,"Failed to create a history client structure for this client");
	history = g_list_append(history, hnew_element);

	new_hist_set = default_history_settings();
	if (new_hist_set == NULL)
		MSG(2,"Failed to create a history settings element for this client");
	new_hist_set->fd = client_socket;			
	history_settings = g_list_append(history_settings, new_hist_set);

	MSG(3,"   data structures for client on fd %d created\n", client_socket);
	return 0;
}

int
speechd_connection_destroy(int fd)
{
	TFDSetElement *fdset_element;
	THistoryClient *hclient;
	GList *gl;
	int v;
	
	/* Client has gone away and we remove it from the descriptor set. */
	MSG(3,"   removing client on fd %d\n", fd);

	MSG(5,"       removing client from settings \n");
	fdset_element = (TFDSetElement*) g_hash_table_lookup(fd_settings, &fd);
	if(fdset_element != NULL){
		g_hash_table_remove(fd_settings, &fd);
		mem_free_fdset(fdset_element);
	}else if(SPEECHD_DEBUG){
	 	DIE("Can't find settings for this client\n");
	}

	MSG(5,"       tagging client as inactive in history \n");
	gl = g_list_find_custom(history, (int*) fd, p_cli_comp_fd);
	if ((gl != NULL) && (gl->data!=NULL)){
		hclient = gl->data;
		hclient->fd = -1;
		hclient->active = 0;
	}else if (SPEECHD_DEBUG){
	 	DIE("Can't find history for this client\n");	
	}
		
	v = 0;
	g_array_insert_val(awaiting_data, fd, v);
	MSG(5,"       closing clients file descriptor %d\n", fd);
	if(close(fd) != 0)
		if(SPEECHD_DEBUG) DIE("Can't close file descriptor associated to this client");
	   	
	FD_CLR(fd, &readfds);
	if (fd == fdmax) fdmax--; /* this may not apply in all cases, but this is sufficient ... */

	return 0;
}

int
main()
{
	struct sockaddr_in server_address;
	fd_set testfds;
	int i, v;
	int fd;
	int ret;

	/* Register signals */
	(void) signal(SIGINT, speechd_quit);	
	
	speechd_init();

	MSG(2,"creating new thread for speak()\n");
	ret = pthread_create(&speak_thread, NULL, speak, NULL);
	if(ret != 0) FATAL("Speak thread failed!\n");

	/* Initialize socket functionality */
	server_socket = socket(AF_INET, SOCK_STREAM, 0);

	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(SPEECH_PORT);

	if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1){
		MSG(1, "bind() failed: %s\n", strerror(errno));
		FATAL("Couldn't open socket");
	}
	
	/* Create a connection queue and initialize readfds to handle input from server_socket. */
	if (listen(server_socket, 5) == -1) FATAL("listen() failed");
	FD_ZERO(&readfds);
	FD_SET(server_socket, &readfds);
	fdmax = server_socket;

   /* Now wait for clients and requests. */   
	MSG(1, "speech server waiting for clients ...\n");
	while (1) {
		testfds = readfds;

		if (select(FD_SETSIZE, &testfds, (fd_set *)0, (fd_set *)0, NULL) >= 1){
			/* Once we know we've got activity,
			* we find which descriptor it's on by checking each in turn using FD_ISSET. */

			for (fd = 0; fd <= fdmax && fd < FD_SETSIZE; fd++) {
		 		if (FD_ISSET(fd,&testfds)){
					MSG(3,"  activity on fd %d ...\n",fd);
				
					if (fd == server_socket){ 
						/* server activity (new client) */
						ret = speechd_connection_new(server_socket);
						if (ret!=0){
						  	MSG(3,"   failed to add new client\n");
							if (SPEECHD_DEBUG) FATAL("failed to add new client");
						}						
				    }else{	
						/* client activity */
						int nread;
						ioctl(fd, FIONREAD, &nread);
	
						if (nread == 0) {
							/* client has gone */
							speechd_connection_destroy(fd);
							if (ret!=0) MSG(3,"   failed to close the client\n");
						}else{
							/* client sends some commands or data */
							if (serve(fd) == -1) MSG(1,"   failed to serve client on fd %d!\n",fd);
						}
					}
				}
			}
		}
	}
}


