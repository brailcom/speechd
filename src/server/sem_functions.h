/*
 * sem_functions.h - Functions for manipulating System V / IPC semaphores
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
 * $Id: sem_functions.h,v 1.2 2003-09-28 22:30:54 hanke Exp $
 */

int semaphore_create(key_t key, int flags);
void semaphore_destroy(int sem_id);
void semaphore_wait(int sem_id);
void semaphore_post(int sem_id);

int speaking_semaphore_create(key_t key);
void speaking_semaphore_destroy();
void speaking_semaphore_post(void);
void speaking_semaphore_wait(void);
