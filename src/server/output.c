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
 * $Id: output.c,v 1.2 2003-09-07 23:26:27 hanke Exp $
 */

#include "output.h"

/* TODO: Correct macro */
#define TEMP_FAILURE_RETRY(expr) (expr)

static OutputModule*
get_output_module(const TSpeechDMessage *message)
{
    OutputModule *output = NULL;

    if (message->settings.type != MSGTYPE_SOUND){
        if (message->settings.output_module != NULL){
            output = g_hash_table_lookup(output_modules, message->settings.output_module);
            if(output == NULL || !output->working){
                if (GlobalFDSet.output_module != NULL){
                    MSG(4,"Didn't find prefered output module, using default");                
                    output = g_hash_table_lookup(output_modules, GlobalFDSet.output_module); 
                    if (output == NULL || !output->working) 
                        MSG(2, "Can't find default output module or it's not working!");
                }
            }
        }
        if (output == NULL){
            MSG(3, "Unspecified output module!\n");
            return NULL;
        }
    }else{   /* if MSGTYPE_SOUND */
        output = sound_module;
        if (output == NULL) MSG(2,"Can't find sound module!");
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

int
output_send_data(char* cmd, OutputModule *output, int wfr)
{
    int ret;
    static char response[256];
    char *tptr;
    int num_resp;

    if (output == NULL) return -1;
    if (cmd == NULL) return -1;

    ret = TEMP_FAILURE_RETRY(write(output->pipe_in[1], cmd, strlen(cmd)));
    fflush(NULL);
    if (ret == -1){
        MSG(3, "Broken pipe to module. Module exiting or crashed.");
        output->working = 0;
        speaking_module = NULL;
        return -1;   /* Broken pipe */
    }
    MSG(5,"Command sent to output module: |%s|", cmd);

    if (wfr){                   /* wait for reply? */
        ret = TEMP_FAILURE_RETRY(read(output->pipe_out[0], response, 255));
        if ((ret == -1) || (ret == 0)){
            MSG(3, "Broken pipe to module. Module exiting or crashed.");
            output->working = 0;
            speaking_module = NULL;
            return -1; /* Broken pipe */       
        }
        MSG(5,"Reply from output module: |%s|", response);
        fflush(NULL);
        if (response[0] == '3'){
            MSG(3, "Module reported error in request from speechd (code 3xx).");
            return -2; /* User (speechd) side error */
        }
        if (response[0] == '4'){
            MSG(3, "Module reported error in itself (code 4xx).");
            return -3; /* Module side error */
        }
        if (response[0] == '2'){
            if (ret > 4){
                if (response[3] == '-'){
                    num_resp = strtol(&(response[4]), &tptr, 10);
                    if (tptr == &(response[4])){
                        MSG(3, "Invalid response from output module.");
                        output->working = 0; /* The syntax of what we read and what
                                                we expect to read is now messed up,
                                                so better not to use the module more. */
                        speaking_module = NULL;
                        return -4;
                    }
                    /* Check if we got the whole two lines or just one */
                    /* TODO: Fix this ugly hack with strstr bellow */
                    if (strstr(response, "\n2") == NULL){
                        /* Read the rest of the response (the last line) */
                        ret = TEMP_FAILURE_RETRY(read(output->pipe_out[0], response, 255));
                        if ((ret == -1) || (ret == 0)){
                            MSG(3, "Broken pipe to module. Module crashed?");
                            output->working = 0;
                            speaking_module = NULL;
                            return -1; /* Broken pipe */       
                        }
                    }
                    return num_resp;
                }
                return 0;
            }
            return 0;
        }else{                  /* unknown response */
            MSG(3, "Unknown response from output module!");
            return -3;
        }
    }       
    return 0;
}

#define SEND_CMD(cmd) \
  {  err = output_send_data(cmd"\n", output, 1); \
    if (err < 0) OL_RET(err); }

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
       g_string_append_printf(set_str, #name"=NULL"); \
    }

int
output_speak(TSpeechDMessage *msg)
{
    OutputModule *output;
    GString *set_str;
    int err;

    if(msg == NULL) return -1;

    output_lock();

    /* Determine which output module should be used */
    output = get_output_module(msg);
    if (output == NULL){
        MSG(4, "Output module NULL...");
        OL_RET(-1)
    }                    

    /* Set the speaking-monitor so that we know who is speaking */
    speaking_module = output;
    speaking_uid = msg->settings.uid;
    speaking_gid = msg->settings.reparted;

    MSG(4, "Module set.");
    set_str = g_string_new("");
    ADD_SET_INT(pitch);
    ADD_SET_INT(rate);
    ADD_SET_INT(punctuation_mode);
    ADD_SET_STR(punctuation_some);
    ADD_SET_STR(punctuation_table);
    ADD_SET_INT(spelling_mode);
    ADD_SET_STR(spelling_table);
    ADD_SET_INT(cap_let_recogn);
    ADD_SET_STR(cap_let_recogn_table);
    ADD_SET_STR(cap_let_recogn_sound);
    ADD_SET_STR(language);
    ADD_SET_INT(voice);

    SEND_CMD("SET");
    SEND_DATA(set_str->str);
    SEND_CMD(".");

    g_string_free(set_str, 1);

    MSG(4, "Module speak!");
    SEND_CMD("SPEAK");
    SEND_DATA(msg->buf);
    SEND_CMD("\n.");

    OL_RET(0)
}
#undef ADD_SET_INT
#undef ADD_SET_STR

int
output_stop()
{
    int err;
    OutputModule *output;

    output_lock();

    if (speaking_module == NULL) OL_RET(0)
    else output = speaking_module;

    MSG(4, "Module stop!");     
    SEND_CMD("STOP");

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
    SEND_CMD_GET_VALUE("PAUSE");

    OL_RET(-1)
}

int
output_module_is_speaking(OutputModule *output)
{
    int err;

    output_lock();

    SEND_CMD_GET_VALUE("SPEAKING");   

    OL_RET(-1)
}

int
output_is_speaking()
{
    int err;
    OutputModule *output;

    if (speaking_module == NULL) OL_RET(0)
    else output = speaking_module;

    return output_module_is_speaking(output);   
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
        sleep(10);
    }
    kill(module->pid, SIGKILL);

    waitpid(module->pid, NULL, WNOHANG);
    MSG(3, "Ok, closed succesfully.");
   
    OL_RET(0)
}

int
output_filtering_generic(const TSpeechDMessage *msg)
{
    OutputModule *output;

    if (msg == NULL) return 0;

    output = get_output_module(msg);
    if (output == NULL){
        MSG(4, "Output module NULL in output_filtering_generic().");
        return 0;
    }                        

    if (output->filtering == FILTERING_GENERIC){
        MSG(4,"Using generic filtering for this message.");
        return 1;
    }else{
        MSG(4,"Filtering is left on the output module or not done.");
        return 0;
    }
}

#undef SEND_CMD
#undef SEND_DATA
