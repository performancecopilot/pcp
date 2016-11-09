/*
 * Copyright (c) 2015-2016 Red Hat.
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
#include <pwd.h>
#include "ipc.h"
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "indom.h"

int
refresh_shm_info(shm_info_t *_shm_info)
{
    static struct shm_info shm_info;
    extern size_t _pm_system_pagesize;

    if (shmctl(0, SHM_INFO, (struct shmid_ds *) &shm_info) < 0)
	return -oserror();

    _shm_info->shm_tot = shm_info.shm_tot * _pm_system_pagesize;
    _shm_info->shm_rss = shm_info.shm_rss * _pm_system_pagesize;
    _shm_info->shm_swp = shm_info.shm_swp * _pm_system_pagesize;
    _shm_info->used_ids = shm_info.used_ids;
    _shm_info->swap_attempts = shm_info.swap_attempts;
    _shm_info->swap_successes = shm_info.swap_successes;
    return 0;
}

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
refresh_sem_info(sem_info_t *sem_info)
{
    static struct seminfo seminfo;
    static union semun arg;

    arg.array = (unsigned short *) &seminfo;
    if (semctl(0, 0, SEM_INFO, arg) < 0)
    	return -oserror();

    sem_info->semusz = seminfo.semusz;
    sem_info->semaem = seminfo.semaem;
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
refresh_msg_info(msg_info_t *msg_info)
{
    static struct msginfo msginfo;

    if (msgctl(0, MSG_INFO, (struct msqid_ds *) &msginfo) < 0)
    	return -oserror();

    msg_info->msgpool = msginfo.msgpool;
    msg_info->msgmap = msginfo.msgmap;
    msg_info->msgtql = msginfo.msgtql;
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

int 
refresh_shm_stat(pmInDom shm_indom)
{
    struct passwd *pw = NULL;
    char shmid[16]; 
    char perms[16];
    int i = 0, maxid = 0;
    int sts = 0;
    shm_stat_t *shm_stat = NULL;
    struct shm_info dummy;

    pmdaCacheOp(shm_indom, PMDA_CACHE_INACTIVE);
    maxid = shmctl(0, SHM_INFO, (struct shmid_ds *)&dummy);
    if (maxid < 0)
	return -1;
 
    while (i <= maxid) {
	int shmid_o;
	struct shmid_ds shmseg;
	struct ipc_perm *ipcp = &shmseg.shm_perm; 

	if ((shmid_o = shmctl(i++, SHM_STAT, &shmseg)) < 0)
	    continue;

	snprintf(shmid, sizeof(shmid), "%d", shmid_o);
	shmid[sizeof(shmid)-1] = '\0';
	sts = pmdaCacheLookupName(shm_indom, shmid, NULL, (void **)&shm_stat);
	if (sts == PMDA_CACHE_ACTIVE)
	    continue;

	if (sts == PMDA_CACHE_INACTIVE) {
	    pmdaCacheStore(shm_indom, PMDA_CACHE_ADD, shmid, shm_stat);
	}
	else {
	    if ((shm_stat = (shm_stat_t *)malloc(sizeof(shm_stat_t))) == NULL)
		continue;
	    memset(shm_stat, 0, sizeof(shm_stat_t));

	    snprintf(shm_stat->shm_key, SHM_KEYLEN, "0x%08x", ipcp->KEY); 
	    shm_stat->shm_key[SHM_KEYLEN-1] = '\0';
	    if ((pw = getpwuid(ipcp->uid)) != NULL)
		strncpy(shm_stat->shm_owner, pw->pw_name, SHM_OWNERLEN);
	    else
		snprintf(shm_stat->shm_owner, SHM_OWNERLEN, "%d", ipcp->uid);
	    shm_stat->shm_owner[SHM_OWNERLEN-1] = '\0';
	    /* convert to octal number */
	    snprintf(perms, sizeof(perms), "%o", ipcp->mode & 0777);
	    perms[sizeof(perms)-1] = '\0';
	    shm_stat->shm_perms      = atoi(perms);
	    shm_stat->shm_bytes      = shmseg.shm_segsz;
	    shm_stat->shm_nattch     = shmseg.shm_nattch;
	    if ((ipcp->mode & SHM_DEST))
		shm_stat->shm_status = "dest";
	    else if ((ipcp->mode & SHM_LOCKED))
		shm_stat->shm_status = "locked";
	    else
		shm_stat->shm_status = " ";
	    sts = pmdaCacheStore(shm_indom, PMDA_CACHE_ADD, shmid, (void *)shm_stat);
	    if (sts < 0) {
		fprintf(stderr, "Warning: %s: pmdaCacheStore(%s, %s): %s\n",
			__FUNCTION__, shmid, shm_stat->shm_key, pmErrStr(sts));
		free(shm_stat->shm_key);
		free(shm_stat->shm_owner);
		free(shm_stat->shm_status);
		free(shm_stat);
	    }	
	}
    }
    pmdaCacheOp(shm_indom, PMDA_CACHE_SAVE);
    return 0;
}
