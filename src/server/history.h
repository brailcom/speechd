
char* 
history_get_client_list();

char* 
history_get_message_list(int from, int num);

char* 
history_get_last(int fd);

char*
history_cursor_set_last(int fd, guint client_id);

char*
history_cursor_set_first(int fd, guint client_id);

char*
history_cursor_set_pos(int fd, guint client_id, int pos);

char*
history_cursor_next(int fd);

char*
history_cursor_prev(int fd);

char*
history_cursor_get(int fd);

char* history_say_id(int fd, int id);
