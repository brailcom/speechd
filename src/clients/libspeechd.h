
/*
 * libspeechd.h - Shared library for easy acces to Speech Deamon functions (header)
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
 * $Id: libspeechd.h,v 1.7 2003-03-23 21:30:22 hanke Exp $
 */

#define LIBSPEECHD_DEBUG 0

	/* This is the header file for the libspeechd library.
	 * If you are reading this, you are either a developer
	 * of Speech Deamon, a (potential) developer of some
	 * application and you want it to speak, or just an
	 * interested user who wants to learn. In all these
	 * cases, I'll try to explain you how this works and
	 * how Speech Deamon could help you. Right here, right
	 * now. You can also see the documentation for reference,
	 * but this *inline* documentation is probably more
	 * complete and up-to-date. If you are new to this
	 * speech interface, we recommend you to start reading
	* it from top to down, not skipping.*/

	/* I'll suppose you know what Speech Deamon is and how
	 * does it work (more or less) internally. If not,
	 * you should have a look at the general documentation
	 * in ../../doc and maybe also at .../server/speechd.h */

	/* From the point of view of a developer who wants
	 * to use Speech Deamon as his output interface,
	 * the basic things you'll have to do are the following:
	 * 	- you must include this header file in your program
	 * 	- you must link with -llibspeechd
	 *  - you should let the user know (eg. in documentation)
	 *  you suppose he'll have Speech Deamon running when your
	 *  program starts
	 */

	/* Speech Deamon operates as a server internally and
	 * your app will be a client. So in some point in
	 * your sourcecode you have to open the connection,
	 * passing some necessary identification parameters.
	 */
int spd_init(char* client_name, char* conn_name);
	/* For the purpose of further communication with speech
	 * interface, you will receive an identification code (file
	 * descriptor) of the opened socket as the return value.
	 * But what do _client_name_ and _conn_name_ stand for?
	 * Well, these are two strings that Speech Deamon uses for
	 * the identification of your client. It will be
	 * particularly useful for the user for settings
	 * of the synthesis parameters and also for browsing
	 * through history of said messages (eg. for visually
	 * impaired people).
	 *
	 * The first, _client_name_, is some general name for your
	 * app (eg. "lynx", "emacs", "vi", "bash"), while the
	 * seccond one, _conn_name_ is some more detailed description
	 * of what should this particular client do (eg. "main",
	 * "cmd_line", "text_buffer", "menu", "status_bar"). 
	 *
	 * The reason is that if you have a more complicated
	 * program, like emacs definitely is, it's a good
	 * idea not to mess all the messages together, but
	 * maintain several different connections each
	 * for a particular purpose. You will then make
	 * your user's life much easier, because he will
	 * be able to set the speech parameters for different
	 * part of you program differently. For example,
	 * a blind person would appreciate if he or she
	 * could set up the cmd_line connection of your
	 * program so that it always spells the input
	 * while the text_buffer will always speak
	 * fluently, by default.
	 *
	 * By conclusion, you should open several connections
	 * to Speeach Deamon, each with the same _client_name_
	 * but with different _conn_name_, and save the returned
	 * connection fd's (file descriptors) 
	 *
	 * But take care, if it returns -1, then it failed
	 * to estabilish a connection and you can't use it!
	 */

	 /* Each connection you open should be closed before
	  * the end of your program. You simply pass it's
	  * file descriptor to spd_close() */
void spd_close(int fd);

	/* Now let's suppose you have succesfully opened
	 * one or more connections, saved it's fd's
	 * and you want to make some fun. The basic thing
	 * you can do is to send some message to Speech
	 * Deamon and if everything goes well, you will
	 * hear it in your speakers. */
int spd_say(int fd, int priority, char* text);
	/* _fd_ is the file descriptor of the connection
	 * you want to use, _text_ is a NULL-terminated
	 * string containing your message. 
	 * 
	 * But there is one more parameter called _priority_,
	 * the reason is, sometimes some messages are more
	 * important than other ones. For example, your app
	 * is a mail client and it's reading a text of a mail
	 * when suddenly you receive an important error.
	 * So you will naturally want to say it with priority
	 * one so that it supresses the currently spoken text.
	 *
	 * There are three priorities 1,2,3. 2 is the normal
	 * for text, menu output and other things. 3 is the
	 * lowest and it should represent messages that can
	 * help the user if there is nothing else to say, but
	 * aren't important and can be supressed at any time.
	 * Well, the priority one is the most powerfull and
	 * you should use it scarcely, important errors are
	 * a good example of a well-founded use. It supresses
	 * all other messages, even from other clients, and
	 * thus it's a little bit dangerous.
	 *
	 * If there are more than one message of the same
	 * priority at one time, they would be said in
	 * the order they were received. 
	 *
	 * It returns 0 on succes, -1 if something failed.*/

	/* spd_sayf is only a shortcut, very similar to
	 * spd_say() (see spd_say() for the detailed description
	 * of it's parameters).
	 *
	 * The good thing about it is that you can call it in
	 * exactly the same way you are used to call printf()
	 * for screen output. */
