/* Speechd server functions
 * CVS revision: $Id: server.c,v 1.5 2002-07-18 17:46:29 hanke Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

#include "robod.h"
#include "history.h"
#include "history.c"

#define BUF_SIZE 4096
#define MAX_CLIENTS 10

char speaking_module[256] = "\0";

/* Stops speaking. And cancels currently spoken message.*/
void 
stop_speaking_active_module()
{
   int speaking;
   OutputModule *output;

   /* If some module is speaking, fill _output_ with it
      and determine if it's still speaking. If so, stop it.*/
   if(strlen(speaking_module)>1){
      output = g_hash_table_lookup(output_modules, speaking_module);
      if (output == NULL) FATAL("Speaking module not in hash table.");
      speaking = (*output->is_speaking) ();
      if (speaking == 1) (*output->stop) ();
   } 
}

/* Determines if this messages is to be spoken
   (returns 1) or it's parent client is paused (returns 0). */
int
message_nto_speak(TSpeechDMessage *elem, void* value)
{
   TFDSetElement *global_settings;

   if(elem == NULL) return 1; /* We will not speak if there is nothing to say.*/

   global_settings = gdsl_list_search_by_value(fd_settings, fdset_list_compare,  (int*) elem->settings.fd);  /* Find global settings for this connection */

   if (global_settings == NULL) 
      FATAL("Couldn't find settings for active client, internal error.");
 
   if (global_settings->paused == 0){
	 return 0;   /* The client is not paused, we can say the message. */
   }

   MSG(4, "Found paused client.");
   return 1; 	     /* Client is paused. */
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
 
   /* We will browse through queues to see if we can say something. */
     if( gdsl_queue_is_empty(MessageQueue->p1) &&
         gdsl_queue_is_empty(MessageQueue->p2) &&
         gdsl_queue_is_empty(MessageQueue->p3) &&
         gdsl_list_is_empty(MessagePausedList)){
         
         return 0;
      }

      output = g_hash_table_lookup(output_modules, "flite");

      if (output == NULL)
         FATAL("Couldn't find appropiate output module.");

      strcpy(speaking_module, "flite"/*(char*) element->settings.output_module*/);      

      /* First check if the output module is speaking. 
         If it is, we can't send other data. */
 
      if ( (*output->is_speaking) () ){
         return 0;
      }

       /* Is there any message after @resume? */
      if(!gdsl_list_is_empty(MessagePausedList)){
         element = gdsl_list_search_by_value(MessagePausedList,
                      message_nto_speak, (void*) NULL); 
         if (element != NULL){
            gdsl_list_remove_by_value(MessagePausedList,
                         message_nto_speak, (void*) NULL);
            }
      }

      /* If we haven't found anything, we will browse through queues. */
      if(element == NULL){
         /* We will descend through priorities to say more important
            messages first. */
         element = (TSpeechDMessage *) gdsl_queue_get(MessageQueue->p1); 
         if (element == NULL){
            element = (TSpeechDMessage *) gdsl_queue_get(MessageQueue->p2); 
            if (element == NULL){
               element = (TSpeechDMessage *) gdsl_queue_get(MessageQueue->p3);
               if (element == NULL){
                  return 0;
               }
            }
         } 
      }

      /* Isn't the parent client of this message paused? 
         If it is, insert the message to the MessagePausedList. */
      if (message_nto_speak(element, NULL)){
         MSG(3, "Inserting message to paused list...\n");
         gdsl_list_insert_tail(MessagePausedList, element);
         return 0;
      }

      /* Write the data to the output module. (say them)*/
      (*output->write) (element->buf, element->bytes, &element->settings); 

   return 0;
}

/* Gets command parameter _n_ from the text buffer _buf_
   which has _bytes_ bytes; */

char* 
get_param(char *buf, int n, int bytes)
{
   char* param;
   int i, y, z;

   param = (char*) malloc(bytes);

   strcpy(param,"");
   i = 1;	/* 1 because we can skip the leading '@' */

   for(y=0; y<=n; y++){
      z=0;
      for(; i<bytes; i++){
         if (buf[i] == ' ') break;
            param[z] = buf[i];
            z++;
      }
      i++;
   }
   
   if (i >= bytes){
	   param[z>1?z-2:0] = 0;	/* write the trailing zero */
   }else{
	param[z] = 0;
   }

   return param;
}

