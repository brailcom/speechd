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
 * $Id: pulse.c,v 1.1 2007-11-11 21:59:01 gcasse Exp $
 */


/* debug */

//#define DEBUG_PULSE
#include <stdio.h>
#include <stdarg.h>

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
  debug_show("%s >\n id=0x%x\n pulse_context=0x%x\n pulse_stream=0x%x\n pulse_mainloop=0x%x\n pulse_volume_valid=0x%x\n pulse_do_trigger=0x%x\n pulse_time_offset_msec=0x%x\n pulse_just_flushed=0x%x\n pulse_connected=0x%x\n pulse_success=0x%x, pulse_drained=0x%x\n",
	     id, 
	     id->pulse_context,
	     id->pulse_stream,
	     id->pulse_mainloop, 
	     id->pulse_volume_valid,
	     id->pulse_do_trigger,
	     id->pulse_time_offset_msec,
	     id->pulse_just_flushed,
	     id->pulse_connected,
	     id->pulse_success,
	     id->pulse_drained);
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

  if (!(o = pa_context_get_sink_input_info(c, index, _info_cb, NULL))) {
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
  //  ENTER(__FUNCTION__);
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
  int ret = -1;
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

  ret = 0;

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


// static void pulse_flush(int time) {
//   ENTER(__FUNCTION__);
//     pa_operation *o = NULL;
//     int success = 0;

//     CHECK_CONNECTED(id,);

//     pa_threaded_mainloop_lock(id->pulse_mainloop);
//     CHECK_DEAD_GOTO(id,fail, 1);

//     if (!(o = pa_stream_flush(id->pulse_stream, _stream_success_cb, &success))) {
//         ERR("pa_stream_flush() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
//         goto fail;
//     }
    
//     while (pa_operation_get_state(o) != PA_OPERATION_DONE) {
//         CHECK_DEAD_GOTO(id,fail, 1);
//         pa_threaded_mainloop_wait(id->pulse_mainloop);
//     }

//     if (!success)
//         ERR("pa_stream_flush() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    
//     written = (uint64_t) (((double) time * pa_bytes_per_second(pa_stream_get_sample_spec(id->pulse_stream))) / 1000);
//     id->pulse_just_flushed = 1;
//     id->pulse_time_offset_msec = time;
    
// fail:
//     if (o)
//         pa_operation_unref(o);
    
//     pa_threaded_mainloop_unlock(id->pulse_mainloop);
// }


static int
_pulse_write(AudioID *id, void* ptr, int length) 
{
  ENTER(__FUNCTION__);

  int ret = -1;
  assert(id);

  SHOW("_pulse_write > length=%d\n", length);

  CHECK_CONNECTED(id, -1);

  pa_threaded_mainloop_lock(id->pulse_mainloop);
  CHECK_DEAD_GOTO(id, fail, 1);

  if (id->pulse_drained || 
      (pa_stream_write(id->pulse_stream, ptr, length, NULL, PA_SEEK_RELATIVE, (pa_seek_mode_t)0) < 0)) {
    ERR("pa_stream_write() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    ret = -1;
    goto fail;
  }
    
  id->pulse_do_trigger = 0;
  ret = 0;

 fail:    
  pa_threaded_mainloop_unlock(id->pulse_mainloop);
  return ret;
}

static void 
_drain(AudioID *id) 
{
  pa_operation *o = NULL;
  assert(id);

  id->pulse_success = 0;

  ENTER(__FUNCTION__);

  SHOW("Draining...\n","");

  CHECK_CONNECTED_NO_RETVAL(id);

  DISPLAY_ID(id, "pa_threaded_mainloop_lock");
  
  pa_threaded_mainloop_lock(id->pulse_mainloop);
  CHECK_DEAD_GOTO(id,fail, 0);

  id->pulse_drained = 1;
  
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

  if (id->pulse_success == 0)
    ERR("pa_stream_drain() failed: %s", pa_strerror(pa_context_errno(id->pulse_context)));
    
 fail:
  SHOW_TIME("pa_operation_unref (call)");
  if (o)
    pa_operation_unref(o);

  id->pulse_drained = 0;
 
  pa_threaded_mainloop_unlock(id->pulse_mainloop);

  SHOW_TIME("_drain (ret)");
}


static void 
_pulse_close(AudioID *id) 
{
  ENTER(__FUNCTION__);
    
  assert(id);

  _drain(id);

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

  SHOW_TIME("_pulse_close (ret)");
}


static int 
_pulse_open(AudioID *id, AudioTrack track) 
{
  ENTER(__FUNCTION__);
  pa_sample_spec ss;
  pa_operation *o = NULL;

  assert(id);

  assert(!id->pulse_mainloop);
  assert(!id->pulse_context);
  assert(!id->pulse_stream);
  assert(!id->pulse_connected);

  /* Choose the correct format */
  if (track.bits == 16){
    ss.format = PA_SAMPLE_S16LE;
  }else if (track.bits == 8){
    ss.format = PA_SAMPLE_U8; // TBD: PA_SAMPLE_ALAW, PA_SAMPLE_ULAW ?
  }else{
    ERR("Unsupported sound data format, track.bits = %d\n", track.bits);
    return -1;
  }

  ss.rate = track.sample_rate;
  ss.channels = track.num_channels;

  SHOW("Setting sample spec to format=%s, rate=%i, count=%i\n", 
       pa_sample_format_to_string(ss.format),
       ss.rate, 
       ss.channels);

  if (!pa_sample_spec_valid(&ss)) {      
    ERR("Sample spec not valid!\n","");
    return -1;
  }

  SHOW("Sample spec valid\n","");

  /*     if (!id->pulse_volume_valid) { */
  pa_cvolume_reset(&id->pulse_volume, ss.channels);
  id->pulse_volume_valid = 1;
  /*     } else if (id->pulse_volume.channels != ss.channels) */
  /*         pa_cvolume_set(&id->pulse_volume, ss.channels, pa_cvolume_avg(&id->pulse_volume)); */

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
  a_attr.fragsize = 882;

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

  SHOW("_pulse_open (ret true)\n","");

  return 0;

 unlock_and_fail:

  if (o)
    pa_operation_unref(o);
    
  pa_threaded_mainloop_unlock(id->pulse_mainloop);
    
 fail:

  _pulse_close(id);

  SHOW("_pulse_open (ret false)\n","");
    
  return -1;
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
  int ret;

  if (id == NULL){
    ERR("Can't open PulseAudio sound output, invalid AudioID structure.\n","");
    return -1;
  }

  if (pars[0] == NULL){
    ERR("Can't open PulseAudio sound output, missing parameters in argument.\n","");
    return -1;
  }
    
  id->pulse_context = NULL;
  id->pulse_stream = NULL;
  id->pulse_mainloop = NULL;
  //  id->pulse_volume = 0;
  id->pulse_volume_valid = 0;
  id->pulse_do_trigger = 0;
  id->pulse_time_offset_msec = 0;
  id->pulse_just_flushed = 0;
  id->pulse_connected = 0;
  id->pulse_success = 0;
  id->pulse_drained = 0;
  id->pulse_volume_time_event = NULL;
  
  if (strcmp(pars[0], "default") != 0) {
    id->pulse_server = strdup(pars[0]);
  }
  else {
    id->pulse_server = NULL;
  }
  
  id->pulse_max_length = (int)pars[1];
  id->pulse_target_length = (int)pars[2];
  id->pulse_pre_buffering = (int)pars[3];
  id->pulse_min_request = (int)pars[4];

  SHOW("PulseAudio sound output opened\n","");

  return 0;
}

int
pulse_play(AudioID *id, AudioTrack track)
{
  ENTER(__FUNCTION__);

  int bytes_per_sample;
  int num_bytes;
  int i;  
  int err;
  int ret;
  unsigned int sr;
  unsigned int a_total_free_mem;

  if (id == NULL){
    ERR("Invalid device passed to %s()\n",__FUNCTION__);
    return -1;
  }

  SHOW("Start of playback on PulseAudio\n","");
  
  /* Is it not an empty track? */
  /* Passing an empty track is not an error */
  if (track.samples == NULL) {
    ERR("Empty track!\n","");
    return 0;
  }

  if (!id->pulse_mainloop) {
    ret = _pulse_open(id, track);
    if (ret != 0) {
      return ret;
    }
  }

  SHOW("Checking buffer size\n","");
  if (track.bits == 16){
    bytes_per_sample = 2;
  }else if (track.bits == 8){
    bytes_per_sample = 1;    
  }else{
    ERR("Unsupported sound data format, track.bits = %d\n", track.bits);
    return -1;
  }

  /* Loop until all samples are played on the device. */

  signed short* output_samples = track.samples;
  num_bytes = track.num_samples*bytes_per_sample;
  
  SHOW("track.samples=%d, track.num_samples=%d, bytes_per_sample=%d\n", track.samples, track.num_samples, bytes_per_sample);

  DISPLAY_ID(id, "pulse_play");

  while(!id->pulse_drained && (num_bytes > 0)) {
    SHOW("Still %d bytes left to be played\n", num_bytes);

    ret = _pulse_free(id, &a_total_free_mem);
    if (ret != 0) {
      return ret;
    }

    if (a_total_free_mem >= num_bytes) {
      SHOW("a_total_free_mem(%d) >= num_bytes(%d)\n", a_total_free_mem, num_bytes);
      ret = _pulse_write(id, output_samples, num_bytes);
      num_bytes = 0;
    }
    else if (a_total_free_mem > 500) {
      // 500: threshold for avoiding to many calls to pulse_write
      SHOW("a_total_free_mem(%d) < num_bytes(%d)\n", a_total_free_mem, num_bytes);
      ret = _pulse_write(id, output_samples, a_total_free_mem);
      num_bytes -= a_total_free_mem;
      output_samples += a_total_free_mem/2;
    }
    if (ret) {
	return ret;
      }
    usleep(10000);
  }

  SHOW_TIME("pulse_play (ret)");
  return 0;   
}

int
pulse_stop(AudioID *id)
{
  ENTER(__FUNCTION__);

  if (id == NULL){
    ERR("Invalid device passed to %s\n",__FUNCTION__);
    return -1;
  }

  _drain(id);

  return 0;
}

int
pulse_close(AudioID *id)
{   
  ENTER(__FUNCTION__);
  if (id == NULL){
    ERR("Invalid device passed to %s\n",__FUNCTION__);
    return -1;
  }

  _pulse_close(id);

  return 0;
}

int
pulse_set_volume(AudioID*id, int volume)
{
  ENTER(__FUNCTION__);

  SHOW("volume=%d\n", volume);

  if ((volume > 100) || (volume < -100)){
    ERR("Requested volume out of range (%d)", volume);
    return -1;
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
    
  return 0;
}

/* Provide the PulseAudio backend */
AudioFunctions pulse_functions = {pulse_open, pulse_play, pulse_stop, pulse_close, pulse_set_volume};