int spd_sayf (int fd, int priority, char *format, ...);
	/* You can for example write:
	 *		spd_sayf(fd_main, 2, "Your rate was %d words per minute.", wpm);
	 */

	/* stop, pause and resume are the functions you will
	 * want to use to regulate the speech flow. */

	/* spd_stop() immediatelly stops the currently spoken
	 * message (from this client) and discards all others
	 * that are waiting. However, it isn't pause() so any
	 * other received messages will be said normally. 
	 * _fd_ is the file descriptor of your connection. */
int spd_stop(int fd);
	/* It returns 0 on succes, -1 if there is no such connection. */

	/* spd_pause() and spd_resume() are the regular pause
	 * and resume known from standard media players.
	 * _fd_ is again the file descriptor of your connection. */
int spd_pause(int fd);
int spd_resume(int fd);
	/* Both return 0 on succes, -1 if there is no such connection. */

	/* Speech Deamon supports a facility called sound icons.
	 * Sound icons are a language independent objects that
	 * you can use with communication with user. For example
	 * there are sound icons: "letter_K", "digit_0", "warning"
	 * or "bell". The exact sound each of them will produce
	 * depends on the current language and can be also set by
	 * user. */

int spd_icon(int fd, int priority, char *name);
	/* spd_icon() gets client's fd, the desired priority
	 * of generated message	and name of the sound icon.
	 * Speech Deamon than converts this sound icon into
	 * regular message and puts it into it's queues. 
	 *
	 * spd_icon() returns 0 on succes, -1 if it fails there
	 * is no such connection or there is no such icon). */

	/* Letters, digits and other signs ('+','-','_','/' ...)
	 * are often used types of sound icons. The following 3 functions
	 * are here to simplify dealing with these symbols. */
int spd_say_letter(int fd, int priority, int c);
int spd_say_digit(int fd, int priority, int c);
int spd_say_sgn(int fd, int priority, int c);
	/* In each of these three functions, c means the symbol (letter,
	 * digit or sign) you want to represent.
	 * Each of these functions returns 0 on succes, -1 otherwise.
	 */ 

int spd_say_key(int fd, int priority, int c);
	/* spd_say_key() is a synthesis of the above 3 functions.
	 * You can pass letters, digits or other printable signs
	 * as _c_ and Speech Deamon will find the appropiate sound
	 * icon.
	 * It returns 0 on succes, -1 otherwise. */

	/* The spd_voice_ family of commands can be used to set the
	 * parameters of the voice. But note that these are only prefered values
	 * and the user is free to set different parameters. */
int spd_voice_rate(int fd, int rate);
int spd_voice_pitch(int fd, int pitch);
	/* spd_voice_rate() and spd_voice_pitch() set the rate and the pitch
	 * of the voice respectively to the value passed in the second parameter.
	 * It must be between -100 and +100. 0 is the default value representing
	 * normal speech. Speech Deamon will then try to find the most appropiate
	 * values for each synthesizers. 
	 * Both functions return 0 on succes, -1 otherwise. */

	/* If punctuation mode is on, Speech Deamon will make the synthesizer
	 * pronounce even marks that are normally silent, like coma, dash, or dot. */
int spd_voice_punctuation(int fd, int flag);
	/* If flag is set to 1, the punctuation mode is on, 0 means off. 
	 * It returns 0 on succes, -1 otherwise. */		
	
	/* Sometimes it's necessary to make distinction between small
	 * and capital letters in the pronounced text. */
int spd_voice_cap_let_recognition(int fd, int flag);
	/* If flag is set to 1, capital letters are pronounced differently.
	 * 0 means normal speech.	
	 * It returns 0 on succes, -1 otherwise. */		


	/* If you are going to use Speech Deamon to make your application
	 * accessible to visually impaired people (which is The Good
	 * Thing (tm) !), you can use this function for all
	 * command line oriented text input: */
