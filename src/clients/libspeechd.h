
int spd_init(char* client_name, char* conn_name);
void spd_close(int fd);
int spd_say(int fd, int priority, char* text);
int spd_sayf (int fd, int priority, char *format, ...);
int spd_stop(int fd);
int spd_pause(int fd);
int spd_resume(int fd);
		

/* internal functions */
char* send_data(int fd, char *message, int wfr);
		
