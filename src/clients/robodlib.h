
int rbd_init(char* client_name, char* conn_name);
void rbd_close(int fd);
int rbd_say(int fd, int priority, char* text);
 
