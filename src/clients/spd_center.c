
/*
 * spd_center.c - Speech Deamon user center
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
 * $Id: spd_center.c,v 1.7 2003-03-24 22:25:43 hanke Exp $
 */

#include<stdio.h>
#include<curses.h>
#include<menu.h>
#include<assert.h>
#include "spd_center.h"
#include "libspeechd.h"
#include "def.h"

#define MAX_POSSIBILITIES_FOR_COMPLETION 8
#define KEY_TAB '\t'

static int
menu_virtualize(int c)
{
	if (c == '\n') return (_MENU_CHOSE);
	if (c == KEY_TAB || c==KEY_EXIT) return (_SWITCH_CMDLINE);
	if (c == KEY_NPAGE) return (REQ_FIRST_ITEM);
	if (c == KEY_PPAGE) return (REQ_LAST_ITEM);
	if (c == KEY_DOWN) return (REQ_NEXT_ITEM);
	if (c == KEY_UP) return (REQ_PREV_ITEM);
	if (c == ' ') return (REQ_TOGGLE_ITEM);
	return (c);
}

char *menu_items_main_s[] =
{
	_("Global"),
	_("Clients"),
	"------",
	_("Help"),
	(char *) 0
};

char *menu_items_global_s[] =
{
	_("Settings"),
	_("History"),
	_("Back"),
	(char *) 0
};

char *menu_items_settings_s[] =
{
	_("Rate"),
	_("Language"),
	_("Voice type"),
	_("Pitch"),
	_("Punctuation mode"),
	_("Capital Letters Recognition"),
	_("Spelling"),
	(char *) 0
};

typedef struct
{
	char *str[8];
	int count;
}TComplete;

TComplete cmd_completions = {
	{
		"quit()",
		"stop(",
		"stop_cur_client(",
		"pause(",
		"pause_cur_client(",
		"resume(",
		"resume_cur_client(",
		"msgs_cur_client("
	},
	8
};

ITEM *menu_items_main[sizeof(menu_items_main_s)];
ITEM *menu_items_global[sizeof(menu_items_global_s)];
ITEM **menu_items_clients;
ITEM **menu_items_messages;

char*
spdc_complete(char *buffer, int n, TComplete complete){
	char *ret;
	char *possibilities[MAX_POSSIBILITIES_FOR_COMPLETION];
	int i;
	int pos = 0;

	if((n<=0)||(buffer==NULL)) return buffer;
	
	for (i=0;i<=complete.count-1;i++){
		if(!strncmp(buffer, complete.str[i], n)){
			possibilities[pos] = (char*) malloc((strlen(complete.str[i]) + 1) * sizeof(char));
			assert(possibilities[pos] != NULL);
			strcpy(possibilities[pos], complete.str[i]);
			pos++;
		}
	}

	/* TODO: We need to separate the cases where pos==1 and pos>1 
	 * and display the possibilities somewhere */
	if (pos >= 1){		
			ret = (char*) malloc((strlen(possibilities[0]) + 1) * sizeof(char));
			assert(ret != NULL);
			strcpy(ret,possibilities[0]);
			spd_sayf(s_main_fd, 2,ret);
			return ret;
	}else{
		return buffer;
	}
}

char*
i_get_string(){
	char *buffer=NULL;
	int c;
	int pos;
	echo();
	attrset(COLOR_PAIR(1));
	mvhline(BAR_DIV_Y,BAR_DIV_X,' ',COLS);
	mvprintw(CMD_L_Y, CMD_L_X,PROMPT);
//	spd_stop(s_main_fd);
	spd_sayf(s_main_fd, 2, "Type in.");
	refresh();
	do{
		c = getch();
		if (c==KEY_TAB){
		   	buffer = spdc_complete(buffer, (buffer!=NULL)?strlen(buffer):0, cmd_completions);
		}else{
			buffer = (char*) spd_command_line(s_main_fd, buffer, (buffer!=NULL)?strlen(buffer):0, c);
		}
		mvhline(BAR_DIV_Y,BAR_DIV_X+strlen(PROMPT)+1,' ',strlen(buffer)+3);
	    mvprintw(CMD_L_Y, CMD_L_X+strlen(PROMPT),buffer);	
	}while(c!='\n');
	noecho();
	attrset(COLOR_PAIR(1));
	mvhline(BAR_DIV_Y,BAR_DIV_X,' ',COLS);
	refresh();	
	return buffer;
}

char*
i_get_cmd()
{
	char *cmd;
	cmd = i_get_string();
	/* Strip the newline character */
	cmd[strlen(cmd)-1] = 0;
	return cmd;
}