char* spd_command_line(int fd, char *buffer, int pos, int c);
	/* Now how it works: you have to use it in a loop, each time
	 * you catch a newly typed key, you call this function and pass
	 * it what's currently on the line as _buffer_, the current
	 * position of cursor on the line as _pos_ and the new key
	 * as _c_. Why so dificult you ask? Well, because it's interface
	 * independent. You can for example use it with ncurses, or even
	 * in X where it would fail if we would have some printf() hardwired
	 * here.
	 * 
	 * Here is an example from Speech Deamon Center (using ncurses library):
	 *     do{
	 *        c = getch();
	 *        if (c==KEY_TAB){
	 *           buffer = spdc_complete(buffer, (buffer!=NULL)?strlen(buffer):0,
	 *              cmd_completions);
	 *        }else{
	 * -->       buffer = (char*) spd_command_line(s_main_fd, buffer, 
	 *              (buffer!=NULL)?strlen(buffer):0, c);
	 *        }
	 *     mvhline(BAR_DIV_Y,BAR_DIV_X+strlen(PROMPT)+1,' ',strlen(buffer)+3);
	 *     mvprintw(CMD_L_Y, CMD_L_X+strlen(PROMPT),buffer);
	 *     }while(c!='\n');
	 */                                                                                 
		
	/* These were all the basic functions Speech Deamon
	 * currently provides and that should be enough
	 * to write common client applications. If it's
	 * your case, you can stop reading here. If there
	 * is some function you miss, please report it
	 * on <mailing list> */

/* END OF THE BASIC FUNCTIONS */

	/* Now there are some more functions that are usefull
	 * for clients modifying the behavior of Speech Deamon
	 * or providing user with the history functions. 
	 * You should also have a look at our client called
	 * Speech Deamon User Center in spd_center.h and spd_center.c*/

	/* Exactly the same as spd_stop(), spd_pause() and
	 * spd_resume(), except that they operate on _foreign_
	 * clients, it's unique id specified as _target_.
	 * You can obtain the unique id for example by
	 * spd_get_client_list() */
int spd_stop_fd(int fd, int target);
int spd_pause_fd(int fd, int target);
int spd_resume_fd(int fd, int target);

	/* When operating with history, some functions
	 * require you to have specified on with
	 * client you operate. You can do that like this: */
int spd_history_select_client(int fd, int id);
	/* _fd_ is again file descriptor of your connection
	 * and _id_ is the unique id.  You can obtain the unique
	 * id for example by spd_get_client_list() */
	/* It returns 0 on succes, -1 if something went wrong. */

	/* So it's time to present the misterious spd_get_client_list()
	 * function, I guess.*/

	/* You have to pass your (main) connection file descriptor as _fd_,
	 * and pointers to allocated arrays _client_names_, _client_ids_ and _active_
	 * where the obtained information will be stored */
int spd_get_client_list(int fd, char **client_names, int *client_ids, int* active);
	/* So let's suppose we have run this function and we didn't get a -1 as
	 * a return value. We got some number instead and that will be the number
	 * of clients in the list (should be the total number of clients Speech Deamon
	 * has had in this run.
	 *
	 * Now client_names[i] (where i is less than the return value) is the
	 * name of the i-th client in the form "client_name:conn_name", client_ids[i]
	 * is the misteriuous unique id we have to pass to every seccond function
	 * working with history, and active[i] contains either 1 or 0 if the client
	 * is still active or gone, respectively. */

	/* Similar function with spd_get_client_list() is spd_get_message_list_fd,
	 * which retrieves the list of messages said by a particular client specified
	 * as target (it's unique id, not file descriptor!). */
int spd_get_message_list_fd(int fd, int target, int *msg_ids, char **client_names);
	/* It returns -1 if it fails, the number of retrieved messages otherwise.
	* But be warned that this function is still to be reworked and the parameters
	* will most likely change in future. */

/* END OF ADVANCED FUNCTIONS */	
		
	/* Internal functions. You should never touch these unless you are
	 * developing Speech Deamon (what is of course The Right Thing (tm) :) */
char* send_data(int fd, char *message, int wfr);
int isanum(char* str);		
char* get_rec_str(char *record, int pos);
int get_rec_int(char *record, int pos);
char* parse_response_data(char *resp, int pos);
