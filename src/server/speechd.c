
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
 * $Id: speechd.c,v 1.12 2003-03-09 20:50:14 hanke Exp $
 */

#include <signal.h>
#include "speechd.h"
/* declare dotconf functions and data structures*/
#include "dc_decl.h"


int server_socket;
int max_uid = 0;

TSpeechDQueue* 
speechd_queue_alloc()
{
   TSpeechDQueue *new;
	
   new = malloc (sizeof(TSpeechDQueue));
   if (new == NULL) FATAL("Not enough memory!");
   new->p1 = NULL;
   new->p2 = NULL;
   new->p3 = NULL;
   return(new);
}


void*
fdset_list_alloc_element(void* element)
{
   TFDSetElement *new;
   new = malloc(sizeof(TFDSetElement));
   if (new == NULL) FATAL("Not enough memory!");
   *new = *((TFDSetElement*) element);

   new->language = malloc (strlen( ((TFDSetElement*) element)->language) );
   if (new->language == NULL) FATAL("Not enough memory!");
   strcpy(new->language, ((TFDSetElement*) element)->language);

   new->client_name = malloc (strlen( ((TFDSetElement*) element)->client_name) );
   if (new->client_name == NULL) FATAL("Not enough memory!");
   strcpy(new->client_name, ((TFDSetElement*) element)->client_name);

   new->output_module = malloc (strlen( ((TFDSetElement*) element)->output_module) );
   if (new->output_module == NULL) FATAL("Not enough memory!");
   strcpy(new->output_module, ((TFDSetElement*) element)->output_module);

   return (void*) new;
}

TSpeechDMessage*
history_list_alloc_message(TSpeechDMessage *old)
{
	TSpeechDMessage* new = NULL;

	new = (TSpeechDMessage *) malloc(sizeof(TSpeechDMessage));
	assert(new != NULL);
	new->buf = malloc(old->bytes);
	assert(new->buf!=NULL);
	*new = *old;
	memcpy(new->buf, old->buf, old->bytes);
	(TFDSetElement) new->settings = (TFDSetElement) old->settings; 
	new->settings.language = (char*) malloc(strlen(old->settings.language)+1);
	new->settings.client_name = (char*) malloc(strlen(old->settings.client_name)+1);
	new->settings.output_module = (char*) malloc(strlen(old->settings.output_module)+1);
	assert(new->settings.language != NULL);
	assert(new->settings.client_name != NULL);
	assert(new->settings.output_module != NULL);
	memcpy(new->settings.language, old->settings.language, strlen(old->settings.language));	
	memcpy(new->settings.client_name, old->settings.client_name, strlen(old->settings.client_name));	
	memcpy(new->settings.output_module, old->settings.output_module, strlen(old->settings.output_module));	
   return new;
}

void*
history_list_create_client(int fd)
{
   THistoryClient *new;
   TFDSetElement *settings;
   GList *gl;
   new = malloc(sizeof(THistoryClient));
   if (new == NULL) FATAL("Not enough memory!");
   gl = g_list_find_custom(fd_settings, (int*) fd, fdset_list_compare_fd);
   if (gl == NULL) FATAL("Couldn't find appropiate settings for active client."); 
   settings = gl->data;
   new->client_name = malloc(strlen(settings->client_name)+1);
   strcpy(new->client_name, settings->client_name);
   new->fd = fd;
   new->uid = settings->uid;
   new->active = 1;
   new->messages = NULL;

   return (void*) new;
}

THistSetElement*
default_hist_set(){
   THistSetElement* new;
   new = malloc(sizeof(THistSetElement));
   new->cur_pos = 1;
   new->sorted = BY_TIME;  
   return new;
}