void
i_initialize()
{
	initscr();
	BAR_STAT_X = 0;	BAR_STAT_Y = LINES-1;	INDENT_STAT = 2;
	BAR_HELP_X = 0;	BAR_HELP_Y = 0;			INDENT_HELP = 2;
	BAR_DIV_X = 0;	BAR_DIV_Y = LINES*3/4;
	CMD_L_X = 2;	CMD_L_Y = BAR_DIV_Y;
	MENU_WIN_X = 1;	MENU_WIN_Y = 2;
	MENU_WIN_L = 40; MENU_WIN_H =BAR_DIV_Y-MENU_WIN_Y-1;

	PROMPT = (char*) malloc(sizeof(char)*10);
	strcpy(PROMPT,"$> ");
	
	start_color();
	init_pair(1,COLOR_WHITE, COLOR_BLUE);
	init_pair(2,COLOR_WHITE, COLOR_BLACK);
}

void
spdc_initialize()
{
	/* Fill the _select_ structure with default values */
	selected.client = NULL;
	selected.subclient = NULL;
	selected.client_id = -1;
	selected.message_id = -1;
	selected.sorted_by = SORT_DEFAULT;
	selected.pos = POS_DEFAULT;
	selected.pos_d = POSD_DEFAULT;
	selected.mode = MODE_DEFAULT;
	selected.select_message_mode = -1;
}

void 
i_basic_screen()
{
    attrset(COLOR_PAIR(1));
	mvhline(BAR_HELP_Y,BAR_HELP_X,' ',COLS);
	mvhline(BAR_STAT_Y,BAR_STAT_X,' ',COLS);
	mvhline(BAR_DIV_Y,BAR_DIV_X,' ',COLS);
	refresh();	

	_menu_win = newwin(MENU_WIN_H, MENU_WIN_L, MENU_WIN_Y, MENU_WIN_X);
	if (_menu_win == NULL) quit(1);
	wrefresh(_menu_win);		
}

MENU*
i_menu_create_d(char **menu_items_s, char **menu_descr_s, ITEM **menu_items)
{
	char **ap;
	char **dp;
	MENU *menu;
		
	ITEM **menu_ip = menu_items;
		
	if(menu_descr_s==NULL){
		for (ap = menu_items_s; *ap; ap++) *menu_ip++ = new_item(*ap, "");
	}else{
		for (ap = menu_items_s, dp = menu_descr_s; *ap; ap++, dp++){
				if(dp!=NULL)   	*menu_ip++ = new_item(*ap, *dp);
				else *menu_ip++ = new_item(*ap, "");
		}
	}
	*menu_ip = (ITEM *) 0;
				
	menu = new_menu(menu_items);
	if (menu == NULL) quit(1);
	keypad(_menu_win, TRUE);
	set_menu_win(menu, _menu_win);
	if(post_menu(menu) != E_OK) return NULL;
	return (menu);
}

MENU*
i_menu_create(char **menu_items_s, ITEM **menu_items)
{
	MENU *menu;
	menu = i_menu_create_d(menu_items_s, NULL, menu_items);
	return menu;
}

void
i_menu_destroy(MENU* menu)
{
	unpost_menu(menu);
	free_menu(menu);
}

void
i_status_bar_msg(char *msg)
{
	attrset(COLOR_PAIR(1));
    mvhline(BAR_STAT_Y,BAR_STAT_X,' ',COLS);
    mvprintw(BAR_STAT_Y, BAR_STAT_X+INDENT_STAT,msg);
	spd_sayf(s_status_fd,3,msg);
    refresh();					
}

void
i_help_bar_msg(char *msg)
{
	attrset(COLOR_PAIR(1));
    mvhline(BAR_HELP_Y,BAR_HELP_X,' ',COLS);
    mvprintw(BAR_HELP_Y, BAR_HELP_X+INDENT_HELP,msg);
    refresh();					
}

