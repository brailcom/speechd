
/*
 * server.c - Speech Deamon server core
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
 * $Id: server.c,v 1.18 2003-03-25 18:08:02 hanke Exp $
 */

#include "speechd.h"

OutputModule *speaking_module;

int highest_priority = 0;
int last_message_id = -1;

int server_data_on(int fd);
void server_data_off(int fd);

/* Stops speaking and cancels currently spoken message.*/
int
is_sb_speaking()
{
	int speaking = 0;

	/* Determine is the current module is still speaking */
	if(speaking_module != NULL){
		speaking = (*speaking_module->is_speaking) ();
	} 
	if (speaking == 0) speaking_module = NULL;
	return speaking;
}

void
stop_speaking_active_module(){
	if (speaking_module == NULL) return;
	if (is_sb_speaking()){
		(*speaking_module->stop) ();
	}
}

int
stop_p23()
{
	int ret1, ret2;
	ret1 = stop_priority(2);
	ret2 = stop_priority(3);
	if((ret1 == -1) || (ret2 == -1)) return -1;
	else return 0;
}

int
stop_p3(){
	int ret;
	ret = stop_priority(3);
	return ret;
}

int
stop_priority(int priority)
{
	int num, i;
    GList *gl;
	GList *queue;
	
	switch(priority){
		case 1:	queue = MessageQueue->p1; break;
		case 2:	queue = MessageQueue->p2; break;
		case 3:	queue = MessageQueue->p3; break;
		default: return -1;
	}

	if (queue == NULL) return 0;
	
	if (highest_priority == priority){
		stop_speaking_active_module();
	}

	num = g_list_length(queue);
	for(i=0;i<=num-1;i++){
		gl = g_list_first(queue);
		assert(gl != NULL);
		assert(gl->data != NULL);
		mem_free_message(gl->data);
		queue = g_list_delete_link(queue, gl);
		msgs_to_say--;
	}

	switch(priority){
		case 1:	MessageQueue->p1 = queue; break;
		case 2:	MessageQueue->p2 = queue; break;
		case 3:	MessageQueue->p3 = queue; break;
		default: return -1;
	}
	
	return 0;
}

void
stop_from_client(int fd)
{
	GList *gl;
	pthread_mutex_lock(&element_free_mutex);
	while(gl = g_list_find_custom(MessageQueue->p1, (int*) fd, p_msg_lc)){
		if(gl->data != NULL) mem_free_message(gl->data);
		MessageQueue->p1 = g_list_delete_link(MessageQueue->p1, gl);
		msgs_to_say--;
	}
	while(gl = g_list_find_custom(MessageQueue->p2, (int*) fd, p_msg_lc)){
		if(gl->data != NULL) mem_free_message(gl->data);
		MessageQueue->p2 = g_list_delete_link(MessageQueue->p2, gl);
		msgs_to_say--;
	}	
	while(gl = g_list_find_custom(MessageQueue->p3, (int*) fd, p_msg_lc)){
		if(gl->data != NULL) mem_free_message(gl->data);
		MessageQueue->p3 = g_list_delete_link(MessageQueue->p3, gl);
		msgs_to_say--;
	}	
	pthread_mutex_unlock(&element_free_mutex);
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
	if(elem == NULL) return 0;

	/* Find global settings for this connection. */	
	global_settings = (TFDSetElement *) g_hash_table_lookup(fd_settings, &(elem->settings.fd));
	if (global_settings == NULL) return 0;	
 
	if (!global_settings->paused) return 0;
	else return 1;
}

