
/*
 * history.c - History functions for Speech Deamon
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
 * $Id: history.c,v 1.10 2003-03-25 22:48:53 hanke Exp $
 */

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

   clist = g_string_new("");
   cursor = history;
   if (cursor == NULL) return ERR_NO_CLIENT;

   do{
     client = cursor->data;
	 assert(client!=NULL);
     g_string_append_printf(clist, C_OK_CLIENTS"-", client->uid);
     g_string_append_printf(clist, "%d ", client->uid);
     g_string_append(clist, client->client_name);
     g_string_append_printf(clist, " %d", client->active);
     cursor = g_list_next(cursor);
     if (cursor != NULL) g_string_append(clist, "\r\n");
	 else g_string_append(clist,"\r\n");
   }while(cursor != NULL);
	g_string_append_printf(clist, OK_CLIENT_LIST_SENT);

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

   MSG(4, "message_list: from %d num %d, client %d\n", from, num, client_id);
   
   gl = g_list_find_custom(history, (int*) client_id, p_cli_comp_id);
   if (gl == NULL) return ERR_NO_SUCH_CLIENT;
   client = gl->data;

   mlist = g_string_new("");

   for (i=from; i<=from+num-1; i++){
     gl = g_list_nth(client->messages, i);
     if (gl == NULL){
			 MSG(4,"no more data\n");
			 return mlist->str;
	 }
     message = gl->data;

 	 if (message == NULL){
		if(SPEECHD_DEBUG) FATAL("Internal error.\n");
		return ERR_INTERNAL;
	}

		g_string_append_printf(mlist, C_OK_MSGS"-");
		g_string_append_printf(mlist, "%d %s\r\n", message->id, client->client_name);
   }
 	
	g_string_append_printf(mlist, OK_MSGS_LIST_SENT);
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
      
	g_string_append_printf(lastm, C_OK_LAST_MSG"-%d %s\r\n", 
		message->id, client->client_name);
	g_string_append_printf(lastm, OK_LAST_MSG);
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
   MSG(4,"cursor pos:%d\n", set->cur_pos);
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

   g_string_printf(reply, C_OK_CUR_POS"-%d\r\n"OK_CUR_POS_RET, new->id);
   return reply->str;
}

char* history_say_id(int fd, int id){
   THistoryClient *client;
   THistSetElement *set;
   TSpeechDMessage *new;
   GList *gl; 

   /* TODO: proper selecting of client! */
   
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
   MSG(4,"putting history message into queue 1\n");
   MessageQueue->p1 = g_list_append(MessageQueue->p1, new);
   msgs_to_say++;

   return OK_MESSAGE_QUEUED;
}

THistSetElement*
default_history_settings(){
	THistSetElement* new;
	new = malloc(sizeof(THistSetElement));
	new->cur_pos = 1;
	new->sorted = BY_TIME;  
	return new;
}
