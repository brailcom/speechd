
#include<stdio.h>
#include<curses.h>
#include<menu.h>
#include "libspeechd.h"
#include "def.h"

int CMD_L_X, CMD_L_Y;
int BAR_HELP_X, BAR_HELP_Y, INDENT_HELP;
int BAR_DIV_X, BAR_DIV_Y;
int BAR_STAT_X, BAR_STAT_Y, INDENT_STAT;
int MENU_WIN_X, MENU_WIN_Y, MENU_WIN_L, MENU_WIN_H;

char *PROMPT;

WINDOW *_menu_win;
MENU *menu_m;

const int _MENU_CHOSE = 888;
const int _SWITCH_CMDLINE = 999;

int s_main_fd;
int s_status_fd;

static int
menu_virtualize(int c)
{
	if (c == '\n') return (_MENU_CHOSE);
	if (c == '\t' || c==KEY_EXIT) return (_SWITCH_CMDLINE);
	if (c == KEY_NPAGE) return (REQ_SCR_UPAGE);
	if (c == KEY_PPAGE) return (REQ_SCR_DPAGE);
	if (c == KEY_DOWN) return (REQ_NEXT_ITEM);
	if (c == KEY_UP) return (REQ_PREV_ITEM);
	if (c==' ') return (REQ_TOGGLE_ITEM);
	return (c);
}

char *menu_items_main_s[] =
{
	"Global",
	"Clients",
	"------",
	"Help",
	"About",
	(char *) 0
};

char *menu_items_global_s[] =
{
	"Settings",
	"History",
	(char *) 0
};

char *menu_items_settings_s[] =
{
	"Speed",
	"Language",
	"Voice type",
	"Pitch",
	"Punctuation mode",
	"Capital Letters Recognition",
	"Spelling",
	(char *) 0
};

ITEM *menu_items_main[sizeof(menu_items_main_s)];

void i_get_string(){
	char *buffer=NULL;
	int c;
	int pos;
	echo();
	attrset(COLOR_PAIR(1));
	mvhline(BAR_DIV_Y,BAR_DIV_X,' ',COLS);
	mvprintw(CMD_L_Y, CMD_L_X,PROMPT);
	spd_sayf(s_main_fd, 2, "Type in.");
	refresh();
	do{
		c = getch();
		buffer = spd_command_line(s_main_fd, buffer, (buffer!=NULL)?strlen(buffer):0, c);
		mvhline(BAR_DIV_Y,BAR_DIV_X+strlen(PROMPT)+1,' ',strlen(buffer)+3);
	    mvprintw(CMD_L_Y, CMD_L_X+strlen(PROMPT),buffer);	
	}while(c!='\n');
	refresh();	
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
i_basic_screen()
{
    attrset(COLOR_PAIR(1));
	mvhline(BAR_HELP_Y,BAR_HELP_X,' ',COLS);
	mvhline(BAR_STAT_Y,BAR_STAT_X,' ',COLS);
	mvhline(BAR_DIV_Y,BAR_DIV_X,' ',COLS);
	refresh();	

	_menu_win = newwin(MENU_WIN_H, MENU_WIN_L, MENU_WIN_Y, MENU_WIN_X);
	if (_menu_win == NULL) exit(1);
	wrefresh(_menu_win);		
}

MENU *
i_menu_create(char **menu_items_s, ITEM **menu_items)
{
        char **ap;
		MENU *menu;
		
	    ITEM **menu_ip = menu_items;
		
		for (ap = menu_items_s; *ap; ap++) *menu_ip++ = new_item(*ap, "");
        *menu_ip = (ITEM *) 0;
				
        menu = new_menu(menu_items);
        keypad(_menu_win, TRUE);
        set_menu_win(menu, _menu_win);
        if (menu == NULL) exit(1);
        if(post_menu(menu) != E_OK) return NULL;
		return (menu);
}

void
i_status_bar_msg(char *msg){

		attrset(COLOR_PAIR(1));
	    mvhline(BAR_STAT_Y,BAR_STAT_X,' ',COLS);
	    mvprintw(BAR_STAT_Y, BAR_STAT_X+INDENT_STAT,msg);
		spd_sayf(s_main_fd,3,msg);
	    refresh();					
}

void
i_help_bar_msg(char *msg){

		attrset(COLOR_PAIR(1));
	    mvhline(BAR_HELP_Y,BAR_HELP_X,' ',COLS);
	    mvprintw(BAR_HELP_Y, BAR_HELP_X+INDENT_HELP,msg);
	    refresh();					
}

char *
i_menu_select(MENU *menu, int *command_line)
{
	int c, ret_men;
	ITEM *cur_item;
	char *it_name;
	
	*command_line = 0;
	it_name = (char*) malloc(255*sizeof(char));
	noecho();
	while(1){
		c = wgetch(_menu_win);
		if (c == 27) quit(0);
		c = menu_virtualize(c);
		if(c == _MENU_CHOSE) break;
		if(c == _SWITCH_CMDLINE){
				*command_line = 1;
			   	break;
		}
		ret_men = menu_driver(menu, c);

		cur_item = current_item(menu);
	    it_name = item_name(cur_item);		
		spd_sayf(s_main_fd,2, it_name);
	}
	cur_item = current_item(menu);
    it_name = item_name(cur_item);		
	echo();
	return it_name;	
}

void
s_initialize()
{
	s_main_fd = spd_init("speechd_client","main");
	if (s_main_fd == 0){ printf("Speech Deamon failed\n"); exit(1); }
//	s_status_fd = spd_init("speechd_client","status");
//	if (s_main_fd == 0){ printf("Speech Deamon failed\n"); exit(1); }
}

void
quit(int err_code)
{

	//TODO: everything
		
	exit(err_code);
}

void
main()
{
	char *test;
	int cl;
	
	i_initialize();
	s_initialize();
	i_basic_screen();
	i_status_bar_msg("Hello, this is Speech Deamon User Center");	
	i_help_bar_msg("Speech Deamon: UP/DOWN: move in the menu   ENTER: select   TAB: enter command");

	menu_m = i_menu_create(menu_items_main_s, menu_items_main);
	test = i_menu_select(menu_m, &cl);

	i_status_bar_msg(test);
	if(cl) i_get_string();
	refresh();
	
	exit(0);

}