/*
  Speak() is responsible for getting right text from right
  queue in right time and saying it loud through corresponding
  synthetiser. (Note that there can be a big problem with synchronization).
  This runs in a separate thread.
*/
void* 
speak(void* data)
{
	TSpeechDMessage *element = NULL;
	OutputModule *output;
	GList *gl = NULL;
	int ret;

	while(1){
		usleep(10);
	    if (msgs_to_say>0){

			/* Look what is the highest priority of waiting
			 * messages and take the desired actions on other
			 * messages */
		    if(g_list_length(MessageQueue->p1) != 0){
				stop_p3();	
				if (is_sb_speaking() && (highest_priority != 1))
					stop_speaking_active_module();
			}
		    
			if(g_list_length(MessageQueue->p2) != 0){
				stop_p3();
			}

		    if(g_list_length(MessageQueue->p3) > 1){
				/* Stop all other priority 3 messages but leave the first */
				gl = g_list_first(MessageQueue->p3); 
				MessageQueue->p3 = g_list_remove_link(MessageQueue->p3, gl);
				stop_p3();
				/* Fill the queue with the list containing only the first message */
				MessageQueue->p3 = gl;
				continue;
			}

			/* Check if sb is speaking or they are all silent. 
			 * If some synthesizer is speaking, we must wait. */
		    if (is_sb_speaking()){
	            usleep(10);
	            continue;
		    }

			pthread_mutex_lock(&element_free_mutex);
			element = NULL;
			gl = NULL;

			if(SPEECHD_DEBUG){
			    if( (g_list_length(MessageQueue->p1) == 0) &&
					(g_list_length(MessageQueue->p2) == 0) &&
					(g_list_length(MessageQueue->p3) == 0))
				{
					FATAL("All queues empty but msgs_to_say > 0\n");
				}
			}

			/* We will descend through priorities to say more important
			 * messages first. */
			gl = g_list_first(MessageQueue->p1); 
			if (gl != NULL){
				MSG(4,"           Msg in queue p1\n");
				MessageQueue->p1 = g_list_remove_link(MessageQueue->p1, gl);
				highest_priority = 1;
			}else{
				gl = g_list_first(MessageQueue->p2); 
				if (gl != NULL){
					MSG(4,"           Msg in queue p2\n");
					MessageQueue->p2 = g_list_remove_link(MessageQueue->p2, gl);
					highest_priority = 2;
				}else{
					gl = g_list_first(MessageQueue->p3);
					if (gl != NULL){
						MSG(4,"           Msg in queue p3\n");
						MessageQueue->p3 = g_list_remove_link(MessageQueue->p3, gl);
						highest_priority = 3;
					}else{
						if (SPEECHD_DEBUG) FATAL("Descending through queues but all of them are empty");
						pthread_mutex_unlock(&element_free_mutex);
						continue;
					}
				}
	 		} 
	
			/* If we got here, we have some message to say. */
			element = (TSpeechDMessage *) gl->data;
			if (element == NULL){
				if(SPEECHD_DEBUG) FATAL("Non-NULL element containing NULL data found!\n");
				MessageQueue->p1 = g_list_delete_link(MessageQueue->p1, gl);
				MessageQueue->p2 = g_list_delete_link(MessageQueue->p2, gl);
				MessageQueue->p3 = g_list_delete_link(MessageQueue->p3, gl);
				pthread_mutex_unlock(&element_free_mutex);
				continue;
			}
     
			/* Isn't the parent client of this message paused? 
			* If it is, insert the message to the MessagePausedList. */
			assert(element != NULL);
			if (message_nto_speak(element, NULL, NULL)){
				if(element->settings.priority != 3){
					MSG(4, "Inserting message to paused list...\n");
					MessagePausedList = g_list_append(MessagePausedList, element);
				}else{
					mem_free_message(element);
				}
				msgs_to_say--;
				pthread_mutex_unlock(&element_free_mutex);
				continue;
			}

			/* Determine which output module should be used */
			output = g_hash_table_lookup(output_modules, element->settings.output_module);
			if(output == NULL){
				MSG(4,"Didn't find prefered output module, using default\n");
				output = g_hash_table_lookup(output_modules, GlobalFDSet.output_module); 
				if (output == NULL){
					MSG(2, "Can't find default output module.");
					mem_free_message(element);
					msgs_to_say--;
					pthread_mutex_unlock(&element_free_mutex);
					continue;				
				}
			}

			/* Set the speking_module monitor so that we knew who is speaking */
			speaking_module = output;
	  
			/* Write the data to the output module. (say them aloud) */
			ret = (*output->write) (element->buf, element->bytes, &element->settings); 
			if (ret <= 0) MSG(2, "Output module failed");
			mem_free_message(element);
			msgs_to_say--;
			pthread_mutex_unlock(&element_free_mutex);
		}
	}	 
}


/* This implements the various stop commands. 
 *
 * TODO: For some reason, I decided that it should be all together in one
 * function, but I don't see the reason any more so it would
 * be nice to split it in various functions. */
