
int speechd_init(char* client_name, char* conn_name);
void speechd_close(int fd);
int speechd_say(int fd, int priority, char* text);
 
