/*
 * symbols.h -- Implements functions handling symbols conversion,
 *              including punctuation, for Speech Dispatcher (header)
 *
 * Copyright (C) 2001,2002,2003,2017 Brailcom, o.p.s
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
 */

#include "speechd.h"

#ifndef SYMBOLS_H
#define SYMBOLS_H

/* Converts symbols to words corresponding to a level into a message. */
void insert_symbols(TSpeechDMessage *msg);

#endif /* SYMBOLS_H */
