/* Speechd server functions
 * CVS revision: $Id: server.c,v 1.6 2002-11-02 22:04:43 hanke Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

#include "robod.h"

#define BUF_SIZE 4096
#define MAX_CLIENTS 10

char speaking_module[256] = "\0";

gint hc_list_compare (gconstpointer, gconstpointer, gpointer);

gint (*p_fdset_lc)() = fdset_list_compare;
gint (*p_msg_nto_speak)() = message_nto_speak;
gint (*p_hc_lc)() = hc_list_compare;

/* Stops speaking and cancels currently spoken message.*/
int
is_sb_speaking()
{
	int speaking;
	OutputModule *output;

	/* If some module is speaking, fill _output_ with it
	 * and determine if it's still speaking. If so, stop it.*/
	if(strlen(speaking_module)>1){
		output = g_hash_table_lookup(output_modules, speaking_module);
		if (output == NULL) FATAL("Speaking module not in hash table.");
		speaking = (*output->is_speaking) ();
	} 
	return speaking;
}

void
stop_speaking_active_module(){
	OutputModule *output;
	if (is_sb_speaking()){
		output = g_hash_table_lookup(output_modules, speaking_module);
		if (output == NULL) FATAL("Speaking module not in hash table.");
		(*output->stop) ();
	}	
}
      
/* Determines if this messages is to be spoken
 * (returns 1) or it's parent client is paused (returns 0).
 * Note: If you are wondering why it's reversed (not to speak instead
 * of to speak), it's because we also use this function for
 * searching through the list. */
gint
message_nto_speak(TSpeechDMessage *elem, gpointer a, gpointer b)
{
	TFDSetElement *global_settings;
	GList *gl;

	/* Is there something in the body of the message? */
	if(elem == NULL) return 1;

	/* Find global settings for this connection. */
	gl = g_list_find_custom(fd_settings, (int*) elem->settings.fd, p_fdset_lc);
	if (gl == NULL) FATAL("Couldn't find settings for active client, internal error.");
	global_settings = gl->data;
 
	/* If the client is not paused, we can say the message. */
	if (!global_settings->paused){
		return 0;
	}

	/* The client must be paused. */
	MSG(4, "Found paused client.");
	return 1;
}

/*
  Speak() is responsible for getting right text from right
  queue in right time and saying it loud through corresponding
  synthetiser. (Note that there is big problem with synchronization).
*/
int 
speak()
{
	TSpeechDMessage *element = NULL;
	OutputModule *output;
	GList *gl = NULL;
 
	/* If all queues are empty, there is no reason for saying something */
	if( (g_list_length(MessageQueue->p1) == 0) &&
		(g_list_length(MessageQueue->p2) == 0) &&
		(g_list_length(MessageQueue->p3) == 0) &&
		(g_list_length(MessagePausedList) == 0))
	{
		return 0;
	}

	/* Check if sb is speaking or they are all silent. 
	 * If some synthetizer is speaking, we must wait. */
	if (is_sb_speaking()) return 0;
      
	/* TODO: We have to somehow solve them according to priorities
	 * and not only have p1 hardcoded here. */
	/* Is there any message after @resume? */
      if(g_list_length(MessagePausedList) != 0){
         gl = g_list_find_custom(MessagePausedList, (void*) NULL, p_msg_nto_speak);
         if (gl != NULL){
			element = (TSpeechDMessage*) gl->data;
			MessagePausedList = g_list_remove_link(MessagePausedList, gl);
         }else{
            element = NULL;
		}
      }

	/* If we haven't found anything, we will browse through queues. */

	/* TODO: Polish it! */      
	if(element == NULL){
		/* We will descend through priorities to say more important
		 * messages first. */
		gl = g_list_last(MessageQueue->p1); 
		if (gl != NULL) MessageQueue->p1 = g_list_remove_link(MessageQueue->p1, gl);
		else{
				gl = g_list_last(MessageQueue->p2); 
				if (gl != NULL) MessageQueue->p2 = g_list_remove_link(MessageQueue->p2, gl);
				else{
					gl = g_list_last(MessageQueue->p3);
					if (gl != NULL) MessageQueue->p3 = g_list_remove_link(MessageQueue->p3, gl);
					if (gl == NULL) return 0;
			}
 		} 
	}

	/* If we got here, we have some message to say. */
	element = (TSpeechDMessage *) gl->data;
      
	/* Isn't the parent client of this message paused? 
	 * If it is, insert the message to the MessagePausedList. */
	if (message_nto_speak(element, NULL, NULL)){
		MSG(3, "Inserting message to paused list...\n");
		MessagePausedList = g_list_append(MessagePausedList, element);
		return 0;
	}

  	/* Determine which output module should be used */
	output = g_hash_table_lookup(output_modules, element->settings.output_module);
	  
	if (output == NULL) FATAL("Couldn't find appropiate output module.");
	/* Set the speking_module monitor so that we know who is speaking */
	strcpy(speaking_module, element->settings.output_module);      
	  
	/* Write the data to the output module. (say them aloud) */
	(*output->write) (element->buf, element->bytes, &element->settings); 

	/* TODO: free(element); */
   return 0;
}

