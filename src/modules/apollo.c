/*
 * apollo.c --- Speech Deamon backend for the Apollo2 hardware synthesizer
 *
 * Copyright (C) 2003 Brailcom, o.p.s.
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
 * $Id: apollo.c,v 1.2 2003-04-23 16:04:14 pdm Exp $
 */


#include <fcntl.h>
#include <glib.h>
#include <unistd.h>

#include "module.h"
#include "fdset.h"


gint apollo_write (gchar*, gint, void*);
gint apollo_stop (void);
gint apollo_is_speaking (void);
gint apollo_close (void);

OutputModule module_apollo = {
  "apollo",			/* name */
  "Apollo2 hardware synthesizer", /* description */
  NULL,				/* filename -- what's it? */
  NULL,				/* GModule* -- what's it? */
  apollo_write,
  apollo_stop,
  apollo_is_speaking,
  apollo_close
};

static const char * const DEVICE = "/dev/apollo";
static const gchar AT_REPLACEMENT_CHAR = ' ';

static int fd = -1;
static TFDSetElement current_parameters;


/* *** Internal functions *** */


#define UPDATE_PARAMETER(old_value, new_value, setter) \
  if (old_value != (new_value)) \
    { \
      old_value = (new_value); \
      setter ((new_value)); \
    }

#define UPDATE_STRING_PARAMETER(old_value, new_value, setter) \
  if (old_value == NULL || strcmp (old_value, new_value)) \
    { \
      if (old_value != NULL) \
      { \
        g_free (old_value); \
	old_value = NULL; \
      } \
      if (new_value == NULL) \
      { \
        old_value = g_strdup (new_value); \
        setter ((new_value)); \
      } \
    }

static gint write_it (const void *data, size_t size)
{
  if (fd == -1)
    return -2;

  while (size > 0)
    {
      int n = write (fd, data, size);
      if (n == -1)
	return -1;
      size = size - n;
      data = data + n;
    }
  return 0;
}

static gint send_apollo_command (const char *command)
{
  return write_it ((const void *)command, strlen (command) * sizeof (char));
}

static gint set_speed (signed int value)
{
  int speed = (int) (pow (3, (value+100)/100.0));
  char *command = "@W_";
  if (speed < 0)
    speed = 0;
  else if (speed > 9)
    speed = 9;

  sprintf (command+2, "%d", speed);
  return send_apollo_command (command);
}

static gint set_pitch (signed int value)
{
  int pitch = (int) ((value+100)/13.0 + 0.5);
  char *command = "@F_";
  if (pitch < 0x0)
    pitch = 0x0;
  else if (pitch > 0xF)
    pitch = 0xF;
  
  sprintf (command+2, "%x", pitch);
  return send_apollo_command (command);
}

static gint set_language (const char *value)
{
  /* TODO: Just hard-wired switching between English and another language.
     Any masochist willing to use @L and DTR in C is welcome to do so here.
  */
  return send_apollo_command (strcmp (value, "en") ? "@=2," : "@=1,");
}

static gint set_voice_type (int value)
{
  char *command;

#define CASE(x, n) case x: command = ("@V" n); break;
  switch (value)
    {
      CASE (MALE1, "1");
      CASE (MALE2, "2");
      CASE (MALE3, "3");
      CASE (CHILD_MALE, "3");
      CASE (FEMALE1, "4");
      CASE (FEMALE2, "5");
      CASE (FEMALE3, "6");
      CASE (CHILD_FEMALE, "6");
    default:
      return 1;
    }
#undef CASE
  
  return send_apollo_command (command);
}


/* *** Interface functions *** */


OutputModule *module_init (void)
{
  current_parameters.speed = -101;
  current_parameters.pitch = -101;
  current_parameters.language = NULL;
  current_parameters.voice_type = -1;

  system ("stty -F /dev/apollo sane 9600 raw crtscts");
  system ("stty -F /dev/apollo -echo");
  fd = open (DEVICE, O_WRONLY);
  
  return &module_apollo;
}

gint apollo_write (gchar *data, gint len, void* set_)
{
  TFDSetElement *set = set_;
  
  UPDATE_PARAMETER (current_parameters.speed, set->speed, set_speed);
  UPDATE_PARAMETER (current_parameters.pitch, set->pitch, set_pitch);
  UPDATE_PARAMETER (current_parameters.voice_type, set->voice_type,
		    set_voice_type);
  UPDATE_STRING_PARAMETER (current_parameters.language, set->language,
			   set_language);

  /* TODO: Apollo has a buffer of a certain size (~2 KB?).  If the buffer size
     is exceeded, the extra data is thrown away. */
  {
    int blen = len * sizeof (gchar);
    gchar *edata = g_malloc (blen);
    int result;
    
    if (edata == NULL)
      {
	result = 1;
      }
    else 
      {
	int i;
	for (i = 0; i < len; i++)
	  edata[i] = (data[i] == '@' ? AT_REPLACEMENT_CHAR : data[i]);
	result = write_it (edata, blen);
	g_free (edata);
      }

    return result;
  }
}

gint apollo_stop (void)
{
  return send_apollo_command ("\030");
}

gint apollo_is_speaking (void)
{
  return 0;			/* How to do anything better? */
}

gint apollo_close (void)
{
  gint result = (fd == -1 ? -1 : close (fd));
  fd = -1;
  return result;
}