int
stop_c(EStopCommands command, int fd, int target)
{
   TFDSetElement *settings;
   GList *gl;

   if (command == STOP){
      /* first stop speaking on the output module */
      stop_speaking_active_module();
	  if(target) stop_from_client(target);
	  else stop_from_client(fd);
      return 0;
   }
   
   if (command == PAUSE){
      /* first stop speaking on the output module */
      stop_speaking_active_module();
      /* Find settings for this particular client */
	  if(target) settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &target);
	  else settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &fd);
      
      if (settings == NULL) return 1;
	  
      /* Set _paused_ flag. */
      settings->paused = 1;     
      return 0;  
   }

   if (command == RESUME){
      /* Find settings for this particular client */
	  if(target) settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &target);
	  else settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &fd);
      if (settings == NULL) return 1;

	  /* Set it to speak again. */
      settings->paused = 0;

	/* TODO: We have to somehow solve them according to priorities
	 * and not only have p2 hardcoded here. */
	/* Is there any message after resume? */

  {
	TSpeechDMessage *element;

    if(g_list_length(MessagePausedList) != 0){
		while(1){
	         gl = g_list_find_custom(MessagePausedList, (void*) NULL, p_msg_nto_speak);
	         if (gl != NULL){
				element = (TSpeechDMessage*) gl->data;
				MessageQueue->p2 = g_list_append(MessageQueue->p2, element);
				MessagePausedList = g_list_remove_link(MessagePausedList, gl);
				msgs_to_say++;
    	     }else{
            	break;
			}
		}
	}
  }	  
   }
   return 1; 
}

int
queue_message(TSpeechDMessage *new, int fd)
{
	GList *gl;
    TFDSetElement *settings;
    THistoryClient *history_client;
    TSpeechDMessage *newgl;
		
	if (new == NULL) return -1;

	/* Find settings for this particular client */
	settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &fd);
	if (settings == NULL)
		FATAL("Couldn't find settings for active client, internal error.");

	/* Copy the settings to the new to-be-queued element */
	new->settings = *settings;
	new->settings.output_module = (char*) spd_malloc(strlen(settings->output_module) + 1);
	new->settings.language = (char*) spd_malloc( strlen(settings->language) + 1);
	new->settings.client_name = (char*) spd_malloc( strlen(settings->client_name) + 1);
	strcpy(new->settings.output_module, settings->output_module);
	strcpy(new->settings.language, settings->language);
	strcpy(new->settings.client_name, settings->client_name);

	/* And we set the global id (note that this is really global, not
	 * depending on the particular client, but unique) */
	last_message_id++;				
	new->id = last_message_id;

	/* Put the element new to queue according to it's priority. */
	switch(settings->priority){
		case 1: MessageQueue->p1 = g_list_append(MessageQueue->p1, new); 
				break;
		case 2: MessageQueue->p2 = g_list_append(MessageQueue->p2, new);
			   	break;
		case 3: MessageQueue->p3 = g_list_append(MessageQueue->p3, new);
				break;
		default: FATAL("Nonexistent priority given");
	}

	msgs_to_say++;
	
	/* Put the element _new_ to history also, acording to it's fd. */
	gl = g_list_find_custom(history, (int*) fd, p_hc_lc);
	if((gl != NULL) && (gl->data != NULL)){
		history_client = (THistoryClient*) gl->data;

		/* We will make an exact copy for inclusion into history. */
		newgl = (TSpeechDMessage*) history_list_new_message(new); 
		if(newgl != NULL){
			history_client->messages = g_list_append(history_client->messages, newgl);
		}else{
			if(SPEECHD_DEBUG) FATAL("Can't include message into history\n");
		}
	}else{	   
		if(SPEECHD_DEBUG) FATAL("No such history client, internal error\n");
	} 

	return 0;
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
	GList *gl;
	char *command;
	char *param;
	int helper;
	char *ret;
	int r, v, i;
	int end_data;
	char *pos;

	end_data = 0;
	
	assert(fd > 0);
	if ((buf == NULL) || (bytes == 0)){
		if(SPEECHD_DEBUG) FATAL("invalid buffer for parse()\n");
		return "ERR INTERNAL";
	}	
   	
	/* First the condition that we are not in data mode and we
	 * are awaiting commands */
	if (g_array_index(awaiting_data,int,fd) == 0){
		/* Read the command */
		command = get_param(buf, 0, bytes);

		MSG(5, "Command caught: \"%s\" \n", command);

		/* Here we will check which command we got and process
		 * it with it's parameters. */

		if (command == NULL){
			if(SPEECHD_DEBUG) FATAL("invalid buffer for parse()\n");
			return ERR_INTERNAL; 
		}		
		
		if (!strcmp(command,"SET")){				
			return (char *) parse_set(buf, bytes, fd);
		}
		if (!strcmp(command,"HISTORY")){				
			return (char *) parse_history(buf, bytes, fd);
		}
		if (!strcmp(command,"STOP")){
			return (char *) parse_stop(buf, bytes, fd);
		}
		if (!strcmp(command,"PAUSE")){
			return (char *) parse_pause(buf, bytes, fd);
		}
		if (!strcmp(command,"RESUME")){
			return (char *) parse_resume(buf, bytes, fd);
		}
		if (!strcmp(command,"SND_ICON")){				
			return (char *) parse_snd_icon(buf, bytes, fd);
		}

		/* Check if the client didn't end the session */
		if (!strcmp(command,"bye") || !strcmp(command,"quit")){
			MSG(4, "Bye received.\n");
			/* Send a reply to the socket */
			write(fd, OK_BYE, 15);
			speechd_connection_destroy(fd);
		}
	
		if (!strcmp(command,"SPEAK")){
				/* Ckeck if we have enough space in awaiting_data table for
				 * this client, that can have higher file descriptor that
				 * everything we got before */
				r = server_data_on(fd);
				if (r!=0){
			        if(SPEECHD_DEBUG) FATAL("can't switch to data on mode\n");
			        return "ERR INTERNAL";								 
				}
				return OK_RECEIVE_DATA;
		}
		return ERR_INVALID_COMMAND;

		/* The other case is that we are in awaiting_data mode and
		 * we are waiting for text that is comming through the chanel */
		}else{
			enddata:
			/* In the end of the data flow we got a "@data off" command. */
			MSG(5,"buffer: |%s| ", buf);
			if((!strncmp(buf,".\r\n", bytes))||(!strncmp(buf,"\r\n.\r\n", bytes))||(end_data==1)){
				MSG(4,"finishing data\n");
				end_data=0;
				/* Set the flag to command mode */
				MSG(4, "switching back to command mode...\n");
				v = 0;
				g_array_insert_val(awaiting_data, fd, v);

				/* Prepare element (text+settings commands) to be queued. */
				new = (TSpeechDMessage*) spd_malloc(sizeof(TSpeechDMessage));
				new->bytes = g_array_index(o_bytes,int,fd);
				new->buf = (char*) spd_malloc(new->bytes + 1);
				memcpy(new->buf, o_buf[fd]->str, new->bytes);
				assert(new->bytes>=0);
				new->buf[new->bytes] = 0;

				if (new->bytes==0) return OK_MSG_CANCELED;
				
				if(queue_message(new,fd) != 0){
					if(SPEECHD_DEBUG) FATAL("Can't queue message\n");
					free(new->buf);
					free(new);
					return ERR_INTERNAL;
				}
				
				MSG(4, "%d bytes put in queue and history\n", g_array_index(o_bytes,int,fd));
				MSG(4, "messages_to_say set to: %d\n",msgs_to_say);

				/* Clear the counter of bytes in the output buffer. */
				server_data_off(fd);
				return OK_MESSAGE_QUEUED;
			}
	
			if(pos = strstr(buf,"\r\n.\r\n")){	
				bytes=pos-buf;
				end_data=1;		
				MSG(4,"command in data caught\n");
			}

			/* Get the number of bytes read before, sum it with the number of bytes read
			 * now and store again in the counter */
			v = g_array_index(o_bytes,int,fd);
			v += bytes;
			g_array_set_size(o_bytes,fd);	 
			g_array_insert_val(o_bytes, fd, v);
    
		    buf[bytes] = 0;
		    g_string_append(o_buf[fd],buf);
		}

		if (end_data == 1) goto enddata;

	/* Don't reply on data */
	return "";
}

