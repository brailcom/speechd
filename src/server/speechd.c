
/*
 * speechd.c - Speech Dispatcher server program
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
 * $Id: speechd.c,v 1.28 2003-05-26 16:04:50 hanke Exp $
 */

#include "speechd.h"

/* Declare dotconf functions and data structures*/
#include "dc_decl.h"

/* Declare functions to allocate and create important data
 * structures */
#include "alloc.h"

int server_socket;

gint (*p_msg_nto_speak)() = message_nto_speak;

/* Logging messages, level of verbosity is defined between 1 and 5, 
 * see documentation */
void
MSG(int level, char *format, ...)
{
    if(level <= LOG_LEVEL){
        va_list args;
        int i;
        char *format_with_spaces;
		
        format_with_spaces = (char*) spd_malloc(sizeof(char)*(strlen(format) + 3*level + 1));
        format_with_spaces[0] = '\0';
        for(i=1;i<level;i++){
            strcat(format_with_spaces, "  ");
        }
        strcat(format_with_spaces, format);
        strcat(format_with_spaces, "\n");
        va_start(args, format_with_spaces);
        vfprintf(logfile, format_with_spaces, args);
        if(SPEECHD_DEBUG) vfprintf(stdout, format_with_spaces, args);
        va_end(args);

    }				
}

gboolean
speechd_client_terminate(gpointer key, gpointer value, gpointer user)
{
	TFDSetElement *set;

	set = (TFDSetElement *) value;
	if (set == NULL){
		MSG(2, "Empty connection, internal error");
		if(SPEECHD_DEBUG) FATAL("Internal error");
		return TRUE;
	}	

	if(set->fd>0){
		MSG(3,"Closing connection on fd %d\n", set->fd);
		speechd_connection_destroy(set->fd);
	}
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
    assert(module->name!=NULL);
    MSG(4,"Terminating a module %s", module->name);
    (*module->close) ();
			
    g_module_close((GModule*) (module->gmodule));
	
    return TRUE;
}
	
void
speechd_quit(int sig)
{
	int ret;
	
	MSG(1, "Terminating...");

	MSG(1, "Closing open connections...");
	/* We will browse through all the connections and close them. */
	g_hash_table_foreach_remove(fd_settings, speechd_client_terminate, NULL);
	g_hash_table_destroy(fd_settings);
	
	MSG(1,"Closing open output modules...");
	/*  Call the close() function of each registered output module. */
	g_hash_table_foreach_remove(output_modules, speechd_modules_terminate, NULL);
	g_hash_table_destroy(output_modules);
	
	MSG(1,"Closing server connection...");
	if(close(server_socket) == -1) MSG(2, "close() failed: %s", strerror(errno));
	FD_CLR(server_socket, &readfds);
	
	MSG(1,"Closing speak() thread...");
	ret = pthread_cancel(speak_thread);
	if(ret != 0) FATAL("Speak thread failed to cancel!\n");
	
	ret = pthread_join(speak_thread, NULL);
	if(ret != 0) FATAL("Speak thread failed to join!\n");
	
	fflush(NULL);

	exit(0);	
}

void
speechd_init()
{
	configfile_t *configfile = NULL;
	char *configfilename = SYS_CONF"/speechd.conf" ;
	int ret;
        char *p;
        int i;
        int v;

	msgs_to_say = 0;
	max_uid = 0;

	/* Initialize logging */
	logfile = malloc(sizeof(FILE));
	logfile = stdout;
	
	/* Initialize Speech Dispatcher priority queue */
	MessageQueue = (TSpeechDQueue*) speechd_queue_alloc();
	if (MessageQueue == NULL) FATAL("Couldn't alocate memmory for MessageQueue.");

	/* Initialize lists */
	MessagePausedList = NULL;
	message_history = NULL;

	/* Initialize hash tables */
	fd_settings = g_hash_table_new(g_int_hash,g_int_equal);
	assert(fd_settings != NULL);

	fd_uid = g_hash_table_new(g_str_hash, g_str_equal);
	assert(fd_uid != NULL);
	
	snd_icon_langs = g_hash_table_new(g_str_hash, g_str_equal);
	assert(snd_icon_langs != NULL);

	output_modules = g_hash_table_new(g_str_hash, g_str_equal);
	assert(output_modules != NULL);

        o_bytes = (int*) spd_malloc(16*sizeof(int));
        o_buf = (GString**) spd_malloc(16*sizeof(GString*));
        awaiting_data = (int*) spd_malloc(16*sizeof(int));
        fds_allocated = 16;

        for(i=0;i<=15;i++) awaiting_data[i] = 0;              

        /* Initialize lists of available tables */
        tables.sound_icons = NULL;
        tables.spelling = NULL;
        tables.characters = NULL;
        tables.keys = NULL;
        tables.punctuation = NULL;

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
	configfile = dotconf_create(configfilename, options, 0, CASE_INSENSITIVE);
	if (!configfile) DIE ("Error opening config file\n");
	if (dotconf_command_loop(configfile) == 0) DIE("Error reading config file\n");
	dotconf_cleanup(configfile);
	MSG(1,"Configuration has been read from \"%s\"", configfilename);
	
	/* Check for output modules */
	if (g_hash_table_size(output_modules) == 0){
		DIE("No output modules were loaded - aborting...");
	}else{
		MSG(1,"Speech Dispatcher started with %d output module%s",
			g_hash_table_size(output_modules),
			g_hash_table_size(output_modules) > 1 ? "s" : "" );
	}
}

