
#include "speechd.h"

/* Compares THistoryClient data structure elements
   with given ID */
gint
client_compare_id (gconstpointer element, gconstpointer value, gpointer n)
{
   int ret;
   ret = ((THistoryClient*) element)->uid - (int) value;
   return ret;
}

gint
client_compare_fd (gconstpointer element, gconstpointer value, gpointer n)
{
   int ret;
   ret = ((THistoryClient*) element)->fd - (int) value;
   return ret;
}

/* Compares THistSetElement data structure elements
   with given fd. */
int
histset_compare_fd (gconstpointer element, gconstpointer value, gpointer n)
{
   int ret;
   ret = ((THistSetElement*) element)->fd - (int) value;
   return ret;
}

/* Compares TSpeechDMessage data structure elements
   with given ID */
int
message_compare_id (gconstpointer element, gconstpointer value, gpointer n)
{
   int ret;
   ret = ((TSpeechDMessage*) element)->id - (int) value;
   return ret;
}

gint (*p_cli_comp_id)() = client_compare_id;
gint (*p_cli_comp_fd)() = client_compare_fd;
gint (*p_hs_comp_fd)() = histset_compare_fd;
gint (*p_msg_comp_id)() = message_compare_id;

char*
history_get_client_list()
{
   THistoryClient *client;
   GString *clist;
   GList *cursor;

   clist = g_string_new("OK CLIENTS:\\\n\r");
   cursor = history;
   if (cursor == NULL) return ERR_NO_CLIENT;

   do{
     client = cursor->data;
     g_string_append_printf(clist, "%d ", client->uid);
     g_string_append(clist, client->client_name);
     g_string_append_printf(clist, " %d", client->active);
     cursor = g_list_next(cursor);
     if (cursor != NULL) g_string_append(clist, "\\\n\r");
	 else g_string_append(clist,"\n\r");
   }while(cursor != NULL);

   return clist->str;                
}

char*
history_get_message_list(guint client_id, int from, int num)
{
   THistoryClient *client;
   TSpeechDMessage *message;
   GString *mlist;
   GList *gl;
   int i;

   MSG(3, "from %d num %d, client %d\n", from, num, client_id);
   
   gl = g_list_find_custom(history, (int*) client_id, p_cli_comp_id);
   if (gl == NULL) return ERR_NO_SUCH_CLIENT;
   client = gl->data;

   mlist = g_string_new("OK MESSAGES:\\\n\r");

   for (i=from; i<=from+num-1; i++){
     gl = g_list_nth(client->messages, i);
     if (gl == NULL){
			 MSG(3,"no more data\n");
			 return mlist->str;
	 }
     message = gl->data;
     if (message == NULL) FATAL("Internal error.\n");
	 if(i<from+num-1)
	     g_string_append_printf(mlist, "%d %s\\\n\r", message->id, client->client_name);
	 else
	     g_string_append_printf(mlist, "%d %s\n\r", message->id, client->client_name);
			
   }
   return mlist->str;
}

char*
history_get_last(int fd)
{
   THistoryClient *client;
   TSpeechDMessage *message;
   GString *lastm;
   GList *gl;

   lastm = g_string_new("");

	//TODO: Proper client setting
   gl = g_list_first(history);
   if (gl == NULL) return ERR_NO_SUCH_CLIENT;
   client = gl->data;
  
   gl = g_list_last(client->messages);
   if (gl == NULL) return ERR_NO_MESSAGE;
   message = gl->data;
      
   g_string_append_printf(lastm, "OK LAST MESSAGE:\n\r%d %s\n\r", 
           message->id, client->client_name);
   return lastm->str;
}

char*
history_cursor_set_last(int fd, guint client_id)
{
   THistoryClient *client;
   THistSetElement *set;
   GList *gl;

   gl = g_list_find_custom(history, (int*) client_id, p_cli_comp_id);
   if (gl == NULL) return ERR_NO_SUCH_CLIENT;
   client = gl->data;
	    
   gl = g_list_find_custom(history_settings, (int*) fd, p_hs_comp_fd);
   if (gl == NULL) FATAL("Couldn't find history settings for active client");
   set = gl->data;

   set->cur_pos = g_list_length(client->messages) - 1;
   set->cur_client_id = client_id;

   return OK_CUR_SET_LAST;
}

