
#ifndef PARSE_H
 #define PARSE_H

char* parse(char *buf, int bytes, int fd);

char* parse_history(char *buf, int bytes, int fd);
char* parse_set(char *buf, int bytes, int fd);
char* parse_stop(char *buf, int bytes, int fd);
char* parse_cancel(char *buf, int bytes, int fd);
char* parse_pause(char *buf, int bytes, int fd);
char* parse_resume(char *buf, int bytes, int fd);
char* parse_snd_icon(char *buf, int bytes, int fd);
char* parse_char(char *buf, int bytes, int fd);
char* parse_key(char* buf, int bytes, int fd);
char* parse_list(char* buf, int bytes, int fd);
char* parse_help(char* buf, int bytes, int fd);

#endif