/* activity is on server_socket (request for a new connection) */
int
speechd_connection_new(int server_socket)
{
	TFDSetElement *new_fd_set;
	struct sockaddr_in client_address;
	unsigned int client_len = sizeof(client_address);
	int client_socket;
	int v;
	int *p_client_socket, *p_client_uid;
	   
	client_socket = accept(server_socket, (struct sockaddr *)&client_address,
	  	&client_len);

	if(client_socket == -1){
		MSG(2,"Can't handle connection request of a new client");
		return -1;
	}
	
	/* We add the associated client_socket to the descriptor set. */
	FD_SET(client_socket, &readfds);
	if (client_socket > fdmax) fdmax = client_socket;
	MSG(3,"Adding client on fd %d", client_socket);

        /* Check if there is space for server status data; allocate it */
        if(client_socket >= fds_allocated){
            o_bytes = (int*) realloc(o_bytes, client_socket * 2 * sizeof(int));
            awaiting_data = (int*) realloc(awaiting_data, client_socket * 2 * sizeof(int));
            o_buf = (GString**) realloc(o_buf, client_socket * 2 * sizeof(GString*));
        }

        awaiting_data[client_socket] = 0;

	/* Create a record in fd_settings */
	new_fd_set = (TFDSetElement *) default_fd_set();
	if (new_fd_set == NULL){
		MSG(2,"Failed to create a record in fd_settings for the new client");
		if(fdmax == client_socket) fdmax--;
		FD_CLR(client_socket, &readfds);
		return -1;
	}
	new_fd_set->fd = client_socket;
	new_fd_set->uid = ++max_uid;
	p_client_socket = (int*) spd_malloc(sizeof(int));
	p_client_uid = (int*) spd_malloc(sizeof(int));
	*p_client_socket = client_socket;
	*p_client_uid = max_uid;
	g_hash_table_insert(fd_settings, p_client_uid, new_fd_set);
	g_hash_table_insert(fd_uid, p_client_socket, p_client_uid);
		

	MSG(3,"Data structures for client on fd %d created", client_socket);
	return 0;
}

int
speechd_connection_destroy(int fd)
{
	TFDSetElement *fdset_element;
	GList *gl;
	int v;
	
	/* Client has gone away and we remove it from the descriptor set. */
	MSG(3,"Removing client on fd %d", fd);

	MSG(4,"Tagging client as inactive in settings");
	fdset_element = get_client_settings_by_fd(fd);
	if(fdset_element != NULL){
		fdset_element->fd = -1;
		fdset_element->active = 0;
	}else if(SPEECHD_DEBUG){
	 	DIE("Can't find settings for this client\n");
	}

	MSG(4,"Removing client from the fd->uid table.");
	g_hash_table_remove(fd_uid, &fd);
		
        awaiting_data[fd] = 0;
	MSG(3,"Closing clients file descriptor %d", fd);

	if(close(fd) != 0)
		if(SPEECHD_DEBUG) DIE("Can't close file descriptor associated to this client");
	   	
	FD_CLR(fd, &readfds);
	if (fd == fdmax) fdmax--; /* this may not apply in all cases, but this is sufficient ... */

        MSG(3,"Connection closed");

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

	MSG(1,"Creating new thread for speak()");
	ret = pthread_create(&speak_thread, NULL, speak, NULL);
	if(ret != 0) FATAL("Speak thread failed!\n");

	/* Initialize socket functionality */
	server_socket = socket(AF_INET, SOCK_STREAM, 0);

	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(SPEECH_PORT);

	MSG(1,"Openning a socket connection");
	if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1){
		MSG(1, "bind() failed: %s", strerror(errno));
		FATAL("Couldn't open socket, try a few minutes later.");
	}
	
	/* Create a connection queue and initialize readfds to handle input from server_socket. */
	if (listen(server_socket, 5) == -1) FATAL("listen() failed");
	FD_ZERO(&readfds);
	FD_SET(server_socket, &readfds);
	fdmax = server_socket;

   /* Now wait for clients and requests. */   
	MSG(1, "Speech server waiting for clients ...");
	while (1) {
		testfds = readfds;

		if (select(FD_SETSIZE, &testfds, (fd_set *)0, (fd_set *)0, NULL) >= 1){
			/* Once we know we've got activity,
			* we find which descriptor it's on by checking each in turn using FD_ISSET. */

			for (fd = 0; fd <= fdmax && fd < FD_SETSIZE; fd++) {
		 		if (FD_ISSET(fd,&testfds)){
					MSG(4,"Activity on fd %d ...",fd);
				
					if (fd == server_socket){ 
						/* server activity (new client) */
						ret = speechd_connection_new(server_socket);
						if (ret!=0){
						  	MSG(2,"Failed to add new client");
							if (SPEECHD_DEBUG) FATAL("Failed to add new client");
						}						
				    }else{	
						/* client activity */
						int nread;
						ioctl(fd, FIONREAD, &nread);
	
						if (nread == 0) {
							/* client has gone */
							speechd_connection_destroy(fd);
							if (ret!=0) MSG(2,"Failed to close the client");
						}else{
							/* client sends some commands or data */
							if (serve(fd) == -1) MSG(2,"Failed to serve client on fd %d!",fd);
						}
					}
				}
			}
		}
	}
}


