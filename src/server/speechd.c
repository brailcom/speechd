/* Speechd server program
 * CVS revision: $Id: speechd.c,v 1.7 2002-12-08 20:21:52 hanke Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

#include "speechd.h"
/* declare dotconf functions and data structures*/
#include "dc_decl.h"

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
   if (new == NULL) FATAL("Not enough memory!");
   new->buf = malloc(old->bytes);
   if (new->buf == NULL) FATAL("Not enough memory!");
   memcpy(new->buf, old->buf,
          old->bytes);
   *new = *old;
   (TFDSetElement) new->settings =
      (TFDSetElement) old->settings; 
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
   gl = g_list_find_custom(fd_settings, (int*) fd, fdset_list_compare);
   if (gl == NULL) FATAL("Couldn't find appropiate settings for active client."); 
   settings = gl->data;
    new->client_name = malloc(strlen(settings->client_name)+1);
   strcpy(new->client_name, settings->client_name);
   new->fd = fd;
   new->id = fd;
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

/*static gdsl_element_t
hset_alloc_element(void* element)
{
 THistSetElement *new;
 new = malloc(sizeof(THistSetElement));
 *new = *((THistSetElement*) element);
 return (gdsl_element_t) new;
}*/

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

int main() {
   configfile_t *configfile = NULL;
   char *configfilename = "/usr/local/etc/speechd.conf";
   int server_socket;
   struct sockaddr_in server_address;
   fd_set testfds;
   struct timeval tv;
   TFDSetElement *new_fd_set;
   THistSetElement *new_hist_set;
   THistoryClient *hnew_element;
   int v, i;

   MessageQueue = speechd_queue_alloc();

   if (MessageQueue == NULL)
	FATAL("Couldn't alocate memmory for MessageQueue.");

   MessagePausedList = NULL;

   fd_settings = NULL;

   history = NULL;
   
   history_settings = NULL;

   awaiting_data = (GArray*) g_array_sized_new(1, 1, sizeof(int),10);
   g_array_set_size(awaiting_data,10);
   o_bytes = (GArray*) g_array_sized_new(1, 1, sizeof(int),10);
   g_array_set_size(o_bytes,10);
   
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

   if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
      FATAL("bind() failed");

   /* Create a connection queue and initialize readfds to handle input from server_socket. */
   if (listen(server_socket, 5) == -1)
      FATAL("listen() failed");

   FD_ZERO(&readfds);
   FD_SET(server_socket, &readfds);
   fdmax = server_socket;

   /* Now wait for clients and requests.
    * Since we have passed a null pointer as the timeout parameter, no timeout will occur.
    * The program will exit and report an error if select returns a value of less than 1.  */
   
      MSG(1, "speech server waiting for clients ...\n");

    tv.tv_sec = 0;
    tv.tv_usec = 10;
   while (1) {
      int fd;
      testfds = readfds;

      if (select(FD_SETSIZE, &testfds, (fd_set *)0, (fd_set *)0, &tv) >= 1){
      /* Once we know we've got activity,
       * we find which descriptor it's on by checking each in turn using FD_ISSET. */

      for (fd = 0; fd <= fdmax && fd < FD_SETSIZE; fd++) {

	 MSG(5," testing fd %d (fdmax = %d)...\n", fd, fdmax);

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

               new_fd_set = default_fd_set();
               new_fd_set->fd = client_socket;
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
		  close(fd);
		  FD_CLR(fd, &readfds);
		  if (fd == fdmax) fdmax--; /* this may not apply in all cases, but this is sufficient ... */
		  MSG(3,"   removing client on fd %d\n", fd);
	       } else {
		  /* Here we 'serve' the client. */
		  MSG(2,"   serving client on fd %d\n", fd);
		  if (serve(fd) == -1) MSG(1,"   failed to serve client on fd %d!\n",fd);
	       }
	    }
	 }
      }

    }
    speak();
   }

   /* on exit ...
   for (i = 0; i < NUM_MODULES && handles[i]; i++)
      dlclose(handles[i]);
   g_hash_table_destroy(table);
   */
}



