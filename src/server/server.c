/* Speechd server functions
 * CVS revision: $Id: server.c,v 1.2 2002-06-30 00:15:59 hanke Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

#include "speechd.h"

#define BUF_SIZE 4096
#define MAX_CLIENTS 10

char speaking_module[256] = "";

/* This is only an experiment how to solve the pause() function. */
int
message_to_speak(TSpeechDMessage *elem)
{
   TFDSetElement *global_settings;

   if(elem == NULL) return 1;

   global_settings = gdsl_list_search_by_value(fd_settings, fdset_list_compare,  elem->settings.fd);

   if (global_settings == NULL)
      FATAL("Couldn't find settings for active client, internal error.");

    
   if (global_settings->paused == 0){
	 return 1;
   }
   printf("found paused client ");
   return 0;
}

/*
  Speak() is responsible for getting right text from right
  queue in right time and saying it loud through corresponding
  synthetiser. (Note that there is big problem with synchronization).
*/

int 
speak()
{
   TSpeechDMessage *element;
   OutputModule *output;
   int helper;
 
   /* We will browse through queues to see if we can say something. */
     if( gdsl_queue_is_empty(MessageQueue->p1) &&
         gdsl_queue_is_empty(MessageQueue->p2) &&
         gdsl_queue_is_empty(MessageQueue->p3) ){
         return 0;
      }

      output = g_hash_table_lookup(output_modules, "flite"
/*(char*) element->settings.output_module*/);

      if (output == NULL)
         FATAL("Couldn't find appropiate output module.");

      strcpy(speaking_module, "flite"/*(char*) element->settings.output_module*/);      

      /* First check if the output module is speaking. 
         If it is, we can't send other data. */
 
      if ( (*output->is_speaking) () ){
         return 0;
      }


      /* We will descend through priorities to say more important
         messages first. */
      element = (TSpeechDMessage *) gdsl_queue_get(MessageQueue->p1); 
      if (!message_to_speak(element)){
         gdsl_queue_put((gdsl_queue_t) element, MessageQueue->p1);
         return 0; 
      }
      if (element == NULL){
         element = (TSpeechDMessage *) gdsl_queue_get(MessageQueue->p2); 
         if (!message_to_speak(element)){
            gdsl_queue_put((gdsl_queue_t) element, MessageQueue->p2);
            return 0;
         }
         if (element == NULL){
            element = (TSpeechDMessage *) gdsl_queue_get(MessageQueue->p3);
            if (!message_to_speak(element)){
               gdsl_queue_put((gdsl_queue_t) element, MessageQueue->p3);
               return 0;
            }
            if (element == NULL){
               return 0;
	    }
         }
      } 

      /* Write the data to the output module. */
      (*output->write) (element->buf, element->bytes, &element->settings); 

   return 0;
}

