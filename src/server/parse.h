
#ifndef PARSE_H
 #define PARSE_H

char* parse(const char* buf, const int bytes, const int fd);

char* parse_history(const char* buf, const int bytes, const int fd);
char* parse_set(const char* buf, const int bytes, const int fd);
char* parse_stop(const char* buf, const int bytes, const int fd);
char* parse_cancel(const char* buf, const int bytes, const int fd);
char* parse_pause(const char* buf, const int bytes, const int fd);
char* parse_resume(const char* buf, const int bytes, const int fd);
char* parse_snd_icon(const char* buf, const int bytes, const int fd);
char* parse_char(const char* buf, const int bytes, const int fd);
char* parse_key(const char* buf, const int bytes, const int fd);
char* parse_list(const char* buf, const int bytes, const int fd);
char* parse_help(const char* buf, const int bytes, const int fd);
char* parse_block(const char* buf, const int bytes, const int fd);

char* deescape_dot(char *otext);

/* Function for parsing the input from clients */
char* get_param(const char *buf, const int n, const int bytes, const int lower_case);

#endif