char *
i_menu_select_d(MENU *menu, int *command_line, char *it_name_r, char *it_descr_r)
{
	int c, ret_men;
	ITEM *cur_item;
	char *it_name;
	char *it_descr;
	static int last_posd;
	static int last_cur;
	
	*command_line = 0;
	noecho();
	
	if(last_posd != selected.pos_d){
		cur_item = current_item(menu);
	    it_name = (char*) item_name(cur_item);		
		spd_sayf(s_main_fd, 3, it_name);
		it_descr = (char*) item_description(cur_item);
		if(it_descr!=NULL) spd_sayf(s_main_fd, 3, it_descr);
		last_posd = selected.pos_d;
	}
		
	while(1){
		c = wgetch(_menu_win);
		if (c == 27) quit(0);	
		c = menu_virtualize(c);
		if(c == _MENU_CHOSE) break;
		if(c == _SWITCH_CMDLINE){
				*command_line = 1;
			   	return "";
		}
		ret_men = menu_driver(menu, c);

//		spd_stop(s_main_fd);
		
		cur_item = current_item(menu);
	    it_name = (char*) item_name(cur_item);
		spd_sayf(s_main_fd, 3, it_name);
		    it_descr = (char*) item_description(cur_item);
			if(it_descr!=NULL) spd_sayf(s_main_fd, 3, it_descr);
	}
	cur_item = current_item(menu);
    it_name = (char*) item_name(cur_item);		
	if (it_descr!=NULL) it_descr = (char*) item_description(cur_item);		
	echo();

	strcpy(it_name_r, it_name);
	if((it_descr_r!=NULL)&&(it_descr!=NULL)) strcpy(it_descr_r, it_descr);
	return it_name;	
}

char*
i_menu_select(MENU *menu, int *command_line)
{
	char *it_name;
	it_name = (char*) malloc(256*sizeof(char));
	i_menu_select_d(menu, command_line, it_name, NULL);
	return it_name;
}

void
s_initialize()
{
	s_main_fd = spd_init("speechd_client","main");
	if (s_main_fd == 0){ printf(_("Speech Deamon failed. speechd not running?\n")); quit(1); }
	s_status_fd = spd_init("speechd_client","status");
	if (s_main_fd == 0){ printf(_("Speech Deamon failed. speechd not running?\n")); quit(1); }
}

void
quit(int err_code)
{
	endwin();

	//TODO: everything
		
	exit(err_code);
}

void
process_root(char *item_n)
{
	if (!strcmp(item_n,_("Global"))){
		selected.pos_d = POSD_GLOBAL;
	}
	if (!strcmp(item_n,_("Clients"))){
		selected.pos_d = POSD_SELECT_CLIENT;
	}
	if (!strcmp(item_n,_("Help"))){
		selected.pos = POS_HELP;
	}
}

void
process_global(char *item_n)
{
	if (!strcmp(item_n,_("Back"))){
		selected.pos_d = POSD_ROOT;
	}
}

int
process_select_clients(char *item_n, char* item_d)
{	
	if (!strcmp(item_n,_("Back"))){
		selected.pos_d = POSD_ROOT;
		return 0;
	}
	if(strcmp(item_d,"")){
		char *client_name, *subclient_name;
		selected.client_id = atoi(item_d);
		client_name = (char*) strtok(item_n,":");
		subclient_name = (char*) strtok(NULL,":\n");
		assert(client_name!=NULL);
		assert(subclient_name!=NULL);
		selected.client = client_name;
		selected.subclient = subclient_name;

		/* I don't really know if it should be here */
		spd_history_select_client(s_main_fd, selected.client_id);
		
		return 1;
	}
}

int
process_select_message(char *item_n)
{
	if (!strcmp(item_n,_("Back"))){
		selected.pos_d = POSD_ROOT;
		return 0;
	}

	{
		char *c_id;
		int id;
		c_id = (char*) malloc(8);
		c_id = (char*) strtok(item_n," ");
		id = atoi(c_id);
		spd_stop(s_main_fd);
		spd_history_say_msg(s_main_fd, id);
	}
	return 1;
}

MENU*
menu_select_clients()
{
	char **client_names;
	int *client_ids;
	int *active;
	int num_clients;
	char **names, **descr;
	int i;

    client_names = (char**) malloc(sizeof(char*)*256);
    client_ids = (int*) malloc(sizeof(int)*256);			
    active = (int*) malloc(sizeof(int)*256);			
	
	num_clients = spd_get_client_list(s_main_fd, client_names, client_ids, active);
	
    names = (char**) malloc(sizeof(char*)*(num_clients+2));
    descr = (char**) malloc(sizeof(char*)*(num_clients+2));
	
	for(i=0;i<=num_clients-1;i++){
		names[i] = (char*) malloc((strlen(client_names[i])+4) * sizeof(char));
		sprintf(names[i], "%c %s", (active[i])?'+':'-', client_names[i]);
		descr[i] = (char*) malloc(10);
		sprintf(descr[i],"%d", client_ids[i]);
	}
	
	names[num_clients] = (char*) malloc(sizeof(_("Back")));
	strcpy(names[num_clients],_("Back"));
	names[num_clients+1] = (char*) malloc(sizeof(char));
	names[num_clients+1] = (char*) 0;
	descr[num_clients] = NULL;
	descr[num_clients+1] = NULL;
	
	menu_items_clients = (ITEM**) malloc((num_clients+2)*sizeof(ITEM*));

	menu_m = i_menu_create_d(names, descr, menu_items_clients);
	assert(menu_m!=NULL);
	return menu_m;	
}

