
/*
 * compare.c - Auxiliary functions for comparing elements in lists
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
 * $Id: compare.c,v 1.2 2003-04-15 10:09:00 pdm Exp $
 */

#include <glib.h>
#include "speechd.h"
#include "compare.h"

/* Pointers to compare_message_fd and compare_message_uid */
gint (*p_msg_lc)() = compare_message_fd;
gint (*p_msg_uid_lc)() = compare_message_uid;

gint
compare_message_fd (gconstpointer element, gconstpointer value, gpointer x)
{
    int ret;
    int *fd_val;
    TSpeechDMessage *message;

    fd_val = (int*) value;

    message = ((TSpeechDMessage*) element);
    assert(message!=NULL);
    assert(message->settings.fd!=0);

    return (message->settings.fd - *fd_val);
}

gint
compare_message_uid (gconstpointer element, gconstpointer value, gpointer x)
{
    int *uid_val;
    TSpeechDMessage *message;

    uid_val = (int*) value;

    message = ((TSpeechDMessage*) element);
    assert(message!=NULL);    
    assert(message->settings.fd!=0);

    return (message->settings.uid - *uid_val);
}
