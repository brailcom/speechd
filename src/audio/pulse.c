/*
 * pulse.c -- The PulseAudio backend for the spd_audio library.
 * based on xmms-pulse (GPLv2) by Lennart Poettering:
 * http://0pointer.de/lennart/projects/xmms-pulse/ 
 *
 * Copyright (C) 2004,2006 Brailcom, o.p.s.
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1, or (at your option) any later
 * version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this package; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * $Id: pulse.c,v 1.13 2009-05-14 09:00:28 hanke Exp $
 */

/* debug */
// #define DEBUG_PULSE 

#include <stdio.h>
#include <stdarg.h>

enum {
  /* 10ms. 
     If a greater value is set (several seconds), 
     please update _pulse_timeout_start accordingly */
  PULSE_TIMEOUT_IN_USEC = 10000,  // in microseconds

  /* return value */
  PULSE_OK = 0,
  PULSE_ERROR = -1,
  PULSE_NO_CONNECTION = -2
};

#ifdef DEBUG_PULSE

#define ENTER(text) debug_enter(text)
#define SHOW(format,...) debug_show(format,__VA_ARGS__)
#define SHOW_TIME(text) debug_time(text)
#define ERR(arg,...) debug_show(arg,__VA_ARGS__)
#define DISPLAY_ID(id, s) display_id(id, s)

#else

#define ENTER(text)
#define SHOW(format,...)
#define SHOW_TIME(text)
#define DISPLAY_ID(id, s) 


#define ERR(arg...)					\
  {							\
    time_t t;						\
    struct timeval tv;					\
    char *tstr;						\
    t = time(NULL);					\
    tstr = strdup(ctime(&t));				\
    tstr[strlen(tstr)-1] = 0;				\
    gettimeofday(&tv,NULL);				\
    fprintf(stderr," %s [%d]",tstr, (int) tv.tv_usec);	\
    fprintf(stderr," PulseAudio ERROR: ");		\
    fprintf(stderr,arg);				\
    fprintf(stderr,"\n");				\
    fflush(stderr);					\
    xfree(tstr);					\
  }


#endif


#ifdef DEBUG_PULSE
#include <sys/time.h>
#include <unistd.h>

static FILE* fd_log = NULL;
static const char* FILENAME = "/tmp/pulse.log";

static void 
debug_init()
{
  fd_log = fopen(FILENAME,"a");
  setvbuf(fd_log, NULL, _IONBF, 0);
}

static void 
debug_enter(const char* text)
{
  struct timeval tv;

  gettimeofday(&tv, NULL);                  

  if (!fd_log)
    {
      debug_init();
    }

  if (fd_log)
    {
      fprintf(fd_log, "%03d.%03dms > ENTER %s\n",(int)(tv.tv_sec%1000), (int)(tv.tv_usec/1000), text);
    }
}


static void 
debug_show(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  if (!fd_log)
    {
      debug_init();
    }
  if (fd_log)
    {
      vfprintf(fd_log, format, args);
    }  
  va_end(args);
}

static void 
debug_time(const char* text)
{
  struct timeval tv;

  gettimeofday(&tv, NULL);                  

  if (!fd_log)
    {
      debug_init();
    }
  if (fd_log)
    {
      fprintf(fd_log, "%03d.%03dms > %s\n",(int)(tv.tv_sec%1000), (int)(tv.tv_usec/1000), text);
    }
}

static void 
display_id(AudioID *id, char*s)
{
  debug_show("%s >\n id=0x%x\n pulse_context=0x%x\n pulse_stream=0x%x\n pulse_mainloop=0x%x\n pulse_volume=0x%x\npulse_volume_valid=0x%x\n pulse_do_trigger=0x%x\n pulse_time_offset_msec=0x%x\n pulse_just_flushed=0x%x\n pulse_connected=0x%x\n pulse_success=0x%x, pulse_stop_required=0x%x\n",
	     __FUNCTION__,
	     id, 
	     id->pulse_context,
	     id->pulse_stream,
	     id->pulse_mainloop, 
	     id->pulse_volume,
	     id->pulse_volume_valid,
	     id->pulse_do_trigger,
	     id->pulse_time_offset_msec,
	     id->pulse_just_flushed,
	     id->pulse_connected,
	     id->pulse_success,
	     id->pulse_stop_required);
}

#endif



/* internal services */


#define CHECK_DEAD_GOTO(id, label, warn) do {				\
    if (!id->pulse_mainloop ||						\
	!id->pulse_context || pa_context_get_state(id->pulse_context) != PA_CONTEXT_READY || \
	!id->pulse_stream || pa_stream_get_state(id->pulse_stream) != PA_STREAM_READY) { \
      if (warn)								\
	SHOW("Connection died: %s\n", id->pulse_context ? pa_strerror(pa_context_errno(id->pulse_context)) : "NULL"); \
      goto label;							\
    }									\
  } while(0);

#define CHECK_CONNECTED(id,retval)					\
  do {									\
    if (!id->pulse_connected){ ERR("CHECK_CONNECTED: !pulse_connected (retval=%d)\n", retval);return retval;} \
  } while (0);

#define CHECK_CONNECTED_NO_RETVAL(id)					\
  do {									\
    if (!id->pulse_connected){ ERR("CHECK_CONNECTED_NO_RETVAL: !pulse_connected\n", ""); return;	} \
  } while (0);



static void 
_info_cb(struct pa_context *c, const struct pa_sink_input_info *i, int is_last, void *userdata) 
{
  ENTER(__FUNCTION__);
  assert(c);

  AudioID *id = (AudioID *)userdata;

  if(!id)
    {
      ERR("%s() failed: userdata==NULL!", __FUNCTION__);
      return;
    }

  if (!i)
    return;

  id->pulse_volume = i->volume;
  id->pulse_volume_valid = 1;
}

static void 
_subscribe_cb(struct pa_context *c, enum pa_subscription_event_type t, uint32_t index, void *userdata) 
{
  pa_operation *o;
  AudioID *id = (AudioID *)userdata;
  ENTER(__FUNCTION__);
    
  assert(c);
  if(!id)
    {
      ERR("%s() failed: userdata==NULL!", __FUNCTION__);
      return;
    }

  if (!id->pulse_stream ||
      index != pa_stream_get_index(id->pulse_stream) ||
      (t != (PA_SUBSCRIPTION_EVENT_SINK_INPUT|PA_SUBSCRIPTION_EVENT_CHANGE) &&
       t != (PA_SUBSCRIPTION_EVENT_SINK_INPUT|PA_SUBSCRIPTION_EVENT_NEW)))
    return;

  if (!(o = pa_context_get_sink_input_info(c, index, _info_cb, userdata))) {
    ERR("pa_context_get_sink_input_info() failed: %s", pa_strerror(pa_context_errno(c)));
    return;
  }
    
  pa_operation_unref(o);
}

