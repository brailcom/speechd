
/* Gettext support */
#define _(String) (String)

/* Main data structure defining current user environment */
typedef struct {
	char *client;				/* Currently selected client */
	char *subclient;
	int client_id;
	int message_id;
	int sorted_by;
	int pos;
	int mode;
	int pos_d;
	int select_message_mode;
}TSPDCEnv;

/* Position between the two windows (int pos) */
#define POS_CMDL 0
#define POS_MENU 1
#define POS_HELP 2

/* Position in different menus (int pos_d) */
#define POSD_ROOT 0
#define POSD_GLOBAL 1
#define POSD_SELECT_CLIENT 2
#define POSD_SELECT_SUBCLIENT 3
#define POSD_SELECT_MODE 4
#define POSD_SELECT_MESSAGE 5

#define MODE_HISTORY 0
#define MODE_SETTINGS 1

/* Select message mode */
#define SMM_CLIENT 1
#define SMM_SUBCLIENT 2
#define SMM_ALL 3

/* Sorted by... */
#define SORT_ALPHABET 0
#define SORT_TIME 1

/* Default values */
int SORT_DEFAULT = 0;
int POS_DEFAULT = 1;
int POSD_DEFAULT = 0;
int MODE_DEFAULT = 0;

/* Declare global variables */

TSPDCEnv selected;

/* Declaration of variables defining layout */
int CMD_L_X, CMD_L_Y;
int BAR_HELP_X, BAR_HELP_Y, INDENT_HELP;
int BAR_DIV_X, BAR_DIV_Y;
int BAR_STAT_X, BAR_STAT_Y, INDENT_STAT;
int MENU_WIN_X, MENU_WIN_Y, MENU_WIN_L, MENU_WIN_H;

/* Menus and windows */
WINDOW *_menu_win;
MENU *menu_m;

/* Prompt */
char *PROMPT;

/* ??? */
const int _MENU_CHOSE = 888;
const int _SWITCH_CMDLINE = 999;

/* Speech Deamon connections */
int s_main_fd;
int s_status_fd;

/* Function prototypes */
void quit(int err_code);

#define HELPBAR_B_CMD _("Speech Deamon:  UP/DOWN: move in the menu  ENTER: select  TAB: enter command")
#define HELPBAR_B_MENU _("Speach Deamon:   '': Menu   'help()': Help   'quit()':Quit")
#define HELPBAR_B_SELECT_MESSAGES _("Speach Deamon: ENTER: hear the message")