char* 
get_param(char *buf, int n, int bytes)
{
   char* param;
   int i, y, z;

   param = (char*) malloc(bytes);

   strcpy(param,"");
   i = 1;	// 1 because we can skip the leading '@'

   for(y=0;y<=n;y++){
      z=0;
      for(; i<bytes; i++){
         if (buf[i]==' ') break;
            param[z] = buf[i];
            z++;
      }
      i++;
   }
   
   if (i>=bytes){
	   param[z>1?z-2:0] = 0;	// write the trailing zero
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
   ret = ((TFDSetElement*) element)->fd - (int) value;
   return ret;
}

typedef enum{
   STOP = 1,
   PAUSE = 2,
   RESUME = 3
}EStopCommands;

int stop_c(EStopCommands command, int fd){
   int speaking;
   OutputModule *output;
   TFDSetElement *settings;

   if (command == STOP){
      /* first stop speaking on the output module */
      if(strlen(speaking_module)>1){
         output = g_hash_table_lookup(output_modules, speaking_module);
         speaking = (*output->is_speaking) ();
         if (speaking == 1) (*output->stop) ();
      }
      /* then remove all queued messages for this client */ 
      // ...
   }
   
   if (command == PAUSE){
      /* first stop speaking on the output module */
      if(strlen(speaking_module)>1){
         output = g_hash_table_lookup(output_modules, speaking_module);
         speaking = (*output->is_speaking) ();
         if (speaking == 1) (*output->stop) ();
      }
      /* Find settings for this particular client */
      settings = gdsl_list_search_by_value(fd_settings, fdset_list_compare, fd);
      if (settings == NULL)
         FATAL("Couldn't find settings for active client, internal error.");
      // settings->paused = 1;      //doesn't work properly
   }

   if (command == RESUME){
      /* Find settings for this particular client */
      settings = gdsl_list_search_by_value(fd_settings, fdset_list_compare, fd);      if (settings == NULL)
         FATAL("Couldn't find settings for active client, internal error.");
      /* Set it to speak. */
      settings->paused = 0;
   }

   return "INVALID COMMAND\n\r";
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
   THistoryClient *history_client = NULL;
   TSpeechDMessage *new;
   TFDSetElement *settings;
   char *command;
   char param[100];
   int helper;
   char *language;
     /* These three lines need to be replaced soon. */
   static int awaiting_data[MAX_CLIENTS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
   static char o_buf[MAX_CLIENTS][BUF_SIZE];
   static int o_bytes[MAX_CLIENTS] = {0,0,0,0,0,0,0,0,0,0};

   if ((buf == NULL) || (bytes == 0))
      FATAL("invalid buffer for parse()");	// this must be internal error
   
   if (!awaiting_data[fd]){		// commands beggin with @
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
         return "ERROR INVALID COMMAND";
      }

      if (!strcmp(command,"history")){
         strcpy(param, get_param(buf,1,bytes));
         if (!strcmp(param,"get")){
            strcpy(param, get_param(buf,2,bytes));
            if (!strcmp(param,"last")){
              
            }
            if (!strcmp(param,"client_list")){

            }  
            if (!strcmp(param,"message_list")){

            }  
         }
         if (!strcmp(param,"sort")){
         
         }
         if (!strcmp(param,"cursor")){
            strcpy(param, get_param(buf,2,bytes));
            if (!strcmp(param,"set")){

            }
            if (!strcmp(param,"next")){

            }
            if (!strcmp(param,"prev")){

            }
            if (!strcmp(param,"get")){

            }
         }
         if (!strcmp(param,"say")){
            if (!strcmp(param,"ID")){

            }
            if (!strcmp(param,"TEXT")){

            }
         }
 
         return "ERROR INVALID COMMAND";
      }

      if (!strcmp(command,"stop")){
         MSG(2, "Stop recieved.");
         stop_c(STOP, fd);
         return "OK STOPPED\n\r";
      }

      if (!strcmp(command,"pause")){
         MSG(2, "Pause recieved.");
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
         settings = gdsl_list_search_by_value(fd_settings, fdset_list_compare, fd);
         if (settings == NULL)
            FATAL("Couldnt find settings for active client, internal error.");

         new->settings = *settings;
         new->settings.output_module = malloc( strlen(settings->output_module) );
         strcpy(new->settings.output_module, settings->output_module);
         new->settings.language = malloc( strlen(settings->language) );
         strcpy(new->settings.language, settings->language);

         /* Put the element new to queue. */

         if(settings->priority == 1)
            gdsl_queue_put((gdsl_queue_t) MessageQueue->p1, new);
         if(settings->priority == 2)
            gdsl_queue_put((gdsl_queue_t) MessageQueue->p2, new);
         if(settings->priority == 3)
            gdsl_queue_put((gdsl_queue_t) MessageQueue->p3, new);

         /* Put the element new to history acording to it's fd. */
         history_client = (THistoryClient*) gdsl_list_search_by_value(history,
                           hc_list_compare,  fd);
         if(history_client == NULL)
            FATAL("no such history client, internal error\n");
 
         gdsl_list_insert_head(history_client->messages, new); 

         o_bytes[fd] = 0;

         MSG(3, "%d bytes put in queue and history\n", o_bytes[fd]);
         return "OK MESSAGE QUEUED\n\r";
      }

      o_bytes[fd] += bytes;
      if (o_bytes[fd] >= BUF_SIZE) return("ERROR TOO MANY DATA");
      o_buf[fd][o_bytes[fd]]=0;
      strcat(o_buf[fd],buf);
   }

   return "";
}

int
serve(int fd)
{
   int bytes;
   char buf[BUF_SIZE];
   char *reply;
 
   reply = malloc(4096);
   strcpy(reply,"");
  
   /* read data from socket */
   bytes = read(fd, buf, BUF_SIZE);
   MSG(3,"    read %d bytes from client on fd %d\n", bytes, fd);

   /* parse the data */
   reply = parse(buf, bytes, fd);
   /* send a reply to the socket */
   write(fd, reply, strlen(reply));
   
   return 0;
}