TFDSetElement*
default_fd_set()
{
   TFDSetElement *new;

   new = malloc(sizeof(TFDSetElement));
   if (new == NULL) FATAL("Not enough memory!");
   new->language = malloc(16);
   if (new->language == NULL) FATAL("Not enough memory!");
   new->output_module = malloc(16);
   if (new->output_module == NULL) FATAL("Not enough memory!");
   new->client_name = malloc(32);
   if (new->client_name == NULL) FATAL("Not enough memory!");
   
   new->paused = 0;
   new->priority = GlobalFDSet.priority;
   new->punctuation_mode = GlobalFDSet.punctuation_mode;
   new->speed = GlobalFDSet.speed;
   new->pitch = GlobalFDSet.pitch;
   strcpy(new->language, GlobalFDSet.language);
   strcpy(new->output_module, GlobalFDSet.output_module);
   strcpy(new->client_name,GlobalFDSet.client_name); 
   new->voice_type = GlobalFDSet.voice_type;
   new->spelling = GlobalFDSet.spelling;         
   new->cap_let_recogn = GlobalFDSet.cap_let_recogn;

   return(new);
}

void
quit(int sig)
{
	int i;
	int ret;
	int clients_num;
	GList *gl = NULL;
	TFDSetElement *fdset;
	
	printf("Terminating...\n");
	MSG(2,"  Closing open connections...\n");
	clients_num = g_list_length(fd_settings);
	MSG(4,"   Connections: %d\n", clients_num);
	/* We will browse through all the connections and close them. */
	for(i=0;i<=clients_num-1;i++){
		gl = g_list_last(fd_settings);
		assert(gl!=NULL);
		fdset = (TFDSetElement*) gl->data;
		assert(fdset!=NULL);
		MSG(4,"    Closing connection on fd %d\n", fdset->fd);
		if(close(fdset->fd) == -1) MSG(1, "close() failed: %s\n", strerror(errno));
 		FD_CLR(fdset->fd, &readfds);	
		fd_settings = g_list_remove(fd_settings, gl);
	}
	if(close(server_socket) == -1) MSG(1, "close() failed: %s\n", strerror(errno));
	MSG(2,"  Freeing allocated memory...\n");
	/* TODO: obvious */
	fflush(NULL);
	exit(0);	
}