static void 
_context_state_cb(pa_context *c, void *userdata) {
  ENTER(__FUNCTION__);
  assert(c);
  AudioID *id = (AudioID *)userdata;
  if(!id) {
    ERR("%s() failed: userdata==NULL!", __FUNCTION__);
    return;
  }

  switch (pa_context_get_state(c)) {
  case PA_CONTEXT_READY:
  case PA_CONTEXT_TERMINATED:
  case PA_CONTEXT_FAILED:
    pa_threaded_mainloop_signal(id->pulse_mainloop, 0);
    break;

  case PA_CONTEXT_UNCONNECTED:
  case PA_CONTEXT_CONNECTING:
  case PA_CONTEXT_AUTHORIZING:
  case PA_CONTEXT_SETTING_NAME:
    break;
  }
}

static void 
_stream_state_cb(pa_stream *s, void * userdata) {
  ENTER(__FUNCTION__);
  assert(s);
  AudioID *id = (AudioID *)userdata;
  if(!id) {
    ERR("%s() failed: userdata==NULL!", __FUNCTION__);
    return;
  }

  switch (pa_stream_get_state(s)) {

  case PA_STREAM_READY:
  case PA_STREAM_FAILED:
  case PA_STREAM_TERMINATED:
    pa_threaded_mainloop_signal(id->pulse_mainloop, 0);
    break;

  case PA_STREAM_UNCONNECTED:
  case PA_STREAM_CREATING:
    break;
  }
}

static void 
_stream_success_cb(pa_stream *s, int success, void *userdata) {
  ENTER(__FUNCTION__);
  assert(s);
  AudioID *id = (AudioID *)userdata;
  if(!id) {
    ERR("%s() failed: userdata==NULL!", __FUNCTION__);
    return;
  }

  id->pulse_success = success;
  
  pa_threaded_mainloop_signal(id->pulse_mainloop, 0);
}

static void 
_context_success_cb(pa_context *c, int success, void *userdata) {
  ENTER(__FUNCTION__);
  assert(c);
  AudioID *id = (AudioID *)userdata;
  if(!id) {
    ERR("%s() failed: userdata==NULL!", __FUNCTION__);
    return;
  }

  id->pulse_success = success;
    
  pa_threaded_mainloop_signal(id->pulse_mainloop, 0);
}

static void 
_stream_request_cb(pa_stream *s, size_t length, void *userdata) {
  ENTER(__FUNCTION__);
  assert(s);
  AudioID *id = (AudioID *)userdata;
  if(!id) {
    ERR("%s() failed: userdata==NULL!", __FUNCTION__);
    return;
  }

  pa_threaded_mainloop_signal(id->pulse_mainloop, 0);
}

static void 
_stream_latency_update_cb(pa_stream *s, void *userdata) {
  /*   ENTER(__FUNCTION__); */
  assert(s);
  AudioID *id = (AudioID *)userdata;
  if(!id) {
    ERR("%s() failed: userdata==NULL!", __FUNCTION__);
    return;
  }

  pa_threaded_mainloop_signal(id->pulse_mainloop, 0);
}