/* 
   fdset_list_compare() is used to compare fd fields
   of TFDSetElement while searching in the fd_settings
   list by fd.
*/
int
fdset_list_compare (gdsl_element_t element, void* value)
{
   int ret;
   ret = ((TFDSetElement*) element)->fd - (int) value;
   return ret;
}

int
hc_list_compare (gdsl_element_t element, void* value)
{
   int ret;
   ret = ((THistoryClient*) element)->fd - (int) value;
   return ret;
}

typedef enum{
   STOP = 1,
   PAUSE = 2,
   RESUME = 3
}EStopCommands;

int stop_c(EStopCommands command, int fd){
   TFDSetElement *settings;

   if (command == STOP){
      /* first stop speaking on the output module */
      stop_speaking_active_module();
      /* then remove all queued messages for this client */ 
      // TODO: remove all queued messages
      // PROBLEM: gdsl_queue_t can't remove_by_value
      return 0;
   }
   
   if (command == PAUSE){
      /* first stop speaking on the output module */
      stop_speaking_active_module();
      /* Find settings for this particular client */
      settings = gdsl_list_search_by_value(fd_settings, fdset_list_compare, (int*) fd);
      if (settings == NULL)
         FATAL("Couldn't find settings for active client, internal error.");
      /* Set _paused_ flag. */
      settings->paused = 1;     
      return 0;  
   }

   if (command == RESUME){
      /* Find settings for this particular client */
      settings = gdsl_list_search_by_value(fd_settings, fdset_list_compare, (int*) fd);      if (settings == NULL)
         FATAL("Couldn't find settings for active client, internal error.");
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
   char param[100];
   int helper;
   char *language;
     /* These three lines need to be replaced soon. */
   static int awaiting_data[MAX_CLIENTS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
   static char o_buf[MAX_CLIENTS][BUF_SIZE];
   static int o_bytes[MAX_CLIENTS] = {0,0,0,0,0,0,0,0,0,0};
   static int last_message_id = 0;

   if ((buf == NULL) || (bytes == 0))
      FATAL("invalid buffer for parse()");	// this must be internal error
   
   if (!awaiting_data[fd]){
      if(buf[0] != '@') return "ERROR INVALID COMMAND\n\r";

      command = malloc(bytes+1);
      command = get_param(buf, 0, bytes);

      MSG(2, "Command catched: \"%s\" \n", command);

      /* There we will check which command we got and process
         it with it's parameters. */
      if (!strcmp(command,"priority")){
         helper = atoi(get_param(buf,1,bytes));
         MSG(2, "Setting priority to %d \n", helper);
         return "OK PRIORITY SET\n\r";
      }

      if (!strcmp(command,"language")){
         strcpy(language,get_param(buf,1,bytes));
         MSG(2, "Setting language to %s \n", language);
         return "OK LANGUAGE SET\n\r";
      }

      if (!strcmp(command,"data")){
         strcpy(param, get_param(buf,1,bytes));
         if (!strcmp(param,"on")){
            awaiting_data[fd] = 1;
            MSG(2, "switching to data mode...\n");
            return "OK RECEIVING DATA\n\r";
         }
         return "ERROR INVALID COMMAND\n\r";
      }

      if (!strcmp(command,"history")){
         strcpy(param, get_param(buf,1,bytes));
         MSG(3, "  param 1 catched: %s\n", param);
         if (!strcmp(param,"get")){
            strcpy(param, get_param(buf,2,bytes));
            if (!strcmp(param,"last")){
               return history_get_last(fd);
            }
            if (!strcmp(param,"client_list")){
               return history_get_client_list();
            }  
            if (!strcmp(param,"message_list")){
               return history_get_message_list( atoi(get_param(buf,3,bytes)),
			atoi(get_param(buf,4,bytes)) );
            }  
         }
         if (!strcmp(param,"sort")){
         
         }
         if (!strcmp(param,"cursor")){
            strcpy(param, get_param(buf,2,bytes));
            MSG(3, "    param 2 catched: %s\n", param);
            if (!strcmp(param,"set")){
               strcpy(param, get_param(buf,4,bytes));
               MSG(3, "    param 4 catched: %s\n", param);
               if (!strcmp(param,"last")){
                  return history_cursor_set_last(fd,atoi(get_param(buf,3,bytes)));
               }
               if (!strcmp(param,"first")){
                  return history_cursor_set_first(fd, atoi(get_param(buf,3,bytes)));
               }
               if (!strcmp(param,"position")){
                  return history_cursor_set_pos( fd, atoi(get_param(buf,3,bytes)),
                                           atoi(get_param(buf,5,bytes)) );
               }
            }
            if (!strcmp(param,"next")){
               return history_cursor_next(fd);
 
            }
            if (!strcmp(param,"prev")){
               return history_cursor_prev(fd);
            }
            if (!strcmp(param,"get")){
               return history_cursor_get(fd);
            }
         }
         if (!strcmp(param,"say")){
            strcpy(param, get_param(buf,2,bytes));
            if (!strcmp(param,"ID")){
              return history_say_id(fd, atoi(get_param(buf, 3, bytes)));
            }
            if (!strcmp(param,"TEXT")){

            }
         }
 
         return "ERROR INVALID COMMAND\n\r";
      }

      if (!strcmp(command,"stop")){
         MSG(2, "Stop recieved.");
         stop_c(STOP, fd);
         return "OK STOPPED\n\r";
      }

      if (!strcmp(command,"pause")){
         MSG(2, "Pause recieved.\n");
         stop_c(PAUSE, fd);
         return "OK PAUSED\n\r";
       }

       if (!strcmp(command,"resume")){
          MSG(2, "Resume recieved.");
          stop_c(RESUME, fd);
          return "OK RESUMED\n\r";
       }

       if (!strcmp(command,"bye") || !strcmp(command,"quit")){
          MSG(2, "Bye received.\n");
          /* Stop speaking. */
          stop_speaking_active_module();
          /* send a reply to the socket */
          write(fd, "HAPPY HACKING\n\r", 15);

          close(fd);
          FD_CLR(fd, &readfds);
          if (fd == fdmax) fdmax--; /* this may not apply in all cases,
					but this is sufficient ... */
          MSG(2,"   removing client on fd %d\n", fd);       
       }

      return "ERROR INVALID COMMAND\n\r";		// invalid command

   }else{
      if(!strncmp(buf,"@data off", bytes-2)){
         awaiting_data[fd] = 0;
         MSG(2, "switching back to command mode...\n");

         /* Prepare element (text+settings commands) to be queued. */
         new = malloc(sizeof(TSpeechDMessage));
         new->buf = malloc(o_bytes[fd]);
         memcpy(new->buf, o_buf[fd], o_bytes[fd]);
         new->bytes = o_bytes[fd];

         /* Find settings for this particular client */
         settings = gdsl_list_search_by_value(fd_settings, fdset_list_compare, (int*) fd);
         if (settings == NULL)
            FATAL("Couldnt find settings for active client, internal error.");

         new->settings = *settings;
         new->settings.output_module = malloc( strlen(settings->output_module) );
         strcpy(new->settings.output_module, settings->output_module);
         new->settings.language = malloc( strlen(settings->language) );
         strcpy(new->settings.language, settings->language);
         last_message_id++;
         new->id = last_message_id;

         /* Put the element new to queue. */

         if(settings->priority == 1)
            gdsl_queue_put((gdsl_queue_t) MessageQueue->p1, new);
         if(settings->priority == 2)
            gdsl_queue_put((gdsl_queue_t) MessageQueue->p2, new);
         if(settings->priority == 3)
            gdsl_queue_put((gdsl_queue_t) MessageQueue->p3, new);

         /* Put the element new to history acording to it's fd. */
         history_client = (THistoryClient*) gdsl_list_search_by_value(history,
                           hc_list_compare, (int*) fd);
         if(history_client == NULL)
            FATAL("no such history client, internal error\n");
 
         gdsl_list_insert_tail(history_client->messages, new); 

         MSG(3, "%d bytes put in queue and history\n", o_bytes[fd]);
         o_bytes[fd] = 0;
         o_buf[fd][0]=0;
         return "OK MESSAGE QUEUED\n\r";
      }

      o_bytes[fd] += bytes;
      if (o_bytes[fd] >= BUF_SIZE) return("ERROR TOO MANY DATA");
      o_buf[fd][o_bytes[fd]]=0;
      buf[bytes]=0;
      strcat(o_buf[fd],buf);
   }

   return "";
}

int
serve(int fd)
{
   int bytes;
   char buf[BUF_SIZE];
   char reply[256] = "\0";
 
   /* read data from socket */
   bytes = read(fd, buf, BUF_SIZE);
   MSG(3,"    read %d bytes from client on fd %d\n", bytes, fd);

   /* parse the data */
   strcpy(reply, parse(buf, bytes, fd));
   /* send a reply to the socket */
   write(fd, reply, strlen(reply));

   return 0;
}
