/* Speechd server program
 * CVS revision: $Id: speechd.c,v 1.3 2002-06-30 00:15:59 hanke Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

#include "speechd.h"

/* define dotconf configuration options */
static const configoption_t options[] = 
{
   {"AddModule", ARG_STR, cb_addmodule, 0, 0},
   /*{"ExampleOption", ARG_STR, cb_example, 0, 0},
     {"MultiLineRaw", ARG_STR, cb_multiline, 0, 0},
     {"", ARG_NAME, cb_unknown, 0, 0},
     {"MoreArgs", ARG_LIST, cb_moreargs, 0, 0},*/
   LAST_OPTION
};

static gdsl_element_t
message_alloc(void *element)
{
   TSpeechDMessage* new;
   new = (TSpeechDMessage *) malloc(sizeof(TSpeechDMessage));
   if (new == NULL) FATAL("Not enough memory!");
   *new = *((TSpeechDMessage*) element);
   return (gdsl_element_t) new; 
}

void
message_free(void *element)
{
   free(element);
   return;
}

TSpeechDQueue* 
speechd_queue_alloc()
{
   TSpeechDQueue *new;
	
   new = malloc (sizeof(TSpeechDQueue));
   if (new == NULL) FATAL("Not enough memory!");
   new->p1 = gdsl_queue_create("priority_one", message_alloc, message_free);
   new->p2 = gdsl_queue_create("priority_two", message_alloc, message_free);
   new->p3 = gdsl_queue_create("priority_three", message_alloc, message_free);
   if (new->p1 == NULL) FATAL("Couldn't create priority 1 queue.");
   if (new->p2 == NULL) FATAL("Couldn't create priority 2 queue.");
   if (new->p3 == NULL) FATAL("Couldn't create priority 3 queue.");
   return(new);
}

static gdsl_element_t
fdset_list_alloc_element(void* element)
{
   TFDSetElement *new;
   new = malloc(sizeof(TFDSetElement));
   if (new == NULL) FATAL("Not enough memory!");
   *new = *((TFDSetElement*) element);
   new->language = malloc (strlen( ((TFDSetElement*) element)->language) );
   if (new->language == NULL) FATAL("Not enough memory!");
   new->output_module = malloc (strlen( ((TFDSetElement*) element)->output_module) );
   if (new->output_module == NULL) FATAL("Not enough memory!");
   strcpy(new->language, ((TFDSetElement*) element)->language);
   strcpy(new->output_module, ((TFDSetElement*) element)->output_module);
   return (gdsl_element_t) new;
}

static gdsl_element_t
history_list_alloc_message(void *element)
{
   TSpeechDMessage* new = NULL;
   TSpeechDMessage* old = (TSpeechDMessage*) element;

   new = (TSpeechDMessage *) malloc(sizeof(TSpeechDMessage));
   if (new == NULL) FATAL("Not enough memory!");
   new->buf = malloc(old->bytes);
   if (new->buf == NULL) FATAL("Not enough memory!");
   memcpy(new->buf, old->buf,
          old->bytes);
   *new = *old;
   (TFDSetElement) new->settings =
      (TFDSetElement) old->settings; 
   return (gdsl_element_t) new;
}

static gdsl_element_t
history_list_alloc_client(void* element)
{
   THistoryClient *new;
   new = malloc(sizeof(THistoryClient));
   if (new == NULL) FATAL("Not enough memory!");
	//   strcpy(new->client_name,(char*) element);
   new->fd = *((int*)element);
   new->messages = gdsl_list_create("message_queue", 
                    history_list_alloc_message, NULL);

   return (gdsl_element_t) new;
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
   if (new->language == NULL) FATAL("Not enough memory!");

   new->paused = 0;
   new->priority = 3;
   new->punctuation_mode = 0;
   new->speed = 100;
   new->pitch = 1.0;
   strcpy(new->language,"english");
   strcpy(new->output_module,"flite");
   new->voice_type = MALE;
   new->spelling = 0;         
   new->cap_let_recogn = 0;

   return(new);
}

int main() {
   configfile_t *configfile = NULL;
   char *configfilename = "speechd.conf";
   int server_socket;
   struct sockaddr_in server_address;
   fd_set testfds;
   struct timeval tv;
   TFDSetElement *new_fd_set;

   MessageQueue = speechd_queue_alloc();

   if (MessageQueue == NULL)
	FATAL("Couldn't alocate memmory for MessageQueue.");

   fd_settings = gdsl_list_create("fd_settings", fdset_list_alloc_element, 
		NULL);

   if (fd_settings == NULL)
	FATAL("Couldn't create list of settings for each client.");

   history = gdsl_list_create("history_of_messages", history_list_alloc_client, 		NULL);

   if (history == NULL)
	FATAL("Couldn't create list for history.");

   if (g_module_supported() == FALSE)
      DIE("Loadable modules not supported by current platform\n");

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
               gdsl_list_insert_head(fd_settings, new_fd_set);
               if(gdsl_list_insert_head(history, &client_socket) == NULL) 
                  FATAL("couldn't insert message to history, internal error\n");
		
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

/* declare dotconf callback functions */

DOTCONF_CB(cb_addmodule)
{
   OutputModule *om;
   om = load_output_module(cmd->data.str);
   g_hash_table_insert(output_modules, om->name, om);
   return NULL;
}