int
server_data_on(int fd)
{
	int v;

	/* Mark this client as ,,sending data'' */
	v = 1;
	g_array_insert_val(awaiting_data, fd, v);
	/* Create new output buffer */
	o_buf[fd] = g_string_new("\0");
	assert(o_buf[fd] != NULL);
	MSG(4, "switching to data mode...\n");
	return 0;
}

void
server_data_off(int fd)
{
	int v;
	v = 0;
	g_array_insert_val(o_bytes, fd, v);
	if(o_buf[fd]!=NULL)	g_string_free(o_buf[fd],1);
	else if(SPEECHD_DEBUG) FATAL("o_buf[fd] == NULL while data on\n");
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
	MSG(4, "reading data\n");
	bytes = read(fd, buf, BUF_SIZE);
	if(bytes == -1) return -1;
	buf[bytes]=0;
	MSG(4,"    read %d bytes from client on fd %d\n", bytes, fd);

	/* Parse the data and read the reply*/
	strcpy(reply, parse(buf, bytes, fd));
	
	/* Send the reply to the socket */
	ret = write(fd, reply, strlen(reply));
	if (ret == -1) return -1;	

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

gint
message_list_compare_fd (gconstpointer element, gconstpointer value, gpointer x)
{
	int ret;
	TSpeechDMessage *message;

	message = ((TSpeechDMessage*) element);
	assert(message!=NULL);
	assert(message->settings.fd!=0);

	ret = message->settings.fd - (int) value;
	return ret;
}
