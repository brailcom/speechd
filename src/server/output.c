/*
 * output.c - Output layer for Speech Dispatcher
 *
 * Copyright (C) 2001, 2002, 2003 Brailcom, o.p.s.
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
 * $Id: output.c,v 1.21 2005-10-16 08:58:28 hanke Exp $
 */

#include "output.h"

#include "fdsetconv.c"
#include "parse.h"

/* TODO: Correct macro */
#define TEMP_FAILURE_RETRY(expr) (expr)

void
output_set_speaking_monitor(TSpeechDMessage *msg, OutputModule *output)
{
    /* Set the speaking-monitor so that we know who is speaking */
    speaking_module = output;
    speaking_uid = msg->settings.uid;
    speaking_gid = msg->settings.reparted;
}

static OutputModule*
get_output_module(const TSpeechDMessage *message)
{
    OutputModule *output = NULL;

    if (message->settings.output_module != NULL){
        MSG(5, "Desired output module is %s", message->settings.output_module);
        output = g_hash_table_lookup(output_modules, message->settings.output_module);
        if(output == NULL || !output->working){
            if (GlobalFDSet.output_module != NULL){
                MSG(3,"Warning: Didn't find prefered output module, using default");                
                output = g_hash_table_lookup(output_modules, GlobalFDSet.output_module); 
                if (output == NULL || !output->working) 
                    MSG(2, "Error: Can't find default output module or it's not working!");
            }
        }
    }
    if (output == NULL){
        MSG(3, "Error: Unspecified output module!\n");
        return NULL;
    }

    return output;
}

static void
output_lock(void)
{
    pthread_mutex_lock(&output_layer_mutex);
}

static void
output_unlock(void)
{
    pthread_mutex_unlock(&output_layer_mutex);
}

#define OL_RET(value) \
  {  output_unlock(); \
    return (value); }

char*
output_read_reply(OutputModule *output)
{
    GString *rstr;
    int bytes;
    char *line = NULL;
    int N = 0;
    char *reply;
    
    rstr = g_string_new("");
    
    /* Wait for activity on the socket, when there is some,
       read all the message line by line */
    do{
	bytes = getline(&line, &N, output->stream_out);	
	if (bytes == -1){
	    MSG(2, "Error: Broken pipe to module.");
	    output->working = 0;
	    speaking_module = NULL;
	    output_check_module(output);
	    return NULL; /* Broken pipe */   
	}
	g_string_append(rstr, line);
	/* terminate if we reached the last line (without '-' after numcode) */
    }while( !((strlen(line) < 4) || (line[3] == ' ')));

    /* The resulting message received from the socket is stored in reply */
    reply = rstr->str;
    g_string_free(rstr, FALSE);

    return reply;
}

char*
output_read_reply2(OutputModule *output)
{
    GString *rstr;
    int bytes;
    char *line = NULL;
    int N = 0;
    char *reply;
    

    reply = malloc( 1024 * sizeof(char));

    bytes = read(output->pipe_out[0], reply, 1024);
    reply[bytes] = 0;
    MSG2(1, "output_module", "2Read: %d bytes: <%s>", bytes, reply);

    return reply;
}

int
output_send_data(char* cmd, OutputModule *output, int wfr)
{
    int ret;
    char *response;

    if (output == NULL) return -1;
    if (cmd == NULL) return -1;
    
    ret = TEMP_FAILURE_RETRY(write(output->pipe_in[1], cmd, strlen(cmd)));
    fflush(NULL);
    if (ret == -1){
        MSG(2, "Error: Broken pipe to module.");        
        output->working = 0;
        speaking_module = NULL;
        output_check_module(output);
        return -1;   /* Broken pipe */
    }
    MSG2(5, "output_module", "Command sent to output module: |%s| (%d)", cmd, wfr);
    
    if (wfr){                   /* wait for reply? */	
	response = output_read_reply(output);
	if (response == NULL) return -1;

        MSG2(5, "output_module", "Reply from output module: |%s|", response);

        if (response[0] == '3'){
            MSG(2, "Error: Module reported error in request from speechd (code 3xx).");
	    spd_free(response);
            return -2; /* User (speechd) side error */
        }

        if (response[0] == '4'){
            MSG(2, "Error: Module reported error in itself (code 4xx).");
	    spd_free(response);
            return -3; /* Module side error */
        }

        if (response[0] == '2'){
	    return 0;
        }else{                  /* unknown response */
            MSG(3, "Unknown response from output module!");
	    spd_free(response);
            return -3;
        }
    }       
    
    return 0;
}

#define SEND_CMD_N(cmd) \
  {  err = output_send_data(cmd"\n", output, 1); \
    if (err < 0) return (err); }

