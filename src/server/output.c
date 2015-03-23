/*
 * output.c - Output layer for Speech Dispatcher
 *
 * Copyright (C) 2001, 2002, 2003, 2007 Brailcom, o.p.s.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * $Id: output.c,v 1.38 2008-06-27 12:28:48 hanke Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fdsetconv.h>
#include <safe_io.h>
#include <spd_utils.h>
#include "output.h"
#include "parse.h"

#if !(defined(__GLIBC__) && defined(_GNU_SOURCE))
/* Added by Willie Walker - strndup is a gcc-ism
 */
char *strndup ( const char *s, size_t n)
{
        size_t nAvail;
        char *p;

        if ( !s )
                return 0;

        if ( strlen(s) > n )
                nAvail = n + 1;
        else
                nAvail = strlen(s) + 1;
        p = g_malloc ( nAvail );
        memcpy ( p, s, nAvail );
        p[nAvail - 1] = '\0';

        return p;
}
#endif /* !(defined(__GLIBC__) && defined(_GNU_SOURCE)) */

void
output_set_speaking_monitor(TSpeechDMessage *msg, OutputModule *output)
{
    /* Set the speaking-monitor so that we know who is speaking */
    speaking_module = output;
    speaking_uid = msg->settings.uid;
    speaking_gid = msg->settings.reparted;
}

OutputModule*
get_output_module_by_name(char *name)
{
  OutputModule *output;
  int i;

  for (i = 0; i < g_list_length(output_modules); i++) {
      output = g_list_nth_data(output_modules, i);
      if(!strcmp(output->name, name)) {
	  if(output->working)
	      return output;
	  else
	      return NULL;
      }
  }
  
  return NULL;
}

/* get_output_module tries to return a pointer to the
   appropriate output module according to message context.
   If it is not possible to find the required module,
   it will subsequently try to get the default module,
   any of the other remaining modules except dummy and
   at last, the dummy output module.

   Only if not even dummy output module is working
   (serious issues), it will log an error message and return
   a NULL pointer.

*/

OutputModule*
get_output_module(const TSpeechDMessage *message)
{
  OutputModule *output = NULL;
  int i, len;

  if (message->settings.output_module != NULL){
    MSG(5, "Desired output module is %s", message->settings.output_module);
    output = get_output_module_by_name(message->settings.output_module);
    if((output != NULL) && output->working) return output;
  }

  MSG(3,"Warning: Didn't find preferred output module, using default");                
  // If the requested module was not found or is not working,
  // first try to use the default output module
  if (GlobalFDSet.output_module != NULL)
    output = get_output_module_by_name(GlobalFDSet.output_module); 

  if (output != NULL && output->working) return output;

  MSG(3, "Couldn't load default output module, trying other modules");

  /* Try all other output modules other than dummy */
  len = g_list_length(output_modules);
  for (i = 0; i < len; i++) {
      output = g_list_nth_data(output_modules, i);
      if (0 == strcmp(output->name, "dummy"))
           continue;

      if (output->working) {
          MSG(3, "Output module %s seems to be working, using it", output->name);
          return output;
      }
  }

  // if we get here there are no good modules use the dummy
  // a pre-synthesized error message with some hints over and over).
  if (output == NULL || !output->working)
    output = get_output_module_by_name("dummy");

      // Give up....
  if (output == NULL)
    MSG(1, "Error: No output module working, not even dummy, no sound produced!\n");

  return output;
}

void
static output_lock(void)
{
    pthread_mutex_lock(&output_layer_mutex);
}

void
static output_unlock(void)
{
    pthread_mutex_unlock(&output_layer_mutex);
}

#define OL_RET(value) \
  {  output_unlock(); \
    return (value); }

GString*
output_read_reply(OutputModule *output)
{
    GString *rstr;
    int bytes;
    char *line = NULL;
    size_t N = 0;
    gboolean errors = FALSE;
    
    rstr = g_string_new("");
    
    /* Wait for activity on the socket, when there is some,
       read all the message line by line */
    do{
	bytes = spd_getline(&line, &N, output->stream_out);
	if (bytes == -1){
	    MSG(2, "Error: Broken pipe to module.");
	    output->working = 0;
	    speaking_module = NULL;
	    output_check_module(output);
	    errors = TRUE; /* Broken pipe */   
	} else {
	    MSG(5, "Got %d bytes from output module over socket", bytes);
	    g_string_append(rstr, line);
	}
	/* terminate if we reached the last line (without '-' after numcode) */
    }while( !errors && !((strlen(line) < 4) || (line[3] == ' ')));

    if(line != NULL)
	g_free(line);

    if(errors) {
	g_string_free(rstr, TRUE);
	rstr = NULL;
    }

    return rstr;
}