/* hc_list_compare() compares THistoryClients according
 * to the given file descriptor */
gint
hc_list_compare (gconstpointer element, gconstpointer value, gpointer n)
{
   int ret;
   ret = ((THistoryClient*) element)->fd - (int) value;
   return ret;
}

/* fdset_list_compare() is used to compare fd fields
 * of TFDSetElement while searching in the fd_settings
 * list by fd. */
gint
fdset_list_compare (gconstpointer element, gconstpointer value, gpointer x)
{
   int ret;
   ret = ((TFDSetElement*) element)->fd - (int) value;
   return ret;
}

/* This implements the various stop commands. 
 *
 * TODO: For some reason, I decided that it should be all together in one
 * function, but I don't see the reason any more so it would
 * be nice to split it in various functions. */
int stop_c(EStopCommands command, int fd){
   TFDSetElement *settings;
   GList *gl;

   if (command == STOP){
      /* first stop speaking on the output module */
      stop_speaking_active_module();
      /* then remove all queued messages for this client */ 
      /* OLD_TODO: remove all queued messages
       * PROBLEM: queue can't remove_by_value
	   * Now it's solved by removing them in speak() later, uf uf...
	   */
      return 0;
   }
   
   if (command == PAUSE){
      /* first stop speaking on the output module */
      stop_speaking_active_module();
      /* Find settings for this particular client */
      gl = g_list_find_custom(fd_settings, (int*) fd, p_fdset_lc);
      
      if (gl == NULL)
         FATAL("Couldn't find settings for active client, internal error.");
      settings = gl->data;
      /* Set _paused_ flag. */
      settings->paused = 1;     
      return 0;  
   }

   if (command == RESUME){
      /* Find settings for this particular client */
      gl = g_list_find_custom(fd_settings, (int*) fd, p_fdset_lc);
	  if (gl == NULL) FATAL("Couldn't find settings for active client, internal error.");
      settings = gl->data;
      /* Set it to speak again. */
      settings->paused = 0;
      return 0;
   }

   return 1; 
}

/*
  Parse() receives input data and parses them. It can
  be either command or data to speak. If it's command, it
  is immadetaily executed (eg. parameters are set). If it's
  data to speak, they are queued in corresponding queues
  with corresponding parameters for synthesis.
*/

