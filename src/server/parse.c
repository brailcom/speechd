
#include "speechd.h"

/* isanum() tests if the given string is a number,
 *  *  * returns 1 if yes, 0 otherwise. */
int
isanum(char *str){
   int i;
   for(i=0;i<=strlen(str)-1;i++){
       if (!isdigit(str[i]))   return 0;
    }
    return 1;
}


/* Gets command parameter _n_ from the text buffer _buf_
 * which has _bytes_ bytes. Note that the parameter with
 * index 0 is the command itself. */
char* 
get_param(char *buf, int n, int bytes)
{
	char* param;
	int i, y, z;

	param = (char*) malloc(bytes);
	assert(param != NULL);
	
	strcpy(param,"");
	i = 1;				/* it's 1 because we can skip the leading '@' */

	/* Read all the parameters one by one,
	 * but stop after the one with index n,
	 * while maintaining it's value in _param_ */
	for(y=0; y<=n; y++){
		z=0;
		for(; i<bytes; i++){
			if (buf[i] == ' ') break;
			param[z] = buf[i];
			z++;
      }
      i++;
   }
   
	/* Write the trailing zero */
	if (i >= bytes){
		param[z>1?z-2:0] = 0;
	}else{
		param[z] = 0;
	}

   return param;
}

/* CONTINUE: Cleaning the rest of the file from here. */

/* Parses @history commands and calls the appropriate history_ functions. */
char*
parse_history(char *buf, int bytes, int fd)
{
	char *param;
	char *helper1, *helper2, *helper3;				
		
         param = get_param(buf,1,bytes);
         MSG(3, "  param 1 catched: %s\n", param);
         if (!strcmp(param,"get")){
            param = get_param(buf,2,bytes);
            if (!strcmp(param,"last")){
               return (char*) history_get_last(fd);
            }
            if (!strcmp(param,"client_list")){
               return (char*) history_get_client_list();
            }  
            if (!strcmp(param,"message_list")){
				helper1 = get_param(buf,3,bytes);
				if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
				helper2 = get_param(buf,4,bytes);
				if (!isanum(helper2)) return ERR_NOT_A_NUMBER;
				helper3 = get_param(buf,5,bytes);
				if (!isanum(helper2)) return ERR_NOT_A_NUMBER;
               return (char*) history_get_message_list( atoi(helper1), atoi(helper2), atoi (helper3));
            }  
         }
         if (!strcmp(param,"sort")){
         	// TODO: everything :)
         }
         if (!strcmp(param,"cursor")){
            param = get_param(buf,2,bytes);
            MSG(3, "    param 2 catched: %s\n", param);
            if (!strcmp(param,"set")){
               param = get_param(buf,4,bytes);
               MSG(3, "    param 4 catched: %s\n", param);
               if (!strcmp(param,"last")){
				  helper1 = get_param(buf,3,bytes);
				  if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
                  return (char*) history_cursor_set_last(fd,atoi(helper1));
               }
               if (!strcmp(param,"first")){
				  helper1 = get_param(buf,3,bytes);
				  if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
                  return (char*) history_cursor_set_first(fd, atoi(helper1));
               }
               if (!strcmp(param,"pos")){
				  helper1 = get_param(buf,3,bytes);
				  if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
				  helper2 = get_param(buf,5,bytes);
				  if (!isanum(helper2)) return ERR_NOT_A_NUMBER;
                  return (char*) history_cursor_set_pos( fd, atoi(helper1), atoi(helper2) );
               }
            }
            if (!strcmp(param,"next")){
               return (char*) history_cursor_next(fd);
            }
            if (!strcmp(param,"prev")){
               return (char*) history_cursor_prev(fd);
            }
            if (!strcmp(param,"get")){
               return (char*) history_cursor_get(fd);
            }
         }
         if (!strcmp(param,"say")){
            param = get_param(buf,2,bytes);
            if (!strcmp(param,"ID")){
				helper1 = get_param(buf,3,bytes);
				if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
              return (char*) history_say_id(fd, atoi(helper1));
            }
            if (!strcmp(param,"TEXT")){
			return "NIY\n\r";
            }
         }
 
         return ERR_INVALID_COMMAND;
}

char*
parse_set(char *buf, int bytes, int fd)
{
	char *param;
	char *language;
	char *client_name;
	char *priority;
	int helper;
	int ret;

	param = get_param(buf,1,bytes);
	if (!strcmp(param, "priority")){
		priority = get_param(buf,2,bytes);
		if (!isanum(priority)) return ERR_NOT_A_NUMBER;
		helper = atoi(priority);
		MSG(3, "Setting priority to %d \n", helper);
		ret = set_priority(fd, helper);
		if(!ret) return ERR_COULDNT_SET_PRIORITY;	
		return OK_PRIORITY_SET;
	}

      if (!strcmp(param,"language")){
         language = get_param(buf,2,bytes);
         MSG(3, "Setting language to %s \n", language);
		 ret = set_language(fd, language);
		 if (!ret) return ERR_COULDNT_SET_LANGUAGE;
         return OK_LANGUAGE_SET;
      }

	if (!strcmp(param,"client_name")){
		MSG(2, "Setting client name.");

		client_name = get_param(buf,2,bytes);
		ret = set_client_name(fd, client_name);
		if (!ret) return ERR_COULDNT_SET_LANGUAGE;
		return OK_CLIENT_NAME_SET;
	}		 
	   
	return ERR_INVALID_COMMAND;
}

char*
parse_stop(char *buf, int bytes, int fd)
{
	int ret;
	int target = 0;
	char *param;

    param = get_param(buf,1,bytes);
	if (isanum(param)) target = atoi(param);
			
	MSG(2, "Stop recieved.");
	ret = stop_c(STOP, fd, target);
	if(ret) return "ERR CLIENT NOT FOUND";
	else return "OK STOPPED\n\r";
}

char*
parse_pause(char *buf, int bytes, int fd)
{
	int target = 0;
	char *param;
	int ret;
	param = get_param(buf,1,bytes);
	if (isanum(param)) target = atoi(param);
	MSG(2, "Pause recieved.\n");
	ret = stop_c(PAUSE, fd, target);
	if(ret)	return "ERR CLIENT NOT FOUND";
	else return "OK PAUSED\n\r";
}

char*
parse_resume(char *buf, int bytes, int fd)
{
	int target = 0;
	char *param;
	int ret;
	param = get_param(buf,1,bytes);
	if (isanum(param)) target = atoi(param);
	MSG(2, "Resume recieved.");
	ret = stop_c(RESUME, fd, target);
	if(ret)	return ("ERR CLIENT NOT FOUND");
	else return "OK RESUMED\n\r";
}
							  
					 