char*
history_cursor_set_first(int fd, guint client_id)
{
   THistoryClient *client;
   THistSetElement *set;
   GList *gl;

   gl = g_list_find_custom(history, (int*) client_id, p_cli_comp_id);
   if (gl == NULL) return ERR_NO_SUCH_CLIENT;
   client = gl->data;
	       
   gl = g_list_find_custom(history_settings, (int*) fd, p_hs_comp_fd);
   if (gl == NULL) FATAL("Couldn't find history settings for active client");
   set = gl->data;

   set->cur_pos = 0;
   set->cur_client_id = client_id;
   return OK_CUR_SET_FIRST;
}

char*
history_cursor_set_pos(int fd, guint client_id, int pos)
{
   THistoryClient *client;
   THistSetElement *set;
   GList *gl;

   gl = g_list_find_custom(history, (int*) client_id, p_cli_comp_id);
   if (gl == NULL) return ERR_NO_SUCH_CLIENT;
   client = gl->data;
	       
   if (pos < 0) return ERR_POS_LOW;
   if (pos > g_list_length(client->messages)-1) return ERR_POS_HIGH;

   gl = g_list_find_custom(history_settings, (int*) fd, p_hs_comp_fd);
   if (gl == NULL) FATAL("Couldn't find history settings for active client");
   set = gl->data;
   
   set->cur_pos = pos;
   set->cur_client_id = client_id;
   MSG(3,"cursor pos:%d\n", set->cur_pos);
   return OK_CUR_SET_POS;
}

char*
history_cursor_next(int fd)
{
   THistoryClient *client;
   THistSetElement *set;
   GList *gl;

   gl = g_list_find_custom(history_settings, (int*) fd, p_hs_comp_fd);
   if (gl == NULL) FATAL("Couldn't find history settings for active client");
   set = gl->data;
   
   gl = g_list_find_custom(history, (int*) set->cur_client_id, p_cli_comp_id);
   if (gl == NULL) return ERR_NO_SUCH_CLIENT;
   client = gl->data;
	       
   if ((set->cur_pos + 1) > g_list_length(client->messages)) 
         return ERR_POS_HIGH;
   set->cur_pos++; 

   return OK_CUR_MOV_FOR;
}

char*
history_cursor_prev(int fd){
   THistSetElement *set;
   GList *gl;

   gl = g_list_find_custom(history_settings, (int*) fd, p_hs_comp_fd);
   if (gl == NULL) FATAL("Couldn't find history settings for active client");
   set = gl->data;
   
   if ((set->cur_pos - 1) < 0) return ERR_POS_LOW;
   set->cur_pos--; 
   
   return OK_CUR_MOV_BACK;
}

char*
history_cursor_get(int fd){
   THistoryClient *client;
   THistSetElement *set;
   TSpeechDMessage *new;
   GString *reply;
   GList *gl;

   reply = g_string_new("");
   
   gl = g_list_find_custom(history_settings, (int*) fd, p_hs_comp_fd);
   if (gl == NULL) FATAL("Couldn't find history settings for active client");
   set = gl->data;
  
   gl = g_list_find_custom(history, (int*) set->cur_client_id, p_cli_comp_id);
   if (gl == NULL) return ERR_NO_SUCH_CLIENT;
   client = gl->data;
	       
   gl = g_list_nth(client->messages, (int) set->cur_pos);
   if (gl == NULL)  return ERR_NO_MESSAGE;
   new = gl->data;

   g_string_printf(reply, "OK CURSOR:\n\r%d\n\r", new->id);
   return reply->str;
}

char* history_say_id(int fd, int id){
   THistoryClient *client;
   THistSetElement *set;
   TSpeechDMessage *new;
   GList *gl; 

   gl = g_list_find_custom(history_settings, (int*) fd, p_hs_comp_fd);
   if (gl == NULL) FATAL("Couldn't find history settings for active client");
   set = gl->data;
   
   gl = g_list_find_custom(history, (int*) set->cur_client_id, p_cli_comp_id);
   if (gl == NULL) return ERR_NO_SUCH_CLIENT;
   client = gl->data;
   
   gl = g_list_find_custom(client->messages, (int*) id, p_msg_comp_id);
   if (gl == NULL) return ERR_ID_NOT_EXIST;
   new = gl->data;

   /* TODO: Solve this "p2" problem... */
   MSG(3,"putting history message into queue 1\n");
   MessageQueue->p1 = g_list_append(MessageQueue->p1, new);
   msgs_to_say++;

   return OK_MESSAGE_QUEUED;
}
