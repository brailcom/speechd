
int rbd_init(char* client_name, char* conn_name);
void rbd_close(int fd);
int rbd_say(int fd, int priority, char* text);
int rbd_sayf (int fd, int priority, char *format, ...);
int rbd_stop(int fd);
int rbd_pause(int fd);
int rbd_resume(int fd);
		

/* internal functions */
char* send_data(int fd, char *message, int wfr);
		
