/*
 * Copyright (c) 2015 Red Hat.
 * Copyright (c) 2002 International Business Machines Corp.
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

#include "pmapi.h"
#define __USE_GNU 1
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include "ipc.h"

int
refresh_shm_limits(shm_limits_t *shm_limits)
{
    static struct shminfo shminfo;

    if (shmctl(0, IPC_INFO, (struct shmid_ds *) &shminfo) < 0)
    	return -oserror();

    shm_limits->shmmax = shminfo.shmmax;
    shm_limits->shmmin = shminfo.shmmin;
    shm_limits->shmmni = shminfo.shmmni;
    shm_limits->shmseg = shminfo.shmseg;
    shm_limits->shmall = shminfo.shmall;
    return 0;
}

int
refresh_sem_limits(sem_limits_t *sem_limits)
{
    static struct seminfo seminfo;
    static union semun arg;

    arg.array = (unsigned short *) &seminfo;
    if (semctl(0, 0, IPC_INFO, arg) < 0)
    	return -oserror();

    sem_limits->semmap = seminfo.semmap;
    sem_limits->semmni = seminfo.semmni;
    sem_limits->semmns = seminfo.semmns;
    sem_limits->semmnu = seminfo.semmnu;
    sem_limits->semmsl = seminfo.semmsl;
    sem_limits->semopm = seminfo.semopm;
    sem_limits->semume = seminfo.semume;
    sem_limits->semusz = seminfo.semusz;
    sem_limits->semvmx = seminfo.semvmx;
    sem_limits->semaem = seminfo.semaem;
    return 0;
}

int
refresh_msg_limits(msg_limits_t *msg_limits)
{
    static struct msginfo msginfo;

    if (msgctl(0, IPC_INFO, (struct msqid_ds *) &msginfo) < 0)
    	return -oserror();

    msg_limits->msgpool = msginfo.msgpool;
    msg_limits->msgmap = msginfo.msgmap;
    msg_limits->msgmax = msginfo.msgmax;
    msg_limits->msgmnb = msginfo.msgmnb;
    msg_limits->msgmni = msginfo.msgmni;
    msg_limits->msgssz = msginfo.msgssz;
    msg_limits->msgtql = msginfo.msgtql;
    msg_limits->msgseg = msginfo.msgseg;
    return 0;
}
