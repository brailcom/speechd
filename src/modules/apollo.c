/*
 * apollo.c --- Speech Dispatcher backend for the Apollo2 hardware synthesizer
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
 * $Id: apollo.c,v 1.18 2003-09-07 11:24:54 hanke Exp $
 */


#include <fcntl.h>
#include <glib.h>
#include <math.h>
#include <unistd.h>

#include <stdio.h>
#include <stdbool.h>
#include <recode.h>

#include "module.h"
#include "fdset.h"

#include "module_utils.c"

#define MODULE_NAME     "apollo"
#define MODULE_VERSION  "0.1"

MOD_OPTION_1_STR(ApolloDevice);
MOD_OPTION_3_HT(ApolloLanguage, lang, rom, char_coding);

static const gchar AT_REPLACEMENT_CHAR = ' ';

static int fd = -1;
static RECODE_OUTER recode_outer;
static RECODE_REQUEST recode_request;

/* *** Internal functions *** */

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
  return write_it ((const void *)command, strlen (command) * sizeof (char))
         || write_it ((const void *)"\r", sizeof (char));
}

static gint set_rate (signed int value)
{
  int rate = (int) (pow (3, (value+100)/100.0));
  char command[4];
  if (rate < 0)
    rate = 0;
  else if (rate > 9)
    rate = 9;
  
  snprintf (command, 4, "@W%d", rate);
  return send_apollo_command (command);
}

static gint set_pitch (signed int value)
{
  int pitch = (int) ((value+100)/13.0 + 0.5);
  char command[4];
  if (pitch < 0x0)
    pitch = 0x0;
  else if (pitch > 0xF)
    pitch = 0xF;
  
  snprintf (command, 4, "@F%x", pitch);
  return send_apollo_command (command);
}

static gint set_language (const char *value)
{
  TApolloLanguage *language_def;
  const int MAX_REQUEST_LENGTH = 100;
  char command[5], request[MAX_REQUEST_LENGTH];
  char rom, *char_coding;
  
  language_def = (TApolloLanguage*) module_get_ht_option(ApolloLanguage, value);
  if (language_def == NULL)
    {
      rom = '1';
      char_coding = "ASCII";
    }
  else 
    {
      rom = language_def->rom[0];
      char_coding = language_def->char_coding;
    }

  sprintf (command, "@=%c,", rom);
  snprintf (request, MAX_REQUEST_LENGTH, "UTF-8..%s", char_coding);

  recode_scan_request (recode_request, request);
  return send_apollo_command (command);
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

int module_load (void)
{
  INIT_SETTINGS_TABLES();

  MOD_OPTION_1_STR_REG(ApolloDevice, "/dev/apollo");
  MOD_OPTION_HT_REG(ApolloLanguage);
  
  return 0;
}

int module_init (void)
{

  system ("stty -F /dev/apollo sane 9600 raw crtscts");
  system ("stty -F /dev/apollo -echo");
  fd = open (ApolloDevice, O_WRONLY);
  if (fd == -1) return -1;
  {
    const char *command = "@P0\r";
    write_it (command, strlen (command) * sizeof (char));
  }

  recode_outer = recode_new_outer (true);
  recode_request = recode_new_request (recode_outer);
  recode_scan_request (recode_request, "UTF-8..ASCII");

  return 0;
}

int module_write (char *data, size_t bytes)
{
  
  UPDATE_PARAMETER (rate, set_rate);
  UPDATE_PARAMETER (pitch, set_pitch);
  UPDATE_PARAMETER (voice, set_voice_type);
  UPDATE_STRING_PARAMETER (language, set_language);

  /* TODO: Apollo has a buffer of a certain size (~2 KB?).  If the buffer size
     is exceeded, the extra data is thrown away. */
  {
    gchar *escaped_data = g_malloc ((bytes + 1) + 1);
    int result;
    
    if (escaped_data == NULL)
      {
	result = 1;
      }
    else 
      {
	char *final_data;
	int i;
	
	for (i = 0; i < bytes; i++)
	  escaped_data[i] = (data[i] == '@'  ? AT_REPLACEMENT_CHAR :
			     data[i] == '\n' ? '\r' :
			                       data[i]);
	escaped_data[bytes] = '\r';
	escaped_data[bytes+1] = '\0';
	final_data = recode_string (recode_request, escaped_data);
	g_free (escaped_data);
	result = write_it (final_data, strlen (final_data));
	free (final_data);
      }

    return ! result;
  }
}

int module_stop (void)
{
  return send_apollo_command ("\030");
}

size_t module_pause (void)
{
  module_stop ();
  return 0;
}

int module_is_speaking (void)
{
    return 2;             /* Apollo doesn't provide any information */
}

void module_close (int status)
{
  if (fd != -1) close (fd);
  exit(status);
}

#include "module_main.c"