/* Creates menu for selecting messages.
 * _flag_ determines what kind of selecting we want:
 * 			0	all messages from all clients	(TODO: figure out how to do this)
 * 			1	only messages from curent subclient	
 * 			2	messages from all subclients of current client (TODO: figure out how to do that)
 * 	_fd_ is the client from who we want the messages (TODO: ugly, ugly, ugly...)
 */
MENU*
menu_select_messages(int flag){
    char **client_names;
    int *msg_ids;
    int num_messages;
    char **names, **descr;
    int i;

	i_help_bar_msg(HELPBAR_B_SELECT_MESSAGES);
	
	client_names = (char**) malloc(sizeof(char*)*256);
    msg_ids = (int*) malloc(sizeof(int)*256);

	switch(flag){
    	case 1: assert(selected.client_id!=-1);
				num_messages = spd_get_message_list_fd(s_main_fd, selected.client_id, msg_ids, client_names);
				break;
		default: assert(0);
	}
    names = (char**) malloc(sizeof(char*)*(num_messages+2));
    descr = (char**) malloc(sizeof(char*)*(num_messages+2));

    for(i=0;i<=num_messages-1;i++){
        names[i] = (char*) malloc((strlen(client_names[i])+4) * sizeof(char));
        sprintf(names[i], "%d %s", msg_ids[i], client_names[i]);
    }
	
    names[num_messages] = (char*) malloc(sizeof(_("Back")));
    strcpy(names[num_messages],_("Back"));
    names[num_messages+1] = (char*) malloc(sizeof(char));
    names[num_messages+1] = (char*) 0;
    descr[num_messages] = NULL;
    descr[num_messages+1] = NULL;

    menu_items_messages = (ITEM**) malloc((num_messages+2)*sizeof(ITEM*));

    menu_m = i_menu_create_d(names, NULL, menu_items_messages);
    assert(menu_m!=NULL);
    return menu_m;	
}

void
run_menu()
{
	char *item_n, *item_d;
	int ret;
	int cl = 0;

	i_help_bar_msg(HELPBAR_B_MENU);
	i_menu_destroy(menu_m);
	switch(selected.pos_d){
		case POSD_ROOT:
			menu_m = i_menu_create(menu_items_main_s, menu_items_main);
			item_n = i_menu_select(menu_m, &cl);
			process_root(item_n);
			break;
		case POSD_GLOBAL:
			menu_m = i_menu_create(menu_items_global_s, menu_items_global);
			item_n = i_menu_select(menu_m, &cl);
			process_global(item_n);
			break;
		case POSD_SELECT_CLIENT:
			menu_m = menu_select_clients();
			item_n = (char*) malloc(1024);
			item_d = (char*) malloc(256);
			i_menu_select_d(menu_m, &cl, item_n, item_d);
			ret = process_select_clients(item_n, item_d);
	        if(ret){ 
				selected.pos_d = POSD_SELECT_MESSAGE;
	        	selected.select_message_mode = SMM_CLIENT;
			}
			break;
		case POSD_SELECT_MESSAGE:
			/*TODO: these messages should be added to the menu as they are spoken
			 * by the client, but how to do this?
			 */
			menu_m = menu_select_messages(selected.select_message_mode);
			do{
				item_n = i_menu_select(menu_m, &cl);
			}while(process_select_message(item_n) == 1);
				
			break;
			
		/* TODO: other menus */

		default: assert(0);
	}

	if (cl == 0) selected.pos = POS_MENU;
	if (cl == 1) selected.pos = POS_CMDL;
}

int
menu_select_client_onfly()
{
	MENU *menu_old;
	char *item_n, *item_d;
	int cl;

	menu_old = menu_m;
	menu_m = menu_select_clients();

	item_n = (char*) malloc(1024);
	item_d = (char*) malloc(256);
	i_menu_select_d(menu_m, &cl, item_n, item_d);
	process_select_clients(item_n, item_d);
	i_menu_destroy(menu_m);
	menu_m = menu_old;
	free(item_n);
	free(item_d);
	return selected.client_id;
}

