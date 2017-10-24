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
#include "linux.h"
#define __USE_GNU 1
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <pwd.h>
#include "ipc.h"

/*
 * We've seen some buffer overrun issues with the polymorphic "buf"
 * parameter to shmctl() on some platforms ... the union here is
 * designed to make sure we have something that is always going to
 * be big enough.
 */
typedef union {
    struct shmid_ds	shmid_ds;
    struct shm_info	shm_info;
    struct shminfo	shminfo;
} shmctl_buf_t;

int
refresh_shm_info(shm_info_t *_shm_info)
{
    static shmctl_buf_t buf;

    if (shmctl(0, SHM_INFO, &buf.shmid_ds) < 0)
	return -oserror();

    _shm_info->shm_tot = buf.shm_info.shm_tot << _pm_pageshift;
    _shm_info->shm_rss = buf.shm_info.shm_rss << _pm_pageshift;
    _shm_info->shm_swp = buf.shm_info.shm_swp << _pm_pageshift;
    _shm_info->used_ids = buf.shm_info.used_ids;
    _shm_info->swap_attempts = buf.shm_info.swap_attempts;
    _shm_info->swap_successes = buf.shm_info.swap_successes;
    return 0;
}

int
refresh_shm_limits(shm_limits_t *shm_limits)
{
    static shmctl_buf_t buf;

    if (shmctl(0, IPC_INFO, &buf.shmid_ds) < 0)
    	return -oserror();

    shm_limits->shmmax = buf.shminfo.shmmax;
    shm_limits->shmmin = buf.shminfo.shmmin;
    shm_limits->shmmni = buf.shminfo.shmmni;
    shm_limits->shmseg = buf.shminfo.shmseg;
    shm_limits->shmall = buf.shminfo.shmall;
    return 0;
}

int
refresh_sem_info(sem_info_t *sem_info)
{
    static struct seminfo seminfo;
    static union semun arg;

    arg.__buf = &seminfo;
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
    shmctl_buf_t buf;

    pmdaCacheOp(shm_indom, PMDA_CACHE_INACTIVE);
    maxid = shmctl(0, SHM_INFO, &buf.shmid_ds);
    if (maxid < 0)
	return -1;
 
    while (i <= maxid) {
	int shmid_o;
	struct ipc_perm *ipcp = &buf.shmid_ds.shm_perm; 

	if ((shmid_o = shmctl(i++, SHM_STAT, &buf.shmid_ds)) < 0)
	    continue;

	pmsprintf(shmid, sizeof(shmid), "%d", shmid_o);
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

	    pmsprintf(shm_stat->shm_key, IPC_KEYLEN, "0x%08x", ipcp->KEY); 
	    shm_stat->shm_key[IPC_KEYLEN-1] = '\0';
	    if ((pw = getpwuid(ipcp->uid)) != NULL)
		strncpy(shm_stat->shm_owner, pw->pw_name, IPC_OWNERLEN);
	    else
		pmsprintf(shm_stat->shm_owner, IPC_OWNERLEN, "%d", ipcp->uid);
	    shm_stat->shm_owner[IPC_OWNERLEN-1] = '\0';
	    /* convert to octal number */
	    pmsprintf(perms, sizeof(perms), "%o", ipcp->mode & 0777);
	    perms[sizeof(perms)-1] = '\0';
	    shm_stat->shm_perms      = atoi(perms);
	    shm_stat->shm_bytes      = buf.shmid_ds.shm_segsz;
	    shm_stat->shm_nattch     = buf.shmid_ds.shm_nattch;
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
		free(shm_stat);
	    }	
	}
    }
    pmdaCacheOp(shm_indom, PMDA_CACHE_SAVE);
    return 0;
}