int
main()
{
   configfile_t *configfile = NULL;
   char *configfilename = SYS_CONF"/speechd.conf" ;
   struct sockaddr_in server_address;
   fd_set testfds;
   struct timeval tv;
   TFDSetElement *new_fd_set;
   THistSetElement *new_hist_set;
   THistoryClient *hnew_element;
   int v, i;
   int fd;
   GList *gl;
   TFDSetElement *fd_set_element;
   THistoryClient *hclient;

	(void) signal(SIGINT,quit);	
	
   msgs_to_say = 0;
   
   MessageQueue = speechd_queue_alloc();

   if (MessageQueue == NULL)
	FATAL("Couldn't alocate memmory for MessageQueue.");

   MessagePausedList = NULL;

   fd_settings = NULL;

   history = NULL;
   
   history_settings = NULL;
 
   snd_icon_langs = g_hash_table_new(g_str_hash,g_str_equal);

   awaiting_data = (GArray*) g_array_sized_new(1, 1, sizeof(int),20);
   g_array_set_size(awaiting_data,20);
   o_bytes = (GArray*) g_array_sized_new(1, 1, sizeof(int),20);
   g_array_set_size(o_bytes,20);
   
   if (g_module_supported() == FALSE)
      DIE("Loadable modules not supported by current platform.\n");

   output_modules = g_hash_table_new(g_str_hash, g_str_equal);

   MSG(3,"parsing configuration file \"%s\"\n", configfilename);

   configfile = dotconf_create(configfilename, options, 0, CASE_INSENSITIVE);
   if (!configfile)
      DIE ("Error opening config file\n");

   if (dotconf_command_loop(configfile) == 0)
      DIE("Error reading config file\n");

   dotconf_cleanup(configfile);
 
   if (g_hash_table_size(output_modules) == 0)
      DIE("No output modules were loaded - aborting...");

   MSG(3,"speech server started with %d output module%s\n", g_hash_table_size(output_modules), g_hash_table_size(output_modules) > 1 ? "s" : "" );

   server_socket = socket(AF_INET, SOCK_STREAM, 0);

   server_address.sin_family = AF_INET;
   server_address.sin_addr.s_addr = htonl(INADDR_ANY);
   server_address.sin_port = htons(SPEECH_PORT);

   if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1){
		MSG(1, "bind() failed: %s\n", strerror(errno));
		FATAL("Couldn't open socket");
	}

   /* Create a connection queue and initialize readfds to handle input from server_socket. */
   if (listen(server_socket, 5) == -1)
      FATAL("listen() failed");

   FD_ZERO(&readfds);
   FD_SET(server_socket, &readfds);
   fdmax = server_socket;

   /* Now wait for clients and requests. */
   
      MSG(1, "speech server waiting for clients ...\n");

    tv.tv_sec = 0;
    tv.tv_usec = 10;
   while (1) {
      testfds = readfds;

      if (select(FD_SETSIZE, &testfds, (fd_set *)0, (fd_set *)0, &tv) >= 1){
			  
      /* Once we know we've got activity,
       * we find which descriptor it's on by checking each in turn using FD_ISSET. */

      for (fd = 0; fd <= fdmax && fd < FD_SETSIZE; fd++) {

//	 MSG(5," testing fd %d (fdmax = %d)...\n", fd, fdmax);

	 if (FD_ISSET(fd,&testfds)) {

	    MSG(3,"  activity on fd %d ...\n",fd);

	    if (fd == server_socket) { /* activity is on server_socket (request for a new connection) */

	       struct sockaddr_in client_address;
	       unsigned int client_len = sizeof(client_address);
	       int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_len);

	       /* We add the associated client_socket to the descriptor set. */
	       FD_SET(client_socket, &readfds);
	       if (client_socket > fdmax) fdmax = client_socket;
	       MSG(3,"   adding client on fd %d\n", client_socket);

           g_array_set_size(awaiting_data, fdmax+2);
           v = 0;
           /* Mark this client as ,,receiving commands'' */
           g_array_insert_val(awaiting_data, client_socket, v);
											   		   
           new_fd_set = default_fd_set();
           new_fd_set->fd = client_socket;
		   new_fd_set->uid = max_uid;
		   max_uid++;
           fd_settings = g_list_append(fd_settings, new_fd_set);

	       hnew_element = history_list_create_client(client_socket);
	       history = g_list_append(history, hnew_element);

           new_hist_set = default_hist_set();
           new_hist_set->fd = client_socket;
	       history_settings = g_list_append(history_settings, new_hist_set);
 /*              new_hist_set = gdsl_list_get_tail(history_settings);
               MSG(3, "fd: fd: fd: %d", new_hist_set->fd); */

               MSG(3,"   configuration for %d added to the list\n", client_socket);
	
	    } else {	/* client activity */

	       int nread;
	       ioctl(fd, FIONREAD, &nread);

	       if (nread == 0) {
		  /* Client has gone away and we remove it from the descriptor set. */
		  MSG(3,"   removing client on fd %d\n", fd);
		  MSG(5,"      stopping client on fd %d\n", fd);
		  stop_from_client(fd);						
          gl = g_list_find_custom(fd_settings, (int*) fd, p_fdset_lc_fd);
		  assert(gl->data!=NULL);
		  fd_set_element = (TFDSetElement*) gl->data;
		  MSG(5,"       removing client from settings \n");
		  fd_settings = g_list_remove(fd_settings, gl->data);
		  MSG(5,"       tagging client as inactive in history \n");
    gl = g_list_find_custom(history, (int*) fd, p_cli_comp_fd);
    if (gl == NULL) return 0;
    hclient = gl->data;
    hclient->fd = -1;
    hclient->active = 0;
	v = 0;
  	g_array_insert_val(awaiting_data, fd, v);
		  MSG(5,"       closing clients file descriptor %d\n", fd);
		  close(fd); 
		  FD_CLR(fd, &readfds);
		  if (fd == fdmax) fdmax--; /* this may not apply in all cases, but this is sufficient ... */
	     } else {
		  /* Here we 'serve' the client. */
		  MSG(2,"   serving client on fd %d\n", fd);
		  if (serve(fd) == -1) MSG(1,"   failed to serve client on fd %d!\n",fd);
	       }
	    }
	 }
      }

    }
	if(msgs_to_say > 0){
		 	speak();
	}else{
		usleep(1);
	}
   }

   /* on exit ...
   for (i = 0; i < NUM_MODULES && handles[i]; i++)
      dlclose(handles[i]);
   g_hash_table_destroy(table);
   */
}


/* isanum() tests if the given string is a number,
 * returns 1 if yes, 0 otherwise. */
int isanum(char *str);