char*
output_read_reply2(OutputModule *output)
{
    int bytes;
    char *reply;
    

    reply = g_malloc( 1024 * sizeof(char));

    bytes = read(output->pipe_out[0], reply, 1024);
    reply[bytes] = 0;
    MSG2(1, "output_module", "2Read: %d bytes: <%s>", bytes, reply);

    return reply;
}

int
output_send_data(char* cmd, OutputModule *output, int wfr)
{
    int ret;
    GString *response;

    if (output == NULL) return -1;
    if (cmd == NULL) return -1;
    
    ret = safe_write(output->pipe_in[1], cmd, strlen(cmd));
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
	int ret = 0;
	response = output_read_reply(output);
	if (response == NULL) return -1;

        MSG2(5, "output_module", "Reply from output module: |%s|", response->str);

	switch (response->str[0]){
	    case '3':
		MSG(2, "Error: Module reported error in request from speechd (code 3xx): %s.", response->str);
		ret = -2; /* User (speechd) side error */
		break;

	    case '4':
		MSG(2, "Error: Module reported error in itself (code 4xx): %s", response->str);
		ret = -3; /* Module side error */
		break;

	    case '2':
		ret = 0;
		break;
	    default:                  /* unknown response */
		MSG(3, "Unknown response from output module!");
		ret = -3;
		break;
	}
    g_string_free(response, TRUE);
    return ret;
    }       
    
    return 0;
}

static SPDVoice**
output_get_voices(OutputModule *module)
{
  SPDVoice** voice_dscr;
  GString *reply;
  gchar **lines;
  gchar **atoms;
  int i;
  int ret = 0;
  gboolean errors = FALSE;

  output_lock();

  if (module == NULL){
    MSG(1, "ERROR: Can't list voices for broken output module");
    OL_RET(NULL);
  }
  output_send_data("LIST VOICES\n", module, 0);
  reply = output_read_reply(module);

  if (reply == NULL){
      output_unlock();
      return NULL;
  }

  //TODO: only 256 voices supported here
  lines = g_strsplit(reply->str, "\n", 256);
  g_string_free(reply, TRUE);
  voice_dscr = g_malloc(256*sizeof(SPDVoice*));
  for (i = 0; !errors && (lines[i] != NULL); i++) {
    MSG(1, "LINE here:|%s|", lines[i]);
    if (strlen(lines[i])<=4){
      MSG(1, "ERROR: Bad communication from driver in synth_voices");
      ret = -1;
      errors = TRUE;
    }
    else if (lines[i][3] == ' ')
      break;
    else if (lines[i][3] == '-'){
      atoms = g_strsplit(&lines[i][4]," ", 0);
      // Name, language, dialect
      if ((atoms[0] == NULL) || (atoms[1] == NULL) || (atoms[2] == NULL)){
	ret = -1;
      errors = TRUE;
      } else {
        //Fill in VoiceDescription
        voice_dscr[i] = g_malloc(sizeof(SPDVoice));
        voice_dscr[i]->name=g_strdup(atoms[0]);
        voice_dscr[i]->language=g_strdup(atoms[1]);
        voice_dscr[i]->variant=g_strdup(atoms[2]);
      }
    if (atoms != NULL)
      g_strfreev(atoms);
    }
    /* Should we do something in a final "else" branch? */

  }
  voice_dscr[i] = NULL;
  g_strfreev(lines);

  output_unlock();
  return voice_dscr;
}