#define SEND_CMD(cmd) \
  {  err = output_send_data(cmd"\n", output, 1); \
    if (err < 0) OL_RET(err)}

#define SEND_DATA_N(data) \
  {  err = output_send_data(data, output, 0); \
    if (err < 0) return (err); }

#define SEND_DATA(data) \
  {  err = output_send_data(data, output, 0); \
    if (err < 0) OL_RET(err); }

#define SEND_CMD_GET_VALUE(data) \
  {  err = output_send_data(data"\n", output, 1); \
    OL_RET(err); }

#define ADD_SET_INT(name) \
    g_string_append_printf(set_str, #name"=%d\n", msg->settings.name);
#define ADD_SET_STR(name) \
    if (msg->settings.name != NULL){ \
       g_string_append_printf(set_str, #name"=%s\n", msg->settings.name); \
    }else{ \
       g_string_append_printf(set_str, #name"=NULL\n"); \
    }
#define ADD_SET_STR_C(name, fconv) \
    val = fconv(msg->settings.name); \
    if (val != NULL){ \
       g_string_append_printf(set_str, #name"=%s\n", val); \
    }else{ \
       g_string_append_printf(set_str, #name"=NULL\n"); \
    } \
    spd_free(val);

int
output_send_settings(TSpeechDMessage *msg, OutputModule *output)
{
    GString *set_str;
    char *val;
    int err;

    MSG(4, "Module set parameters.");
    set_str = g_string_new("");
    ADD_SET_INT(pitch);
    ADD_SET_INT(rate);
    ADD_SET_INT(volume);
    ADD_SET_STR_C(punctuation_mode, EPunctMode2str);
    ADD_SET_STR_C(spelling_mode, ESpellMode2str);
    ADD_SET_STR_C(cap_let_recogn, ECapLetRecogn2str);
    ADD_SET_STR(language);
    ADD_SET_STR_C(voice, EVoice2str);

    SEND_CMD_N("SET");
    SEND_DATA_N(set_str->str);
    SEND_CMD_N(".");

    g_string_free(set_str, 1);

    return 0;
}
#undef ADD_SET_INT
#undef ADD_SET_STR


int
output_speak(TSpeechDMessage *msg)
{
    OutputModule *output;
    int err;
    int ret;

    if(msg == NULL) return -1;

    output_lock();


    /* Determine which output module should be used */
    output = get_output_module(msg);
    if (output == NULL){
        MSG(3, "Output module doesn't work...");
        OL_RET(-1)
    }                    

    msg->buf = escape_dot(msg->buf);
    msg->bytes = -1;

    output_set_speaking_monitor(msg, output);

    ret = output_send_settings(msg, output);
    if (ret != 0) OL_RET(ret);

    MSG(4, "Module speak!");


    switch(msg->settings.type)
        {
        case MSGTYPE_TEXT: SEND_CMD("SPEAK") break;
        case MSGTYPE_SOUND_ICON: SEND_CMD("SOUND_ICON"); break;
        case MSGTYPE_CHAR: SEND_CMD("CHAR"); break;
        case MSGTYPE_KEY: SEND_CMD("KEY"); break;
        default: MSG(2,"Invalid message type in output_speak()!");
        }

    SEND_DATA(msg->buf)
    SEND_CMD("\n.")
 
    OL_RET(0)
}

int
output_stop()
{
    int err;
    OutputModule *output;

    output_lock();

    if (speaking_module == NULL) OL_RET(0)
    else output = speaking_module;

    MSG(4, "Module stop!");     
    SEND_DATA("STOP\n");

    OL_RET(0)
}

size_t
output_pause()
{
    static int err;
    static OutputModule *output;

    output_lock();

    if (speaking_module == NULL) OL_RET(0)
    else output = speaking_module;

    MSG(4, "Module pause!");
    SEND_DATA("PAUSE\n");

    OL_RET(0)
}

int
output_module_is_speaking(OutputModule *output, char **index_mark)
{
    int err;
    int ret;
    char *response;

    output_lock();

    response = output_read_reply(output);
    if (response == NULL){
	*index_mark = NULL;
	return -1;
    }

    MSG2(5, "output_module", "Reply from output module: |%s|", response);
    if (response[0] == '3'){
	MSG(2, "Error: Module reported error in request from speechd (code 3xx).");
	OL_RET(-2); /* User (speechd) side error */
    }
    if (response[0] == '4'){
	MSG(2, "Error: Module reported error in itself (code 4xx).");
	OL_RET(-3); /* Module side error */
    }
    if (response[0] == '2'){	
	if (strlen(response) > 4){
	    if (response[3] == '-'){
		char *p;                         
		p = strchr(response, '\n');                
		*index_mark = (char*) strndup(response+4, p-response-4);
		MSG2(5, "output_module", "Detected INDEX MARK: %s", *index_mark);
	    }else{
		MSG2(2, "output_module", "Error: Wrong communication from output module!"
		    "Reply on SPEAKING not multi-line.");
		OL_RET(-1); 
	    }
	}else{
	    MSG2(2, "output_module", "Error: Wrong communication from output module! Reply less than four bytes.");
	    OL_RET(-1); 
	}
	OL_RET(0)
    }else{                  /* unknown response */
	MSG(3, "Unknown response from output module!");
	OL_RET(-3);
    }

    OL_RET(-1)
}

int
output_is_speaking(char **index_mark)
{
    int err;
    OutputModule *output;

    if (speaking_module == NULL){
	index_mark = NULL;
	return 0;
    }else{
	output = speaking_module;
    }

    err = output_module_is_speaking(output, index_mark);   
    if (err < 0){
	*index_mark = NULL;
    }
    return err;
}

int
output_close(OutputModule *module)
{
    int err;
    OutputModule *output;
    output = module;

    if (output == NULL) return -1;
    
    output_lock();

    assert(output->name != NULL);
    MSG(3, "Closing module \"%s\"...", output->name);
    if (output->working){
        SEND_CMD("STOP");
        SEND_CMD("QUIT");
        usleep(100);            /* So that the module has some time to exit() correctly */
    }
    kill(module->pid, SIGKILL); /* If the module didn't manage to exit */

    waitpid(module->pid, NULL, WNOHANG);
    MSG(3, "Ok, closed succesfully.");
   
    OL_RET(0)
}

#undef SEND_CMD
#undef SEND_DATA

int
output_check_module(OutputModule* output)
{
    int ret;
    int err;
    int status;

    if(output == NULL) return -1;

    MSG(3, "Output module working status: %d (pid:%d)", output->working, output->pid);

    if (output->working == 0){
        /* Investigate on why it crashed */
        ret = waitpid(output->pid, &status, WNOHANG);
        if (ret == 0){
            MSG(2, "Output module not running.");
            return 0;
        }
        ret = WIFEXITED(status);

	/* TODO: Linux kernel implementation of threads is not very good :(  */
	//        if (ret == 0){
	if (1){
            /* Module terminated abnormally */
            MSG(2, "Output module terminated abnormally, probably crashed.");
        }else{
            /* Module terminated normally, check status */
            err = WEXITSTATUS(status);
            if (err == 0) MSG(2, "Module exited normally");
            if (err == 1) MSG(2, "Internal error in output module!");
            if (err == 2){
                MSG(2, "Output device not working. For software devices, this can mean"
                "that they are not running or they are not accessible due to wrong"
                "acces permissions.");
            }
            if (err > 2) MSG(2, "Unknown error happened in output module, exit status: %d !", err);            
        }
    }
    return 0;
}


char*
escape_dot(char *otext)
{
    char *seq;
    GString *ntext;
    char *ootext;
    char *line;
    char *ret = NULL;
    int len;

    if (otext == NULL) return NULL;

    MSG2(5, "escaping", "Incomming text: |%s|", otext);

    ootext = otext;

    ntext = g_string_new("");

    if (strlen(otext) == 1){
        if (!strcmp(otext, ".")){
            g_string_append(ntext, "..");
            otext += 1;
        }
    }

    if (strlen(otext) >= 2){
        if ((otext[0] == '.') && (otext[1] == '\n')){
            g_string_append(ntext, "..\n");
            otext = otext+2;
        }
    }

    MSG2(6, "escaping", "Altering text (I): |%s|", ntext->str);

    while (seq = strstr(otext, "\n.\n")){
        *seq = 0;
        g_string_append(ntext, otext);
        g_string_append(ntext, "\n..\n");
        otext = seq+3;
    }

    MSG2(6, "escaping", "Altering text (II): |%s|", ntext->str);    

    len = strlen(otext);
    if (len >= 2){
        if ((otext[len-2] == '\n') && (otext[len-1] == '.')){
            g_string_append(ntext, otext);
            g_string_append(ntext, ".");
            otext = otext+len;
            MSG2(6, "escaping", "Altering text (II-b): |%s|", ntext->str);    
        }
    }

    if (otext == ootext){
	g_string_free(ntext, 1);
        ret = otext;
    }else{
        g_string_append(ntext, otext);
        free(ootext);
        ret = ntext->str;
	g_string_free(ntext, 0);
    }

    MSG2(6, "escaping", "Altered text: |%s|", ret);

    return ret;
}


