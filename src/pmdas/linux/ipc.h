/*
 * Copyright (C) 2015 Red Hat.
 * Copyright (C) 2002 International Business Machines Corp.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

typedef struct {
    unsigned int shmmax; /* maximum shared segment size (bytes) */
    unsigned int shmmin; /* minimum shared segment size (bytes) */
    unsigned int shmmni; /* maximum number of segments system wide */
    unsigned int shmseg; /* maximum shared segments per process */
    unsigned int shmall; /* maximum shared memory system wide (pages) */
} shm_limits_t;

extern int refresh_shm_limits(shm_limits_t *);

#ifdef _SEM_SEMUN_UNDEFINED
/* glibc 2.1 no longer defines semun, instead it defines
 * _SEM_SEMUN_UNDEFINED so users can define semun on their own.
 */
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short int *array;
    struct seminfo *__buf;
};
#endif

typedef struct {
    unsigned int semmap; /* # of entries in semaphore map */
    unsigned int semmni; /* max # of semaphore identifiers */
    unsigned int semmns; /* max # of semaphores in system */  
    unsigned int semmnu; /* num of undo structures system wide */
    unsigned int semmsl; /* max num of semaphores per id */
    unsigned int semopm; /* max num of ops per semop call */
    unsigned int semume; /* max num of undo entries per process */
    unsigned int semusz; /* sizeof struct sem_undo */
    unsigned int semvmx; /* semaphore maximum value */
    unsigned int semaem; /* adjust on exit max value */
} sem_limits_t;

extern int refresh_sem_limits(sem_limits_t *);

typedef struct {
    unsigned int msgpool; /* size of message pool (kbytes) */
    unsigned int msgmap;  /* # of entries in message map */
    unsigned int msgmax;  /* maximum size of a message */
    unsigned int msgmnb;  /* default maximum size of message queue */
    unsigned int msgmni;  /* maximum # of message queue identifiers */
    unsigned int msgssz;  /* message segment size */
    unsigned int msgtql;  /* # of system message headers */
    unsigned int msgseg;  /* maximum # of message segments */
} msg_limits_t;

extern int refresh_msg_limits(msg_limits_t *);