SPDVoice**
output_list_voices(char* module_name)
{
  OutputModule *module;
  if (module_name == NULL) return NULL;
  module=get_output_module_by_name(module_name);
  if (module == NULL){
    MSG(1, "ERROR: Can't list voices for module %s", module_name);
    return NULL;
  }
  return output_get_voices(module);
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
    val = fconv(msg->settings.msg_settings.name); \
    if (val != NULL){ \
       g_string_append_printf(set_str, #name"=%s\n", val); \
    } \
    g_free(val);

int
output_send_settings(TSpeechDMessage *msg, OutputModule *output)
{
    GString *set_str;
    char *val;
    int err;

    MSG(4, "Module set parameters.");
    set_str = g_string_new("");
    g_string_append_printf(set_str, "pitch=%d\n",
                           msg->settings.msg_settings.pitch);
    g_string_append_printf(set_str, "rate=%d\n",
                           msg->settings.msg_settings.rate);
    g_string_append_printf(set_str, "volume=%d\n",
                           msg->settings.msg_settings.volume);
    ADD_SET_STR_C(punctuation_mode, EPunctMode2str);
    ADD_SET_STR_C(spelling_mode, ESpellMode2str);
    ADD_SET_STR_C(cap_let_recogn, ECapLetRecogn2str);
    val = EVoice2str(msg->settings.msg_settings.voice_type);
    if (val != NULL){
        g_string_append_printf(set_str, "voice=%s\n", val);
    }
    g_free(val);
    if (msg->settings.msg_settings.voice.language != NULL) {
        g_string_append_printf(set_str, "language=%s\n",
                               msg->settings.msg_settings.voice.language);
    } else {
       g_string_append_printf(set_str, "language=NULL\n");
    }
    if (msg->settings.msg_settings.voice.name != NULL) {
       g_string_append_printf(set_str, "synthesis_voice=%s\n",
                              msg->settings.msg_settings.voice.name);
    } else {
        g_string_append_printf(set_str, "synthesis_voice=NULL\n");
    }

    SEND_CMD_N("SET");
    SEND_DATA_N(set_str->str);
    SEND_CMD_N(".");

    g_string_free(set_str, 1);

    return 0;
}
#undef ADD_SET_INT
#undef ADD_SET_STR


#define ADD_SET_INT(name) \
    g_string_append_printf(set_str, #name"=%d\n", GlobalFDSet.name);
#define ADD_SET_STR(name) \
    if (GlobalFDSet.name != NULL){ \
       g_string_append_printf(set_str, #name"=%s\n", GlobalFDSet.name); \
    }else{ \
       g_string_append_printf(set_str, #name"=NULL\n"); \
    }

int
output_send_audio_settings(OutputModule *output)
{
    GString *set_str;
    int err;

    MSG(4, "Module set parameters.");
    set_str = g_string_new("");
    ADD_SET_STR(audio_output_method);
    ADD_SET_STR(audio_oss_device);
    ADD_SET_STR(audio_alsa_device);
    ADD_SET_STR(audio_nas_server);
    ADD_SET_STR(audio_pulse_server);
    ADD_SET_INT(audio_pulse_min_length);

    SEND_CMD_N("AUDIO");
    SEND_DATA_N(set_str->str);
    SEND_CMD_N(".");

    g_string_free(set_str, 1);

    return 0;
}

int
output_send_loglevel_setting(OutputModule *output)
{
    GString *set_str;
    int err;

    MSG(4, "Module set parameters.");
    set_str = g_string_new("");
    ADD_SET_INT(log_level);

    SEND_CMD_N("LOGLEVEL");
    SEND_DATA_N(set_str->str);
    SEND_CMD_N(".");

    g_string_free(set_str, 1);

    return 0;
}
#undef ADD_SET_INT
#undef ADD_SET_STR

int
output_send_debug(OutputModule *output, int flag, char* log_path)
{
    char *cmd_str;
    int err;

    MSG(4, "Module sending debug flag %d with file %s", flag, log_path);
    
    output_lock();
    if (flag){
      cmd_str = g_strdup_printf("DEBUG ON %s \n", log_path);
      err = output_send_data(cmd_str, output, 1);
      g_free(cmd_str);
      if (err){
	MSG(3, "ERROR: Can't set debugging on for output module %s", output->name);
	OL_RET(-1);
      }      
    }else{
      err = output_send_data("DEBUG OFF \n", output, 1);
      if (err){
	MSG(3, "ERROR: Can't switch debugging off for output module %s", output->name);
	OL_RET(-1);
      }
      
    }
   
    OL_RET(0);
}

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
        case SPD_MSGTYPE_TEXT: SEND_CMD("SPEAK") break;
        case SPD_MSGTYPE_SOUND_ICON: SEND_CMD("SOUND_ICON"); break;
        case SPD_MSGTYPE_CHAR: SEND_CMD("CHAR"); break;
        case SPD_MSGTYPE_KEY: SEND_CMD("KEY"); break;
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
    GString *response;
    int retcode = -1;

    output_lock();

    MSG(5, "output_module_is_speaking()");

    if (output == NULL){
        MSG(5, "output==NULL in output_module_is_speaking()");
    	OL_RET(-1);
    }

    response = output_read_reply(output);
    if (response == NULL){
	*index_mark = NULL;
	OL_RET(-1);
    }    

    MSG2(5, "output_module", "Reply from output module: |%s|", response->str);

    if (response->len < 4){
	MSG2(2, "output_module",
	     "Error: Wrong communication from output module! Reply less than four bytes.");
	g_string_free(response, TRUE);
	OL_RET(-1); 
    }

    switch(response->str[0]) {
    	case '3':
	    MSG(2, "Error: Module reported error in request from speechd (code 3xx).");
	    retcode = -2; /* User (speechd) side error */
	    break;

	case '4':
	    MSG(2, "Error: Module reported error in itself (code 4xx).");
	    retcode = -3; /* Module side error */
	    break;

	case '2':
	    retcode = 0;
	    if (response->len > 4){
		if (response->str[3] == '-'){
		    char *p;
		    p = strchr(response->str, '\n');
		    *index_mark = (char*) strndup(response->str+4, p-response->str-4);
		    MSG2(5, "output_module", "Detected INDEX MARK: %s", *index_mark);
		}else{
		    MSG2(2, "output_module", "Error: Wrong communication from output module!"
			"Reply on SPEAKING not multi-line.");
		    retcode = -1; 
		}
	    }
	    break;

	case '7':
	    retcode = 0;
	    MSG2(5, "output_module", "Received event:\n %s", response->str);
	    if (!strncmp(response->str, "701", 3))
		*index_mark = (char*) g_strdup("__spd_begin");
	    else if (!strncmp(response->str, "702", 3))
		*index_mark = (char*) g_strdup("__spd_end");
	    else if (!strncmp(response->str, "703", 3))
		*index_mark = (char*) g_strdup("__spd_stopped");
	    else if (!strncmp(response->str, "704", 3))
		*index_mark = (char*) g_strdup("__spd_paused");
	    else if (!strncmp(response->str, "700", 3)) {
		char *p;
		p = strchr(response->str, '\n');
		MSG2(5, "output_module", "response:|%s|\n p:|%s|", response->str, p);
		*index_mark = (char*) strndup(response->str+4, p-response->str-4);
		MSG2(5, "output_module", "Detected INDEX MARK: %s", *index_mark);
	    } else {
		MSG2(2, "output_module", "ERROR: Unknown event received from output module");
		retcode = -5;
	    }
	    break;

	default:                  /* unknown response */
	    MSG(3, "Unknown response from output module!");
	    retcode = -3;
	    break;

    }

    g_string_free(response, TRUE);
    OL_RET(retcode)
}

int
output_is_speaking(char **index_mark)
{
    int err;
    OutputModule *output;

    output = speaking_module;

    err = output_module_is_speaking(output, index_mark);   
    if (err < 0){
	*index_mark = NULL;
    }

    return err;
}

/* Wait until the child _pid_ returns with timeout. Calls waitpid() each 100ms
 until timeout is exceeded. This is not exact and you should not rely on the 
 exact time waited. */
int
waitpid_with_timeout(pid_t pid, int *status_ptr, int options,
		     size_t timeout)
{
    size_t i;
    int ret;
    for (i=0; i<= timeout; i+=100){
	ret = waitpid(pid, status_ptr, options | WNOHANG);
	if (ret > 0) return ret;
	if (ret < 0) return ret;
	usleep(100 * 1000);	/* Sleep 100 ms */
    }
    return 0;    
}

int
output_close(OutputModule *module)
{
    int err;
    int ret;
    OutputModule *output;
    output = module;

    if (output == NULL) return -1;
    
    output_lock();

    assert(output->name != NULL);
    MSG(3, "Closing module \"%s\"...", output->name);
    if (output->working){
        SEND_DATA("STOP\n");
        SEND_CMD("QUIT");
        usleep(100);
	/* So that the module has some time to exit() correctly */
    }

    MSG(4, "Waiting for module pid %d", module->pid);
    ret = waitpid_with_timeout(module->pid, NULL, 0, 1000); 
    if (ret > 0){
	MSG(4, "Ok, module closed successfully.");
    }else if (ret == 0){
	int ret2;
	MSG(1, "ERROR: Timed out when waiting for child cancellation");
	MSG(3, "Killing the module");
	kill(module->pid, SIGKILL);
	MSG(4, "Waiting until the child terminates.");
	ret2 = waitpid_with_timeout(module->pid, NULL, 0, 1000);
	if (ret2 > 0){
	    MSG(3, "Module terminated");
	}else{
	    MSG(1, "ERROR: Module is not able to terminate, giving up.");
	}
    }else{
	MSG(1, "ERROR: waitpid() failed when waiting for child (module).");
    }
    
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

    MSG(4, "Output module working status: %d (pid:%d)", output->working, output->pid);

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
    char *ret = NULL;
    int len;

    if (otext == NULL) return NULL;

    MSG2(5, "escaping", "Incoming text: |%s|", otext);

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

    while ( (seq = strstr(otext, "\n.\n")) ){
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
        g_free(ootext);
        ret = ntext->str;
	g_string_free(ntext, 0);
    }

    MSG2(6, "escaping", "Altered text: |%s|", ret);

    return ret;
}