static void 
_volume_time_cb(pa_mainloop_api *api, pa_time_event *e, const struct timeval *tv, void *userdata) 
{
  pa_operation *o;
  ENTER(__FUNCTION__);

  AudioID *id = (AudioID *)userdata;
  if(!id) {
    ERR("%s() failed: userdata==NULL!", __FUNCTION__);
    return;
  }
  
  if (!(o = pa_context_set_sink_input_volume(id->pulse_context, pa_stream_get_index(id->pulse_stream), &id->pulse_volume, NULL, NULL))) {
    ERR("pa_context_set_sink_input_volume() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
  }
  else
    pa_operation_unref(o);

  /* We don't wait for completion of this command */

  api->time_free(id->pulse_volume_time_event);
  id->pulse_volume_time_event = NULL;
}

static int 
_pulse_free(AudioID *id, size_t* length) {
  ENTER(__FUNCTION__);
  assert(id);
  assert(length);
  int ret = PULSE_ERROR;
  *length = 0;
  pa_operation *o = NULL;

  CHECK_CONNECTED(id,-1);

  SHOW("_pulse_free: %s (call)\n", "pa_threaded_main_loop_lock");
  pa_threaded_mainloop_lock(id->pulse_mainloop);
  CHECK_DEAD_GOTO(id, fail, 1);

  if ((*length = pa_stream_writable_size(id->pulse_stream)) == (size_t) -1) {
    ERR("pa_stream_writable_size() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    *length = 0;
    goto fail;
  }

  SHOW("_pulse_free: %s\n", "pa_stream_writable_size");

  ret = PULSE_OK;

  /* If this function is called twice with no _pulse_write() call in
   * between this means we should trigger the playback */
  if (id->pulse_do_trigger) {
    id->pulse_success = 0;

    SHOW("_pulse_free: %s (call)\n", "pa_stream_trigger");
    if (!(o = pa_stream_trigger(id->pulse_stream, _stream_success_cb, id))) {
      ERR("pa_stream_trigger() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
      goto fail;
    }
        
    SHOW("_pulse_free: %s (call)\n", "pa_threaded_main_loop");
    while (pa_operation_get_state(o) != PA_OPERATION_DONE) {
      CHECK_DEAD_GOTO(id, fail, 1);
      pa_threaded_mainloop_wait(id->pulse_mainloop);
    } 
    SHOW("_pulse_free: %s (ret)\n", "pa_threaded_main_loop");
       
    if (id->pulse_success == 0)
      ERR("pa_stream_trigger() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
  }
    
 fail:
  SHOW("_pulse_free: %s (call)\n", "pa_operation_unref");
  if (o)
    pa_operation_unref(o);
    
  SHOW("_pulse_free: %s (call)\n", "pa_threaded_main_loop_unlock");
  pa_threaded_mainloop_unlock(id->pulse_mainloop);

  id->pulse_do_trigger = !!*length;
  SHOW("_pulse_free: %d (ret)\n", (int)*length);
  return ret;
}

static int 
_pulse_playing(AudioID *id, const pa_timing_info *the_timing_info) 
{
  ENTER(__FUNCTION__);
  assert(id);

  int r = 0;
  const pa_timing_info *i;

  assert(the_timing_info);

  CHECK_CONNECTED(id,0);
    
  pa_threaded_mainloop_lock(id->pulse_mainloop);

  for (;;) {
    CHECK_DEAD_GOTO(id,fail, 1);

    if ((i = pa_stream_get_timing_info(id->pulse_stream))) {
      break;        
    }
    if (pa_context_errno(id->pulse_context) != PA_ERR_NODATA) {
      ERR("pa_stream_get_timing_info() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
      goto fail;
    }

    pa_threaded_mainloop_wait(id->pulse_mainloop);
  }

  r = i->playing;
  memcpy((void*)the_timing_info, (void*)i, sizeof(pa_timing_info));

 fail:
  pa_threaded_mainloop_unlock(id->pulse_mainloop);

  return r;
}

static int
_pulse_write(AudioID *id, void* ptr, int length) 
{
  int r;
  ENTER(__FUNCTION__);

  int ret = PULSE_ERROR;
  assert(id);

  SHOW("_pulse_write > length=%d\n", length);

  CHECK_CONNECTED(id, PULSE_ERROR);

  pa_threaded_mainloop_lock(id->pulse_mainloop);
  CHECK_DEAD_GOTO(id, fail, 1);


  // TODO: Problem is if PulseAudio daemon gets suspended
  // it won't resume playing again
  r = pa_stream_write(id->pulse_stream, ptr, length, NULL, PA_SEEK_RELATIVE, (pa_seek_mode_t) 0);
  SHOW("pa_stream_write returned: %d", r);

  if (r<0){
    ERR("pa_stream_write() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    ret = PULSE_ERROR;
    goto fail;
  }
    
  id->pulse_do_trigger = 0;
  ret = PULSE_OK;

 fail:    
  pa_threaded_mainloop_unlock(id->pulse_mainloop);
  return ret;
}

static int
_drain(AudioID *id) 
{
  pa_operation *o = NULL;
  int ret = PULSE_ERROR;
  assert(id);

  ENTER(__FUNCTION__);

  CHECK_CONNECTED(id, PULSE_ERROR);

  pa_threaded_mainloop_lock(id->pulse_mainloop);
  CHECK_DEAD_GOTO(id, fail, 1); /* TBD 0 instead? */

  SHOW("Draining...\n","");
  DISPLAY_ID(id, "pa_threaded_mainloop_lock");
  
  id->pulse_success = 0;
  
  SHOW_TIME("pa_stream_drain (call)");
  if (!(o = pa_stream_drain(id->pulse_stream, _stream_success_cb, id))) {
    ERR("pa_stream_drain() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    goto fail;
  }
    
  SHOW_TIME("pa_threaded_mainloop_wait (call)");
  while (pa_operation_get_state(o) != PA_OPERATION_DONE) {
    CHECK_DEAD_GOTO(id,fail, 1);
    pa_threaded_mainloop_wait(id->pulse_mainloop);
  }
  SHOW_TIME("pa_threaded_mainloop_wait (ret)");

  if (id->pulse_success == 0) {
    ERR("pa_stream_drain() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
  } 
  else {
    ret = PULSE_OK;
  }

 fail:
  SHOW_TIME("pa_operation_unref (call)");
  if (o)
    pa_operation_unref(o);

  pa_threaded_mainloop_unlock(id->pulse_mainloop);

  SHOW_TIME("_drain (ret)");

  return ret;
}


static void 
_pulse_close(AudioID *id) 
{
  ENTER(__FUNCTION__);
    
  assert(id);

  if (_drain(id) == 0) {
    CHECK_CONNECTED_NO_RETVAL(id);    
    id->pulse_connected = 0;

    if (id->pulse_mainloop)
      pa_threaded_mainloop_stop(id->pulse_mainloop);

    if (id->pulse_stream) {
      SHOW_TIME("pa_stream_disconnect (call)");
      pa_stream_disconnect(id->pulse_stream);
      pa_stream_unref(id->pulse_stream);
      id->pulse_stream = NULL;
    }

    if (id->pulse_context) {
      SHOW_TIME("pa_context_disconnect (call)");
      pa_context_disconnect(id->pulse_context);
      pa_context_unref(id->pulse_context);
      id->pulse_context = NULL;
    }
  
    if (id->pulse_mainloop) {
      SHOW_TIME("pa_threaded_mainloop_free (call)");
      pa_threaded_mainloop_free(id->pulse_mainloop);
      id->pulse_mainloop = NULL;
    }
  
    id->pulse_volume_time_event = NULL;

    if (id->pulse_server)
      {
	free(id->pulse_server);
	id->pulse_server = NULL;
      }
  }else{
    ERR("_pulse_close: error (_drain)\n","");
  }

  SHOW_TIME("_pulse_close (ret)");
}

static int
_pulse_get_sample(const AudioTrack* track, pa_sample_spec* ss)
{
  ENTER(__FUNCTION__);

  assert(track && ss);

  /* Choose the correct format */
  if (track->bits == 16){
    ss->format = PA_SAMPLE_S16LE;
  }else if (track->bits == 8){
    /* TBD: PA_SAMPLE_ALAW, PA_SAMPLE_ULAW ? */
    ss->format = PA_SAMPLE_U8; 
  }else{
    ERR("Unsupported sound data format, track.bits = %d\n", track->bits);
    return PULSE_ERROR;
  }

  ss->rate = track->sample_rate;
  ss->channels = track->num_channels;

  SHOW("Setting sample spec to format=%s, rate=%i, count=%i\n", 
       pa_sample_format_to_string(ss->format),
       ss->rate, 
       ss->channels);

  if (!pa_sample_spec_valid(ss)) {      
    ERR("Sample spec not valid!\n","");
    return PULSE_ERROR;
  }

  SHOW("Sample spec valid\n","");
  return PULSE_OK;
}


/* return PULSE_OK, PULSE_ERROR, PULSE_NO_CONNECTION */
static int 
_pulse_open(AudioID *id, pa_sample_spec* ss) 
{
  ENTER(__FUNCTION__);
  
  pa_operation *o = NULL;
  int ret = PULSE_ERROR;

  assert(id && ss);

  assert(!id->pulse_mainloop);
  assert(!id->pulse_context);
  assert(!id->pulse_stream);
  assert(!id->pulse_connected);

  if (!id->pulse_volume_valid) {
    pa_cvolume_reset(&id->pulse_volume, ss->channels);
    id->pulse_volume_valid = 1;
  } else if (id->pulse_volume.channels != ss->channels) 
    pa_cvolume_set(&id->pulse_volume, ss->channels, pa_cvolume_avg(&id->pulse_volume));

  SHOW_TIME("pa_threaded_mainloop_new (call)");
  if (!(id->pulse_mainloop = pa_threaded_mainloop_new())) {
    ERR("Failed to allocate main loop","");
    goto fail;
  }

  pa_threaded_mainloop_lock(id->pulse_mainloop);

  SHOW_TIME("pa_context_new (call)");
  if (!(id->pulse_context = pa_context_new(pa_threaded_mainloop_get_api(id->pulse_mainloop), "speech-dispatcher"))) {
    ERR("Failed to allocate context","");
    goto unlock_and_fail;
  }

  pa_context_set_state_callback(id->pulse_context, _context_state_cb, id);
  pa_context_set_subscribe_callback(id->pulse_context, _subscribe_cb, id);

  SHOW_TIME("pa_context_connect (call)");
  if (pa_context_connect(id->pulse_context, id->pulse_server, (pa_context_flags_t)0, NULL) < 0) {
    ERR("Failed to connect to server: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    ret = PULSE_NO_CONNECTION;
    goto unlock_and_fail;
  }

  SHOW_TIME("pa_threaded_mainloop_start (call)");
  if (pa_threaded_mainloop_start(id->pulse_mainloop) < 0) {
    ERR("Failed to start main loop","");
    goto unlock_and_fail;
  }

  /* Wait until the context is ready */
  SHOW_TIME("pa_threaded_mainloop_wait");
  pa_threaded_mainloop_wait(id->pulse_mainloop);

  if (pa_context_get_state(id->pulse_context) != PA_CONTEXT_READY) {
    ERR("Failed to connect to server: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    ret = PULSE_NO_CONNECTION;
    if (id->pulse_mainloop)
      pa_threaded_mainloop_stop(id->pulse_mainloop);
    goto unlock_and_fail;
  }

  SHOW_TIME("pa_stream_new");
  if (!(id->pulse_stream = pa_stream_new(id->pulse_context, "unknown", ss, NULL))) {
    ERR("Failed to create stream: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    goto unlock_and_fail;
  }

  pa_stream_set_state_callback(id->pulse_stream, _stream_state_cb, id);
  pa_stream_set_write_callback(id->pulse_stream, _stream_request_cb, id);
  pa_stream_set_latency_update_callback(id->pulse_stream, _stream_latency_update_cb, id);

  pa_buffer_attr a_attr;
  a_attr.maxlength = id->pulse_max_length;
  a_attr.tlength = id->pulse_target_length;
  a_attr.prebuf = id->pulse_pre_buffering;
  a_attr.minreq = id->pulse_min_request;
  a_attr.fragsize = 0;

  SHOW("attr: maxlength=%i, tlength=%i, prebuf=%i, minreq=%i, fragsize=%i\n",
       a_attr.maxlength,
       a_attr.tlength,
       a_attr.prebuf,
       a_attr.minreq,
       a_attr.fragsize);


  SHOW_TIME("pa_connect_playback");
  if (pa_stream_connect_playback(id->pulse_stream, NULL, &a_attr,
				 (pa_stream_flags_t)(PA_STREAM_INTERPOLATE_TIMING|PA_STREAM_AUTO_TIMING_UPDATE|PA_STREAM_ADJUST_LATENCY),
				 &id->pulse_volume, NULL) < 0) {
    ERR("Failed to connect stream: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    goto unlock_and_fail;
  }

  /* Wait until the stream is ready */
  SHOW_TIME("pa_threaded_mainloop_wait");
  pa_threaded_mainloop_wait(id->pulse_mainloop);

  if (pa_stream_get_state(id->pulse_stream) != PA_STREAM_READY) {
    ERR("Failed to connect stream: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    goto unlock_and_fail;
  }

  /* Now subscribe to events */
  SHOW_TIME("pa_context_subscribe");
  if (!(o = pa_context_subscribe(id->pulse_context, PA_SUBSCRIPTION_MASK_SINK_INPUT, _context_success_cb, id))) {
    ERR("pa_context_subscribe() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    goto unlock_and_fail;
  }
    
  id->pulse_success = 0;
  SHOW_TIME("pa_threaded_mainloop_wait");
  while (pa_operation_get_state(o) != PA_OPERATION_DONE) {
    CHECK_DEAD_GOTO(id,fail, 1);
    pa_threaded_mainloop_wait(id->pulse_mainloop);
  }

  if (id->pulse_success == 0) {
    ERR("pa_context_subscribe() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    goto unlock_and_fail;
  }

  pa_operation_unref(o);

  /* Now request the initial stream info */
  if (!(o = pa_context_get_sink_input_info(id->pulse_context, pa_stream_get_index(id->pulse_stream), _info_cb, id))) {
    ERR("pa_context_get_sink_input_info() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    goto unlock_and_fail;
  }
    
  SHOW_TIME("pa_threaded_mainloop_wait 2");
  while (pa_operation_get_state(o) != PA_OPERATION_DONE) {
    CHECK_DEAD_GOTO(id, fail, 1);
    pa_threaded_mainloop_wait(id->pulse_mainloop);
  }

  /*     if (!id->pulse_volume_valid) { */
  /*         SHOW("pa_context_get_sink_input_info() failed: %s", pa_strerror(pa_context_errno(context))); */
  /*         goto unlock_and_fail; */
  /*     } */

  id->pulse_do_trigger = 0;
  id->pulse_time_offset_msec = 0;
  id->pulse_just_flushed = 0;
  id->pulse_connected = 1;
  id->pulse_volume_time_event = NULL;
    
  pa_threaded_mainloop_unlock(id->pulse_mainloop);

  SHOW("_pulse_open (ret true)\n","");

  return PULSE_OK;

 unlock_and_fail:

  if (o)
    pa_operation_unref(o);
    
  pa_threaded_mainloop_unlock(id->pulse_mainloop);
    
 fail:

  if (ret == PULSE_NO_CONNECTION) {
    if (id->pulse_context) {
      SHOW_TIME("pa_context_disconnect (call)");
      pa_context_disconnect(id->pulse_context);
      pa_context_unref(id->pulse_context);
      id->pulse_context = NULL;
    }
  
    if (id->pulse_mainloop) {
      SHOW_TIME("pa_threaded_mainloop_free (call)");
      pa_threaded_mainloop_free(id->pulse_mainloop);
      id->pulse_mainloop = NULL;
    }
  
    if (id->pulse_server)
      {
	free(id->pulse_server);
	id->pulse_server = NULL;
      }
  } 
  else {
    _pulse_close(id);
  }

  id->suspended = 0;

  SHOW("_pulse_open (ret %d)\n", ret);
    
  return ret;
}

static int 
_pulse_set_sample(AudioID *id, AudioTrack track) 
{
  ENTER(__FUNCTION__);
  pa_sample_spec ss;
  pa_operation *o = NULL;
  int ret = PULSE_ERROR;

  assert(id);

  /*   assert(!id->pulse_mainloop); */
  /*   assert(!id->pulse_context); */
  /*   assert(!id->pulse_stream); */
  /*   assert(!id->pulse_connected); */

  /* Choose the correct format */
  if (track.bits == 16){
    ss.format = PA_SAMPLE_S16LE;
  }else if (track.bits == 8){
    /* TBD: PA_SAMPLE_ALAW, PA_SAMPLE_ULAW ? */
    ss.format = PA_SAMPLE_U8; 
  }else{
    ERR("Unsupported sound data format, track.bits = %d\n", track.bits);
    return PULSE_ERROR;
  }

  ss.rate = track.sample_rate;
  ss.channels = track.num_channels;

  SHOW("Setting sample spec to format=%s, rate=%i, count=%i\n", 
       pa_sample_format_to_string(ss.format),
       ss.rate, 
       ss.channels);

  if (!pa_sample_spec_valid(&ss)) {      
    ERR("Sample spec not valid!\n","");
    return PULSE_ERROR;
  }

  SHOW("Sample spec valid\n","");

  if (!id->pulse_volume_valid) { 
    pa_cvolume_reset(&id->pulse_volume, ss.channels);
    id->pulse_volume_valid = 1;
  } else if (id->pulse_volume.channels != ss.channels)
    pa_cvolume_set(&id->pulse_volume, ss.channels, pa_cvolume_avg(&id->pulse_volume));
  
  SHOW_TIME("pa_threaded_mainloop_new (call)");
  if (!(id->pulse_mainloop = pa_threaded_mainloop_new())) {
    ERR("Failed to allocate main loop","");
    goto fail;
  }

  pa_threaded_mainloop_lock(id->pulse_mainloop);

  SHOW_TIME("pa_context_new (call)");
  if (!(id->pulse_context = pa_context_new(pa_threaded_mainloop_get_api(id->pulse_mainloop), "speech-dispatcher"))) {
    ERR("Failed to allocate context","");
    goto unlock_and_fail;
  }

  pa_context_set_state_callback(id->pulse_context, _context_state_cb, id);
  pa_context_set_subscribe_callback(id->pulse_context, _subscribe_cb, id);

  SHOW_TIME("pa_context_connect (call)");
  if (pa_context_connect(id->pulse_context, id->pulse_server, (pa_context_flags_t)0, NULL) < 0) {
    ERR("Failed to connect to server: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    ret = PULSE_NO_CONNECTION;
    goto unlock_and_fail;
  }

  SHOW_TIME("pa_threaded_mainloop_start (call)");
  if (pa_threaded_mainloop_start(id->pulse_mainloop) < 0) {
    ERR("Failed to start main loop","");
    goto unlock_and_fail;
  }

  /* Wait until the context is ready */
  SHOW_TIME("pa_threaded_mainloop_wait");
  pa_threaded_mainloop_wait(id->pulse_mainloop);

  if (pa_context_get_state(id->pulse_context) != PA_CONTEXT_READY) {
    ERR("Failed to connect to server: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    ret = PULSE_NO_CONNECTION;
    if (id->pulse_mainloop)
      pa_threaded_mainloop_stop(id->pulse_mainloop);
    goto unlock_and_fail;
  }

  SHOW_TIME("pa_stream_new");
  if (!(id->pulse_stream = pa_stream_new(id->pulse_context, "unknown", &ss, NULL))) {
    ERR("Failed to create stream: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    goto unlock_and_fail;
  }

  pa_stream_set_state_callback(id->pulse_stream, _stream_state_cb, id);
  pa_stream_set_write_callback(id->pulse_stream, _stream_request_cb, id);
  pa_stream_set_latency_update_callback(id->pulse_stream, _stream_latency_update_cb, id);

  pa_buffer_attr a_attr;
  a_attr.maxlength = id->pulse_max_length;
  a_attr.tlength = id->pulse_target_length;
  a_attr.prebuf = id->pulse_pre_buffering;
  a_attr.minreq = id->pulse_min_request;
  a_attr.fragsize = 0;

  SHOW("attr: maxlength=%i, tlength=%i, prebuf=%i, minreq=%i, fragsize=%i\n",
       a_attr.maxlength,
       a_attr.tlength,
       a_attr.prebuf,
       a_attr.minreq,
       a_attr.fragsize);

  SHOW_TIME("pa_connect_playback");
  if (pa_stream_connect_playback(id->pulse_stream, NULL, &a_attr, (pa_stream_flags_t)(PA_STREAM_INTERPOLATE_TIMING|PA_STREAM_AUTO_TIMING_UPDATE), &id->pulse_volume, NULL) < 0) {
    ERR("Failed to connect stream: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    goto unlock_and_fail;
  }

  /* Wait until the stream is ready */
  SHOW_TIME("pa_threaded_mainloop_wait");
  pa_threaded_mainloop_wait(id->pulse_mainloop);

  if (pa_stream_get_state(id->pulse_stream) != PA_STREAM_READY) {
    ERR("Failed to connect stream: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    goto unlock_and_fail;
  }

  /* Now subscribe to events */
  SHOW_TIME("pa_context_subscribe");
  if (!(o = pa_context_subscribe(id->pulse_context, PA_SUBSCRIPTION_MASK_SINK_INPUT, _context_success_cb, id))) {
    ERR("pa_context_subscribe() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    goto unlock_and_fail;
  }
    
  id->pulse_success = 0;
  SHOW_TIME("pa_threaded_mainloop_wait");
  while (pa_operation_get_state(o) != PA_OPERATION_DONE) {
    CHECK_DEAD_GOTO(id,fail, 1);
    pa_threaded_mainloop_wait(id->pulse_mainloop);
  }

  if (id->pulse_success == 0) {
    ERR("pa_context_subscribe() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    goto unlock_and_fail;
  }

  pa_operation_unref(o);

  /* Now request the initial stream info */
  if (!(o = pa_context_get_sink_input_info(id->pulse_context, pa_stream_get_index(id->pulse_stream), _info_cb, id))) {
    ERR("pa_context_get_sink_input_info() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    goto unlock_and_fail;
  }
    
  SHOW_TIME("pa_threaded_mainloop_wait 2");
  while (pa_operation_get_state(o) != PA_OPERATION_DONE) {
    CHECK_DEAD_GOTO(id, fail, 1);
    pa_threaded_mainloop_wait(id->pulse_mainloop);
  }

  /*     if (!id->pulse_volume_valid) { */
  /*         SHOW("pa_context_get_sink_input_info() failed: %s", pa_strerror(pa_context_errno(context))); */
  /*         goto unlock_and_fail; */
  /*     } */

  id->pulse_do_trigger = 0;
  id->pulse_time_offset_msec = 0;
  id->pulse_just_flushed = 0;
  id->pulse_connected = 1;
  id->pulse_volume_time_event = NULL;
    
  pa_threaded_mainloop_unlock(id->pulse_mainloop);

  SHOW("_pulse_set_sample (ret true)\n","");

  return PULSE_OK;

 unlock_and_fail:

  if (o)
    pa_operation_unref(o);
    
  pa_threaded_mainloop_unlock(id->pulse_mainloop);
    
 fail:
  if (ret == PULSE_NO_CONNECTION) {
    if (id->pulse_context) {
      SHOW_TIME("pa_context_disconnect (call)");
      pa_context_disconnect(id->pulse_context);
      pa_context_unref(id->pulse_context);
      id->pulse_context = NULL;
    }
  
    if (id->pulse_mainloop) {
      SHOW_TIME("pa_threaded_mainloop_free (call)");
      pa_threaded_mainloop_free(id->pulse_mainloop);
      id->pulse_mainloop = NULL;
    }
  
    if (id->pulse_server)
      {
	free(id->pulse_server);
	id->pulse_server = NULL;
      }
  } 
  else {
    _pulse_close(id);
  }

  SHOW("_pulse_set_sample (ret false)\n","");
    
  return ret;
}


/* _pulse_timeout_thread
   Close the PulseAudio connection after a while of inactivaty (PULSE_INFINITE_TIMEOUT_IN_SEC).

   The timeout is started by pulse_play as soon as the stream has been played:
   pulse_play indicates the current time in time_start and increments the semaphore.

   If pulse_play is called again, it simply resets the time_start value.
   
   The semaphore incremented by pulse_play wakes up _pulse_timeout_thread which then 
   stores the time_start value and waits for the next step (timeout occurence or semaphore).

   If timeout occurs, the time_start value is compared to the previous one. 
   If equal, _pulse_close is called. Otherwise, the timeout is ignored since pulse_play has 
   been called again.
*/
static void* 
_pulse_timeout_thread(void* the_id)
{
  ENTER(__FUNCTION__);

  struct timespec ts;
  struct timeval tv;	
  int time_ref;	
  AudioID * id = (AudioID *)the_id;
  int a_start_is_required=0;
  t_pulse_timeout* a_timeout = &(id->pulse_timeout);

  assert (gettimeofday(&tv, NULL) != -1);
  ts.tv_sec = tv.tv_sec;
  ts.tv_nsec = tv.tv_usec*1000;

  a_timeout->my_close_is_imminent = 0;

  int i=0;
  while(1) {
    int err = PULSE_OK;
    
    SHOW_TIME("sem_wait/sem_timedwait");

    if (a_timeout->my_close_is_imminent) {
      while ((err = sem_timedwait(&(a_timeout->my_sem), &ts)) == -1 
	     && errno == EINTR) {
	continue;
      }
      SHOW_TIME("sem_timedwait (ret)");
    } 
    else {
      SHOW("%s > wait for a request\n", __FUNCTION__);    
      while ((err = sem_wait(&(a_timeout->my_sem))) == -1 
	     && errno == EINTR) {
	continue;
      }
      SHOW_TIME("sem_wait (ret)");
    }

    time_ref = a_timeout->my_time_start;

    assert (gettimeofday(&tv, NULL) != -1);

    if (err == PULSE_OK) { /* got semaphore */
      if (time_ref) {
	SHOW("%s > compute timeout (my_time_start=%d)\n", __FUNCTION__, time_ref);
	int a_usec = tv.tv_usec + PULSE_TIMEOUT_IN_USEC;

	tv.tv_sec += a_usec/1000000;
	tv.tv_usec = a_usec % 1000000;

	ts.tv_sec = tv.tv_sec;	  
	ts.tv_nsec = tv.tv_usec*1000;

	a_timeout->my_close_is_imminent = 1;
	a_timeout->my_time_start_old = time_ref;
      }
      else {
	SHOW("%s > time_ref=0!\n", __FUNCTION__);
	a_timeout->my_close_is_imminent = 0;
      }
    }
    else if (a_timeout->my_close_is_imminent) { /* timeout */
      if (a_timeout->my_time_start_old == time_ref) {	
	SHOW("%s > timeout!\n", __FUNCTION__);
	int a_status;
	pthread_mutex_t* a_mutex = &id->pulse_mutex;

	if ((a_status=pthread_mutex_lock(a_mutex))) {
	  ERR("Error: pulse_mutex lock=%d (%s)\n", a_status, __FUNCTION__);
	} 
	else {
	  //SHOW_TIME("Suspending all sinks");
	  //pa_context_suspend_sink_by_index(id->pulse_context,PA_INVALID_INDEX,1,NULL,NULL);
	  id->suspended = 1;
	  
	  // Do not close the device, just suspend the sinks
	  //_pulse_close(id);
	  pa_stream_cork(id->pulse_stream,1,NULL,NULL);

	  pthread_mutex_unlock(a_mutex);
	}
      } else {
	SHOW("%s > my_time_start has changed (old=%d, current=%d)\n", __FUNCTION__, a_timeout->my_time_start_old, time_ref);
      }
      a_timeout->my_close_is_imminent = 0;
    }
  }

  SHOW_TIME("_pulse_timeout (ret)");
  
  return NULL;
}

/* start timeout */
static void 
_pulse_timeout_start(AudioID *id)
{
  ENTER(__FUNCTION__);

  assert(id);
  t_pulse_timeout* a_timeout = &(id->pulse_timeout);

  struct timeval tv;	

  assert (gettimeofday(&tv, NULL) != -1);
  
  /* convert the time in ms.
     the limit (currently 5s) must be higher than PULSE_TIMEOUT_IN_USEC! */
  a_timeout->my_time_start = tv.tv_usec/1000 + 1000*(tv.tv_sec%4);
  SHOW("my_time_start=%d\n", a_timeout->my_time_start);

  assert( sem_post(&(a_timeout->my_sem)) != -1);
}

/* stop timeout */
static void 
_pulse_timeout_stop(AudioID *id)
{
  ENTER(__FUNCTION__);

  assert(id);
  t_pulse_timeout* a_timeout = &(id->pulse_timeout);
  a_timeout->my_time_start = 0;
  SHOW("my_time_start=%d\n", a_timeout->my_time_start);
}



/* external services */


/* Open PulseaAudio for playback.
  
   These parameters are passed in pars:
   (char*) pars[0] ... null-terminated string containing the name
   of the PulseAudio server (or "default")
   (void*) pars[1] ... Maximum length of the buffer
   (void*) pars[2] ... Target length of the buffer
   (void*) pars[3] ... Pre-buffering
   (void*) pars[4] ... Minimum request
   (void*) pars[5] ... =NULL
*/

int
pulse_open(AudioID *id, void **pars) 
{
  ENTER(__FUNCTION__);
  int ret, err = 0, state;

  if (id == NULL){
    ERR("Can't open PulseAudio sound output, invalid AudioID structure.\n","");
    return PULSE_ERROR;
  }

  if (pars[0] == NULL){
    ERR("Can't open PulseAudio sound output, missing parameters in argument.\n","");
    return PULSE_ERROR;
  }
    
  pthread_mutex_init( &id->pulse_mutex, (const pthread_mutexattr_t *)NULL);

  assert(-1 != sem_init(&(id->pulse_timeout.my_sem), 0, 0));
  pthread_attr_t a_attrib;    
  if (pthread_attr_init (& a_attrib)
      || pthread_attr_setdetachstate(&a_attrib, PTHREAD_CREATE_JOINABLE)
      || pthread_create( &(id->pulse_timeout.my_thread), 
			 & a_attrib, 
			 _pulse_timeout_thread, 
			 (void*)id))
    {
      assert(0);
    }
  pthread_attr_destroy(&a_attrib);

  id->pulse_context = NULL;
  id->pulse_stream = NULL;
  id->pulse_mainloop = NULL;
  id->pulse_volume_valid = 0;
  id->pulse_do_trigger = 0;
  id->pulse_time_offset_msec = 0;
  id->pulse_just_flushed = 0;
  id->pulse_connected = 0;
  id->pulse_success = 0;
  id->pulse_volume_time_event = NULL;
  id->pulse_stop_required = 0;

  if (strcmp(pars[0], "default") != 0) {
    id->pulse_server = strdup(pars[0]);
  }
  else {
    id->pulse_server = NULL;
  }
  
  /* The following cast is verified correct despite
   gcc warnings */
  id->pulse_max_length = (int) pars[1];
  id->pulse_target_length = (int) pars[2];
  id->pulse_pre_buffering = (int) pars[3];
  id->pulse_min_request = (int) pars[4];

  /* Check if the server is running */
  id->pulse_mainloop = pa_mainloop_new();
  if (id->pulse_mainloop == NULL) {
    ERR("Failed to allocate main loop","");
    ret = PULSE_ERROR;
    goto fail;
  }

  id->pulse_context = pa_context_new(pa_mainloop_get_api(id->pulse_mainloop), "speech-dispatcher");
  if (id->pulse_context == NULL) {
    ERR("Failed to allocate context","");
    ret = PULSE_ERROR;
    goto fail;
  }

  err = pa_context_connect(id->pulse_context, id->pulse_server, 0, NULL);
  if (err < 0) {
    ERR("Failed to connect to server: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    ret = PULSE_ERROR;
    goto fail;
  }

  do {
    err = pa_mainloop_prepare(id->pulse_mainloop, -1);
    if (err < 0) {
      ret = PULSE_ERROR;
      goto fail;
    }

    err = pa_mainloop_poll(id->pulse_mainloop);
    if (err < 0) {
      ret = PULSE_ERROR;
      goto fail;
    }

    err = pa_mainloop_dispatch(id->pulse_mainloop);
    if (err < 0) {
      ret = PULSE_ERROR;
      goto fail;
    }

    state = pa_context_get_state(id->pulse_context);
  } while (state < PA_CONTEXT_AUTHORIZING);

  if (state > PA_CONTEXT_READY) {
    ret = PULSE_ERROR;
    goto fail;
  }

  SHOW("PulseAudio sound output opened\n","");

  ret = PULSE_OK;

  fail:
    if (ret != PULSE_OK)
      ERR("Failed to connect to pulse server.");

    if (id->pulse_context != NULL) {
      pa_context_disconnect(id->pulse_context);
      pa_context_unref(id->pulse_context);
      id->pulse_context = NULL;
    }
	
    if (id->pulse_mainloop != NULL) {
      pa_mainloop_free(id->pulse_mainloop);
      id->pulse_mainloop = NULL;
    }

    return ret;
}

int
pulse_play(AudioID *id, AudioTrack track)
{
  ENTER(__FUNCTION__);

  int bytes_per_sample;
  int num_bytes;
  int ret = PULSE_OK;
  size_t a_total_free_mem;
  pa_sample_spec ss;

  if (id == NULL){
    ERR("Invalid device passed to %s()\n",__FUNCTION__);
    return PULSE_ERROR;
  }

  _pulse_timeout_stop(id);

  /* Is it not an empty track? */
  /* Passing an empty track is not an error */
  if (track.samples == NULL) {
    ERR("Empty track!\n","");
    return PULSE_OK;
  }

  if (_pulse_get_sample(&track, &ss) == PULSE_ERROR) {
    ERR("Erroneous track!\n","");
    return PULSE_ERROR;
  }

  int a_status = pthread_mutex_lock(&id->pulse_mutex);
  if (a_status) {
    ERR("Error: pulse_mutex lock=%d (%s)\n", a_status, __FUNCTION__);
    return PULSE_ERROR;
    }

  if (id->pulse_mainloop) {
    if (id->pulse_stop_required) {
      ret = PULSE_OK;
      goto terminate;
    }

    /*  if the sample spec changes, the stream will be rebuilt. */
    const pa_sample_spec *ss2 = pa_stream_get_sample_spec (id->pulse_stream);

    if (!pa_sample_spec_equal (&ss, ss2)) {
      SHOW_TIME("Sound specs changed, closing the device to reopen.");
      _pulse_close(id);
    }
  }

  if (!id->pulse_mainloop) {
    if (id->pulse_stop_required) {
      ret = PULSE_OK;
      goto terminate;
    }
    ret = _pulse_open(id, &ss);
    if (ret != PULSE_OK) {
      goto terminate;
    }
  }

  SHOW_TIME("Resume sinks if suspended");
  if (id->suspended){
    SHOW_TIME("Resuming sinks");
    //pa_context_suspend_sink_by_index(id->pulse_context,PA_INVALID_INDEX,0,NULL,NULL);
    pa_stream_cork(id->pulse_stream,0,NULL,NULL);
  }

  SHOW("Checking buffer size\n","");
  if (track.bits == 16){
    bytes_per_sample = 2;
  }else if (track.bits == 8){
    bytes_per_sample = 1;    
  }else{
    ERR("Unsupported sound data format, track.bits = %d\n", track.bits);
    return PULSE_ERROR;
  }

  /* Loop until all samples are played on the device. */

  signed short* output_samples = track.samples;
  num_bytes = track.num_samples*bytes_per_sample;
  
  SHOW("track.samples=%d, track.num_samples=%d, bytes_per_sample=%d\n", track.samples, track.num_samples, bytes_per_sample);

  DISPLAY_ID(id, "pulse_play");

  while(!id->pulse_stop_required && (num_bytes > 0)) {
    SHOW("Still %d bytes left to be played\n", num_bytes);

    ret = _pulse_free(id, &a_total_free_mem);
    if (ret) {
      goto terminate;
    }

    if (a_total_free_mem >= num_bytes) {
      SHOW("a_total_free_mem(%d) >= num_bytes(%d)\n", a_total_free_mem, num_bytes);
      ret = _pulse_write(id, output_samples, num_bytes);
      num_bytes = 0;
    }
    else if (a_total_free_mem > 500) {
      /* 500: threshold for avoiding too many calls to pulse_write */
      SHOW("a_total_free_mem(%d) < num_bytes(%d)\n", a_total_free_mem, num_bytes);
      ret = _pulse_write(id, output_samples, a_total_free_mem);
      num_bytes -= a_total_free_mem;
      output_samples += a_total_free_mem/2;
    }
    if (ret) {
      goto terminate;
    }
    usleep(10000);
  }

  SHOW_TIME("pulse_play > _drain");
  _drain(id);

  _pulse_timeout_start(id);

 terminate:
  pthread_mutex_unlock(&id->pulse_mutex);
  SHOW("pulse_play: ret=%d", ret);
  SHOW_TIME("pulse_play (ret)");
  return ret;
}

int
pulse_stop(AudioID *id)
{
  ENTER(__FUNCTION__);

  if (id == NULL){
    ERR("Invalid device passed to %s\n",__FUNCTION__);
    return PULSE_ERROR;
  }

  id->pulse_stop_required = 1;
  int a_status = pthread_mutex_lock(&id->pulse_mutex);
  if (a_status) {
    id->pulse_stop_required = 0;
    ERR("Error: pulse_mutex lock=%d (%s)\n", a_status, __FUNCTION__);
    return PULSE_ERROR;
    }

  _drain(id);

  id->pulse_stop_required = 0;
  a_status = pthread_mutex_unlock(&id->pulse_mutex);
  SHOW_TIME("pulse_stop (ret)");

  return PULSE_OK;
}

int
pulse_close(AudioID *id)
{   
  ENTER(__FUNCTION__);

  int a_status;
  pthread_mutex_t* a_mutex = NULL;

  if (id == NULL){
    ERR("Invalid device passed to %s\n",__FUNCTION__);
    return PULSE_ERROR;
  }

  a_mutex = &id->pulse_mutex;
  a_status = pthread_mutex_lock(a_mutex);

  if (a_status) {
    ERR("Error: pulse_mutex lock=%d (%s)\n", a_status, __FUNCTION__);
    return PULSE_ERROR;
    }

  _pulse_close(id);

  SHOW_TIME("timeout thread cancel");
  pthread_cancel(id->pulse_timeout.my_thread);
  pthread_join(id->pulse_timeout.my_thread, NULL);
  sem_destroy(&(id->pulse_timeout.my_sem));

  SHOW_TIME("unlock mutex");
  a_status = pthread_mutex_unlock(a_mutex);
  pthread_mutex_destroy(a_mutex);

  return PULSE_OK;
}

int
pulse_set_volume(AudioID*id, int volume)
{
  ENTER(__FUNCTION__);

  SHOW("volume=%d\n", volume);

  if ((volume > 100) || (volume < -100)){
    ERR("Requested volume out of range (%d)", volume);
    return PULSE_ERROR;
  }

  if (id->pulse_connected) {
    pa_threaded_mainloop_lock(id->pulse_mainloop);
    CHECK_DEAD_GOTO(id, fail, 1);
  }
  
  if (!id->pulse_volume_valid || id->pulse_volume.channels !=  1) {
    id->pulse_volume.values[0] = id->pulse_volume.values[1] = ((pa_volume_t) (volume + 100) * PA_VOLUME_NORM)/200;
    id->pulse_volume.channels = 2;
  } else {
    id->pulse_volume.values[0] = ((pa_volume_t) (volume + 100) * PA_VOLUME_NORM)/200;
    id->pulse_volume.channels = 1;
  }
  
  id->pulse_volume_valid = 1;
  
  if (id->pulse_connected && !id->pulse_volume_time_event) {
    struct timeval tv;
    pa_mainloop_api *api = pa_threaded_mainloop_get_api(id->pulse_mainloop);
    id->pulse_volume_time_event = api->time_new(api, pa_timeval_add(pa_gettimeofday(&tv), 100000), _volume_time_cb, id);
  }
  
 fail:
  if (id->pulse_connected)
    pa_threaded_mainloop_unlock(id->pulse_mainloop);
    
  return PULSE_OK;
}

/* Provide the PulseAudio backend */
AudioFunctions pulse_functions = {pulse_open, pulse_play, pulse_stop, pulse_close, pulse_set_volume};