char**
parse_command(char* cmd, int num)
{
	char *parse;
	char **params;
	int i;
	
	params = (char**) malloc(256*sizeof(char*));
	parse = (char*) strdup(cmd);

	strtok(parse,"(),");
	i=0;
	for(i=0;i<=num-1;i++){
		params[i] = (char*) strtok(NULL,"(),");
		if(params[i] == NULL) return NULL;
	}
	return params;
}

int
com_stop_fd(char *cmd)
{
	int fd;
	int ret;
	char **params;

	params = parse_command(cmd,1);
	assert(params!=NULL);
	fd = strtol(params[0], (char **)NULL, 10);
	ret = spd_stop_fd(s_main_fd, fd);
	return ret;
}

int
com_pause_fd(char *cmd)
{
	int fd;
	int ret;
	char **params;

	params = parse_command(cmd,1);
	assert(params!=NULL);
	fd = strtol(params[0], (char **)NULL, 10);
	ret = spd_pause_fd(s_main_fd, fd);
	return ret;
}

int
com_resume_fd(char *cmd)
{
	int fd;
	int ret;

	char **params;

	params = parse_command(cmd,1);
	assert(params!=NULL);
	fd = strtol(params[0], (char **)NULL, 10);
	ret = spd_resume_fd(s_main_fd, fd);
	return ret;
}

int
com_stop()
{
	int ret;
	ret = spd_stop(s_main_fd);
	return ret;
}

int
com_pause()
{
	int ret;
	ret = spd_pause(s_main_fd);
	return ret;
}

int
com_stop_cur_client()
{
	int client;
	int ret;
	client = selected.client_id;
	if (client == -1){
		client = menu_select_client_onfly();
	}
	ret = spd_stop_fd(s_main_fd, client);
	return ret;
}

int
com_pause_cur_client()
{
	int client;
	int ret;
	client = selected.client_id;
	if (client == -1){
		client = menu_select_client_onfly();
	}
	ret = spd_pause_fd(s_main_fd, client);
	return ret;
}

int
com_resume_cur_client()
{
	int client;
	int ret;
	client = selected.client_id;
	if (client == -1){
		client = menu_select_client_onfly();
	}
	ret = spd_resume_fd(s_main_fd, client);
	return ret;
}

int
com_msgs_cur_client()
{
	int client;
	int ret;
	client = selected.client_id;
	if (client == -1){
		client = menu_select_client_onfly();
	}
	selected.pos = POS_MENU;
	selected.pos_d = POSD_SELECT_MESSAGE;
	selected.select_message_mode = SMM_CLIENT;
	return ret;
}

int
com_resume()
{
	int ret;
	ret = spd_resume(s_main_fd);
	return ret;
}

void
run_cmdl()
{
	char *cmd;
	int ret;

	i_help_bar_msg(HELPBAR_B_CMD);
	cmd = i_get_cmd();

	if(!strcmp(cmd,"")) selected.pos = POS_MENU;
	if(!strcmp(cmd,"quit()")) quit(0);
	if(!fnmatch("stop()",cmd,0)) 				ret = com_stop();
	if(!fnmatch("stop(?*)",cmd,0)) 				ret = com_stop_fd(cmd);
	if(!fnmatch("stop_cur_client()",cmd,0)) 	ret = com_stop_cur_client();
	if(!fnmatch("pause()",cmd,0)) 				ret = com_pause();
	if(!fnmatch("pause(?*)",cmd,0)) 			ret = com_pause_fd(cmd);
	if(!fnmatch("pause_cur_client()",cmd,0)) 	ret = com_pause_cur_client();
	if(!fnmatch("resume()",cmd,0)) 				ret = com_resume();
	if(!fnmatch("resume(?*)",cmd,0)) 			ret = com_resume_fd(cmd);
	if(!fnmatch("resume_cur_client()",cmd,0)) 	ret = com_resume_cur_client();
	if(!fnmatch("msgs_cur_client()",cmd,0)) 	ret = com_msgs_cur_client();

	/* TODO: other commands, handle the return value */
}

int
main()
{
	i_initialize();
	s_initialize();
	spdc_initialize();
	
	i_basic_screen();
	i_status_bar_msg(_("Hello, this is Speech Deamon User Center"));	
	i_help_bar_msg(HELPBAR_B_MENU);

	while(1){
		if(selected.pos == POS_MENU) run_menu();
		if(selected.pos == POS_CMDL) run_cmdl();
	}
		
	quit(1);
}
