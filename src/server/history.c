/* Compares THistoryClient data structure elements
   with given ID */

int
client_compare_id (gdsl_element_t element, void* value)
{
   int ret;
   ret = ((THistoryClient*) element)->id - (int) value;
   return ret;
}

/* Compares THistSetElement data structure elements
   with given fd. */
int
histset_compare_fd (gdsl_element_t element, void* value)
{
   int ret;
   ret = ((THistSetElement*) element)->fd - (int) value;
   return ret;
}

/* Compares TSpeechDMessage data structure elements
   with given ID */
int
message_compare_id (gdsl_element_t element, void* value)
{
   int ret;
   ret = ((TSpeechDMessage*) element)->id - (int) value;
   return ret;
}

char*
history_get_client_list()
{
   THistoryClient *client;
   char *clist;
   int size = 1024;
   char id_string[16];

   clist = malloc(size*sizeof(char));

   sprintf(clist, "OK CLIENTS:\n\r");

   gdsl_list_cursor_move_to_head(history);
   while(1){
     if(strlen(clist) > size/2) clist = realloc(clist, size*=2);
     client = gdsl_list_cursor_get(history);
     if (client == NULL) return "ERROR NO CLIENT\n\r";
     strcat(clist, client->client_name);
     snprintf(id_string, 15, "%d", client->id);
     strcat(clist," ");
     strcat(clist, id_string);
     strcat(clist,"\n\r");
     if (gdsl_list_cursor_is_on_tail(history)) return clist;               
     gdsl_list_cursor_move_forward(history);
   }
}

char*
history_get_message_list(int from, int num)
{
   int i;
   THistoryClient *client;
   TSpeechDMessage *message;
   char *mlist;
   int size = 1024;
   char record[256];

   mlist = malloc(size*sizeof(char));
   sprintf(mlist, "OK MESSAGES:\n\r");

   client = gdsl_list_get_head(history);
   if (client == NULL) return "ERROR NO SUCH CLIENT\n\r";

   if (gdsl_list_is_empty(client->messages)) 
       return "ERROR HISTORY EMPTY\n\r\0";

   gdsl_list_cursor_search_by_position(client->messages, from);
   for (i=from; i<=from+num-1; i++){
     if(strlen(mlist) > size/2) mlist = realloc(mlist, size*=2);
     message = gdsl_list_cursor_get(client->messages);
     sprintf(record, "%d %s\n\r", message->id, client->client_name);
     strcat(mlist, record);
     if (message == NULL) FATAL("Internal error.\n");
     if (gdsl_list_cursor_is_on_tail(client->messages)) return mlist;
     gdsl_list_cursor_move_forward(client->messages);
   }
   return mlist;
}

char*
history_get_last(int fd)
{
   THistoryClient *client;
   TSpeechDMessage *message;
   char lastm[256];
   client = gdsl_list_get_head(history);
   if (client == NULL) return "ERROR NO SUCH CLIENT\n\r";
   message = gdsl_list_get_tail(client->messages);
   if (message == NULL) return "ERROR NO MESSAGE FOUND";
   sprintf(lastm, "OK LAST MESSAGE:\n\r %d %s\n\r", 
           message->id, client->client_name);
   return lastm;
}

char*
history_cursor_set_last(int fd, guint client_id)
{
   THistoryClient *client;
   THistSetElement *set;
   int pos;
   client = gdsl_list_search_by_value(history,client_compare_id, (int*) client_id);
   if (client == NULL) return "ERROR NO SUCH CLIENT\n\r";
   pos = gdsl_list_get_card(client->messages);
   set = gdsl_list_search_by_value(history_settings, histset_compare_fd, (int*) fd);
   if (set == NULL) FATAL("Couldn't find history settings for active client");
   set->cur_pos = pos;
   set->cur_client_id = client_id;
   MSG(3,"cursor pos:%d\n", pos);
   return "OK CURSOR SET LAST\n\r";
}

