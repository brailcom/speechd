
int spd_init(char* client_name, char* conn_name);
void spd_close(int fd);
int spd_say(int fd, int priority, char* text);
int spd_sayf (int fd, int priority, char *format, ...);
int spd_stop(int fd);
int spd_pause(int fd);
int spd_resume(int fd);
int spd_stop_fd(int fd, int target);
int spd_pause_fd(int fd, int target);
int spd_resume_fd(int fd, int target);

/* internal functions */
char* send_data(int fd, char *message, int wfr);
int isanum(char* str);		
char* get_rec_str(char *record, int pos);
int get_rec_int(char *record, int pos);
char* parse_response_data(char *resp, int pos);