int 
refresh_msg_que(pmInDom msg_indom)
{
    struct passwd *pw = NULL;
    char msgid[IPC_KEYLEN]; 
    char perms[IPC_KEYLEN];
    int i = 0, maxid = 0;
    int sts = 0;
    msg_que_t *msg_que = NULL;
    struct msqid_ds dummy;
    struct msqid_ds msgseg;  

    pmdaCacheOp(msg_indom, PMDA_CACHE_INACTIVE);

    maxid = msgctl(0, MSG_STAT, &dummy);
    if (maxid < 0)
	return -1;
 
    while (i <= maxid) {
	int msgid_o;
        struct ipc_perm *ipcp = &msgseg.msg_perm; 

	if ((msgid_o = msgctl(i++, MSG_STAT, &msgseg)) < 0)
	    continue;

	pmsprintf(msgid, sizeof(msgid), "%d", msgid_o);
	msgid[sizeof(msgid)-1] = '\0';
	sts = pmdaCacheLookupName(msg_indom, msgid, NULL, (void **)&msg_que);
	if (sts == PMDA_CACHE_ACTIVE)
	    continue;

	if (sts == PMDA_CACHE_INACTIVE) {
	    pmdaCacheStore(msg_indom, PMDA_CACHE_ADD, msgid, msg_que);
	}
	else {
	    if ((msg_que = (msg_que_t *)malloc(sizeof(msg_que_t))) == NULL)
		continue;
	    memset(msg_que, 0, sizeof(msg_que_t));

	    pmsprintf(msg_que->msg_key, IPC_KEYLEN, "0x%08x", ipcp->KEY); 
	    msg_que->msg_key[IPC_KEYLEN-1] = '\0';
	    if ((pw = getpwuid(ipcp->uid)) != NULL)
		strncpy(msg_que->msg_owner, pw->pw_name, IPC_OWNERLEN);
	    else
		pmsprintf(msg_que->msg_owner, IPC_OWNERLEN, "%d", ipcp->uid);
	    msg_que->msg_owner[IPC_OWNERLEN-1] = '\0';

	    /* convert to octal number */
	    pmsprintf(perms, sizeof(perms), "%o", ipcp->mode & 0777);
	    perms[sizeof(perms)-1] = '\0';
	    msg_que->msg_perms     = atoi(perms);
	    msg_que->msg_bytes     = msgseg.msg_cbytes;
	    msg_que->messages      = msgseg.msg_qnum;

	    sts = pmdaCacheStore(msg_indom, PMDA_CACHE_ADD, msgid, (void *)msg_que);
	    if (sts < 0) {
		fprintf(stderr, "Warning: %s: pmdaCacheStore(%s, %s): %s\n",
			__FUNCTION__, msgid, msg_que->msg_key, pmErrStr(sts));
		free(msg_que);
	    }	
	}
    }
    pmdaCacheOp(msg_indom, PMDA_CACHE_SAVE);
    return 0;
}

int
refresh_sem_array(pmInDom sem_indom)
{
    struct passwd *pw = NULL;
    char semid[IPC_KEYLEN];
    char perms[IPC_KEYLEN];
    int i = 0, maxid = 0;
    int sts = 0;
    sem_array_t *sem_arr = NULL;
    struct seminfo dummy;
    static union semun arg;
    struct semid_ds semseg;

    pmdaCacheOp(sem_indom, PMDA_CACHE_INACTIVE);

    arg.__buf = &dummy;
    maxid = semctl(0, 0, SEM_INFO, arg);
    if (maxid < 0)
	return -1;
 
    while (i <= maxid) {
	int semid_o;
        struct ipc_perm *ipcp = &semseg.sem_perm;
        arg.buf = (struct semid_ds *)&semseg;

	if ((semid_o = semctl(i++, 0, SEM_STAT, arg)) < 0)
	    continue;

	pmsprintf(semid, sizeof(semid), "%d", semid_o);
	semid[sizeof(semid)-1] = '\0';
	sts = pmdaCacheLookupName(sem_indom, semid, NULL, (void **)&sem_arr);
	if (sts == PMDA_CACHE_ACTIVE)
	    continue;

	if (sts == PMDA_CACHE_INACTIVE) {
	    pmdaCacheStore(sem_indom, PMDA_CACHE_ADD, semid, sem_arr);
	}
	else {
	    if ((sem_arr = (sem_array_t *)malloc(sizeof(sem_array_t))) == NULL)
		continue;
	    memset(sem_arr, 0, sizeof(sem_array_t));

	    pmsprintf(sem_arr->sem_key, IPC_KEYLEN, "0x%08x", ipcp->KEY);
	    sem_arr->sem_key[IPC_KEYLEN-1] = '\0';
	    if ((pw = getpwuid(ipcp->uid)) != NULL)
		strncpy(sem_arr->sem_owner, pw->pw_name, IPC_OWNERLEN);
	    else
		pmsprintf(sem_arr->sem_owner, IPC_OWNERLEN, "%d", ipcp->uid);
	    sem_arr->sem_owner[IPC_OWNERLEN-1] = '\0';

	    /* convert to octal number */
	    pmsprintf(perms, sizeof(perms), "%o", ipcp->mode & 0777);
	    perms[sizeof(perms)-1] = '\0';
	    sem_arr->sem_perms     = atoi(perms);
	    sem_arr->nsems      = semseg.sem_nsems;

	    sts = pmdaCacheStore(sem_indom, PMDA_CACHE_ADD, semid, (void *)sem_arr);
	    if (sts < 0) {
		fprintf(stderr, "Warning: %s: pmdaCacheStore(%s, %s): %s\n",
			__FUNCTION__, semid, sem_arr->sem_key, pmErrStr(sts));
		free(sem_arr);
	    }
	}
    }
    pmdaCacheOp(sem_indom, PMDA_CACHE_SAVE);
    return 0;
}