char*
history_cursor_set_first(int fd, guint client_id)
{
   THistoryClient *client;
   THistSetElement *set;
   client = gdsl_list_search_by_value(history,client_compare_id, (int*) client_id);
   if (client == NULL) return "ERROR NO SUCH CLIENT\n\r";
   set = gdsl_list_search_by_value(history_settings, histset_compare_fd, fd);
   if (set == NULL) FATAL("Couldn't find history settings for active client");
   set->cur_pos = 1;
   set->cur_client_id = client_id;
   MSG(3,"cursor pos:%d\n", set->cur_pos);
   return "OK CURSOR SET FIRST\n\r";
}

char*
history_cursor_set_pos(int fd, guint client_id, int pos)
{
   THistoryClient *client;
   THistSetElement *set;
   client = gdsl_list_search_by_value(history, client_compare_id, (int*) client_id);
   if (client == NULL) return "ERROR NO SUCH CLIENT\n\r";
   if (pos > gdsl_list_get_card(client->messages)) return "ERROR POSITION TOO HIGH";
   set = gdsl_list_search_by_value(history_settings, histset_compare_fd, (int*) fd);
   if (set == NULL) FATAL("Couldn't find history settings for active client");
   set->cur_pos = pos;
   set->cur_client_id = client_id;
   MSG(3,"cursor pos:%d\n", set->cur_pos);
   return "OK CURSOR SET TO POSSITION\n\r";
}

char*
history_cursor_next(int fd){
   THistoryClient *client;
   THistSetElement *set;
   set = gdsl_list_search_by_value(history_settings, histset_compare_fd, (int*) fd);
   if (set == NULL) FATAL("Couldn't find history settings for active client");
   client = gdsl_list_search_by_value(history, client_compare_id, (int*) set->cur_client_id);
   if (client == NULL) FATAL ("Couldn't find active client in table.");
   if ((set->cur_pos + 1) > gdsl_list_get_card(client->messages)) 
         return "ERROR POSITION TOO HIGH\n\r";
   set->cur_pos++; 
   MSG(3, "Cursor moved to %d\n\r", set->cur_pos);
   return "OK CURSOR MOVED FORWARD\n\r";
}

char*
history_cursor_prev(int fd){
   THistoryClient *client;
   THistSetElement *set;
   set = gdsl_list_search_by_value(history_settings, histset_compare_fd, (int*) fd);
   if (set == NULL) FATAL("Couldn't find history settings for active client");

   client = gdsl_list_search_by_value(history, client_compare_id, (int*) set->cur_client_id);
   if (client == NULL) FATAL ("Couldn't find active client in table.");
   if ((set->cur_pos - 1) <= 0) return "ERROR POSITION TOO LOW\n\r";
   set->cur_pos--; 
   MSG(3, "Cursor moved to %d\n\r", set->cur_pos);
   return "OK CURSOR MOVED BACKWARDS\n\r";
}

char*
history_cursor_get(int fd){
   THistoryClient *client;
   THistSetElement *set;
   TSpeechDMessage *new;
   char reply[256];
   set = gdsl_list_search_by_value(history_settings, histset_compare_fd, (int*) fd);
   if (set == NULL) FATAL("Couldn't find history settings for active client");

   client = gdsl_list_search_by_value(history, client_compare_id, (int*) set->cur_client_id);
   if (client == NULL) FATAL ("Couldn't find active client in table.");

   new = gdsl_list_search_by_position(client->messages, (int*) set->cur_pos);
   if (new == NULL) FATAL("Position doesn't exist, internal error.");

   sprintf(reply, "OK CURSOR:\n\r%d\n\r", new->id);
   return reply;
}

char* history_say_id(int fd, int id){
   THistoryClient *client;
   THistSetElement *set;
   TSpeechDMessage *new;

   set = gdsl_list_search_by_value(history_settings, histset_compare_fd, (int*) fd);
   if (set == NULL) FATAL("Couldn't find history settings for active client");

   client = gdsl_list_search_by_value(history, client_compare_id, (int*) set->cur_client_id);
   if (client == NULL) FATAL ("Couldn't find active client in table.");

   new = gdsl_list_search_by_value(client->messages, message_compare_id, (int*) id);
   if (new == NULL) return "ERROR ID DOESN'T EXIST";
   gdsl_queue_put((gdsl_queue_t) MessageQueue->p2, new);

   return "OK MESSAGE QUEUED\n\r";
}
