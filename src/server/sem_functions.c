
/*
 * sem_functions.c - Functions for manipulating System V / IPC semaphores
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
 * $Id: sem_functions.c,v 1.2 2003-09-28 22:30:44 hanke Exp $
 */

#include "speechd.h"
#include "sem_functions.h"

int
semaphore_create(key_t key, int flags)
{
    union semun sem_union;
    int sem_id;
    int err;

    sem_id = semget(key, 1, flags);
    if (sem_id == -1) return -1;
    
    sem_union.val = 1;
    if(semctl(sem_id, 0, SETVAL, sem_union) == -1) return -1;

    return sem_id;    
}

void
semaphore_destroy(int sem_id)
{
    union semun sem_union;
    if (semctl(sem_id, 0, IPC_RMID, sem_union) == -1)
        MSG(2, "Failed to delete System V/IPC semaphore!");
}

void
semaphore_wait(int sem_id)
{
    static struct sembuf sem_b;
    int err;
    
    sem_b.sem_num = 0;
    sem_b.sem_op = -1;          /* P() */
    sem_b.sem_flg = SEM_UNDO;
    if (semop(sem_id, &sem_b, 1) == -1){
        err = errno;
        MSG(1, "semaphore_wait failed: %s", strerror(err));    
        FATAL("Can't continue");
    }
}

void
semaphore_post(int sem_id)
{
    struct sembuf sem_b;
    int err;

    sem_b.sem_num = 0;
    sem_b.sem_op = 1;          /* V() */
    sem_b.sem_flg = SEM_UNDO;
    MSG(4, "Posting on semaphore.");
    if (semop(sem_id, &sem_b, 1) == -1){
        err = errno;
        MSG(1,"Semaphore_post on semaphore id %d failed: errno=%d %s",
            sem_id, err, strerror(err));
        MSG(1,"You may want to run `ipcs -s' to see if the semaphore still exits.");
        FATAL("Can't continue");
    }
}

int
speaking_semaphore_create(key_t key)
{
    int ret;
    ret = semaphore_create(key, 0666 | IPC_CREAT);
    if (ret == -1) 
        FATAL("Can't initialize semaphore. Does your system support SYS V/IPC?");   
    MSG(4, "Created semaphore with key %d and id %d", key, ret);
    return ret;
}

void
speaking_semaphore_destroy()
{
    semaphore_destroy(speaking_sem_id);
}

void
speaking_semaphore_post(void)
{
    semaphore_post(speaking_sem_id);
}

void
speaking_semaphore_wait(void)
{
    semaphore_wait(speaking_sem_id);
}