char* 
parse(char *buf, int bytes, int fd)
{
	TSpeechDMessage *new;
	TFDSetElement *settings;
	THistoryClient *history_client;
	char *command;
	char *param;
	int helper;
	char *language; 
	char *ret;
		/* TODO: What about buffer sizes? Should these be fixed? */
	static char o_buf[MAX_CLIENTS][BUF_SIZE];
	static int last_message_id = 0;
	int r;
	GList *gl;
	TSpeechDMessage *newgl;
	int v;
	char *helper1;

	if ((buf == NULL) || (bytes == 0)) FATAL("invalid buffer for parse()");
   
	/* First the condition that we are not in data mode and we
	 * are awaiting commands */
	if ((g_array_index(awaiting_data,int,fd)) == 0){
		/* Every command has to be introduced by '@' */
		if(buf[0] != '@') return "ERROR INVALID COMMAND\n\r";
		/* Read the command */
		command = get_param(buf, 0, bytes);
		MSG(2, "Command catched: \"%s\" \n", command);

	/* Here we will check which command we got and process
	 * it with it's parameters. */

	if (!strcmp(command,"set")){				
		return (char *) parse_set(buf, bytes, fd);
	}
	if (!strcmp(command,"history")){				
		return (char *) parse_history(buf, bytes, fd);
	}
	if (!strcmp(command,"stop")){
		return (char *) parse_stop(buf, bytes, fd);
	}
	if (!strcmp(command,"pause")){
		return (char *) parse_pause(buf, bytes, fd);
	}
	if (!strcmp(command,"resume")){
		return (char *) parse_resume(buf, bytes, fd);
	}

	/* Check if the client didn't end the session */
	if (!strcmp(command,"bye") || !strcmp(command,"quit")){
		MSG(2, "Bye received.\n");
		/* Stop speaking. */
		stop_speaking_active_module();
		/* Send a reply to the socket */
		write(fd, BYE_MSG, 15);
		close(fd);
		FD_CLR(fd, &readfds);
		if (fd == fdmax) fdmax--; /* this may not apply in all cases, but this is sufficient ... */
		MSG(2,"   removing client on fd %d\n", fd);      
		/* TODO: We should free() all the allocated data structures associated to this client. */
	}
	
	/* Check if the command isn't "@data on" */
	if (!strcmp(command,"data")){
		param = get_param(buf,1,bytes);
		if (!strcmp(param,"on")){
			/* Ckeck if we have enough space in awaiting_data table for
			 * this client, that can have higher file descriptor that
			 * everything we got before */
			g_array_set_size(awaiting_data, fd+1);
			v = 1;
			/* Mark this client as ,,sending data'' */
			g_array_insert_val(awaiting_data, fd, v);
			MSG(2, "switching to data mode...\n");
			return OK_RECEIVE_DATA;
		}
		return ERR_INVALID_COMMAND;
	}
	return ERR_INVALID_COMMAND;

	/* The other case is that we are in awaiting_data mode and
	 * we are waiting for text that is comming through the chanel */
	}else{
		/* In the end of the data flow we got a "@data off" command. */
		if(!strncmp(buf,"@data off", bytes-2)){
			/* Set the flag to command mode */
			v = 0;
			g_array_set_size(awaiting_data, fd+1);
			g_array_insert_val(awaiting_data, fd, v);
			MSG(2, "switching back to command mode...\n");

			/* Prepare element (text+settings commands) to be queued. */
			new = malloc(sizeof(TSpeechDMessage));
			new->bytes = g_array_index(o_bytes,int,fd);
			new->buf = malloc(new->bytes);
			memcpy(new->buf, o_buf[fd], new->bytes);

			/* Find settings for this particular client */
			gl = g_list_find_custom(fd_settings, (int*) fd, p_fdset_lc);
			if (gl == NULL)
            FATAL("Couldnt find settings for active client, internal error.");
			settings = gl->data;

			/* Copy the settings to the new to-be-queued element */
			new->settings = *settings;
			new->settings.output_module = malloc(strlen(settings->output_module));
			strcpy(new->settings.output_module, settings->output_module);
			new->settings.language = malloc( strlen(settings->language) );
			strcpy(new->settings.language, settings->language);

			/* And we set the global id (note that this is really global, not
			 * depending on the particular client, but unique) */
			last_message_id++;				
			new->id = last_message_id;

		/* Put the element new to queue according to it's priority. */
			switch(settings->priority){
				case 1:	MessageQueue->p1 = g_list_append(MessageQueue->p1, new); break;
				case 2: MessageQueue->p2 = g_list_append(MessageQueue->p2, new); break;
				case 3: MessageQueue->p3 = g_list_append(MessageQueue->p3, new); break;
				default: FATAL("Non existing priority requiered");
			}

         /* Put the element _new_ to history also, acording to it's fd. */
		gl = g_list_find_custom(history, (int*) fd, p_hc_lc);
		if(gl == NULL) FATAL("no such history client, internal error\n");
		history_client = (THistoryClient*) gl->data;
 
		/* We will make an exact copy for inclusion into history. */
		newgl = (TSpeechDMessage*) history_list_alloc_message(new); 
		history_client->messages = g_list_append(history_client->messages, newgl);

		MSG(3, "%d bytes put in queue and history\n", g_array_index(o_bytes,int,fd));

		/* Clear the counter of bytes in the output buffer. */
		v = 0;
		g_array_insert_val(o_bytes, fd, v);
		/* Clear the output buffer (well, kind of...)*/
		o_buf[fd][0]=0;
		return OK_MESSAGE_QUEUED;
	}
	
	/* Get the number of bytes read before, sum it with the number of bytes read
	 * now and store again in the counter */
	v = g_array_index(o_bytes,int,fd);
	v += bytes;
	g_array_set_size(o_bytes,fd);	 
	g_array_insert_val(o_bytes, fd, v);
    
  	/* Check if we didn't exceed the maximum size of the buffer. */
	if (g_array_index(o_bytes,int,fd) >= BUF_SIZE) return(ERR_TOO_MUCH_DATA);

	/* Add the new data to the buffer. */
	o_buf[fd][g_array_index(o_bytes,int,fd)] = 0;
    buf[bytes] = 0;
    strcat(o_buf[fd],buf);
	}

	// TODO: eh??
	return "";
}

/* Serve the client on _fd_ if we got some activity. */
int
serve(int fd)
{
	int bytes;				/* Number of bytes we got */
	char buf[BUF_SIZE];		/* The content (commands or data) we got */
	char reply[256] = "\0";	/* Our reply to the client */
	int ret;					/* Return value of write() */
 
	/* Read data from socket */
	bytes = read(fd, buf, BUF_SIZE);
	if(bytes = -1) return -1;
	MSG(3,"    read %d bytes from client on fd %d\n", bytes, fd);

	/* Parse the data and read the reply*/
	strcpy(reply, parse(buf, bytes, fd));

	/* Send the reply to the socket */
	ret = write(fd, reply, strlen(reply));
	if (ret == -1) return -1;	

	return 0;
}
