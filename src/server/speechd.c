/* Speechd server program
 * CVS revision: $Id: speechd.c,v 1.2 2001-04-10 10:42:05 cerha Exp $
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

/* associative array of all configured (and succesfully loaded) output modules */
static GHashTable *output_modules;

int main() {
   configfile_t *configfile = NULL;
   char *configfilename = "speechd.conf";
   int server_socket, fdmax;
   struct sockaddr_in server_address;
   fd_set readfds, testfds;

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
   
   while (1) {
      int fd;
      testfds = readfds;

      MSG(1,"speech server waiting for clients ...\n");

      if (select(FD_SETSIZE, &testfds, (fd_set *)0, (fd_set *)0, (struct timeval *) 0) < 1)
	 FATAL("select() failed");

      /* Once we know we've got activity,
       * we find which descriptor it's on by checking each in turn using FD_ISSET. */

      for (fd = 0; fd <= fdmax && fd < FD_SETSIZE; fd++) {

	 MSG(3," testing fd %d (fdmax = %d)...\n", fd, fdmax);

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
		  if (serve(fd, output_modules) == -1) MSG(1,"   failed to serve client on fd %d!\n",fd);
	       }
	    }
	 }
      }
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
