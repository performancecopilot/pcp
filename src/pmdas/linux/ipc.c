/*
 * Copyright (c) 2015-2016,2019 Red Hat.
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
refresh_shm_info(shm_info_t *shmp)
{
    shmctl_buf_t buf = {0};

    if (shmctl(0, SHM_INFO, &buf.shmid_ds) < 0)
	return -oserror();

    shmp->shm_tot = buf.shm_info.shm_tot << _pm_pageshift;
    shmp->shm_rss = buf.shm_info.shm_rss << _pm_pageshift;
    shmp->shm_swp = buf.shm_info.shm_swp << _pm_pageshift;
    shmp->used_ids = buf.shm_info.used_ids;
    shmp->swap_attempts = buf.shm_info.swap_attempts;
    shmp->swap_successes = buf.shm_info.swap_successes;
    return 0;
}

int
refresh_shm_limits(shm_limits_t *shmp)
{
    static shmctl_buf_t buf;

    if (shmctl(0, IPC_INFO, &buf.shmid_ds) < 0)
	return -oserror();

    shmp->shmmax = buf.shminfo.shmmax;
    shmp->shmmin = buf.shminfo.shmmin;
    shmp->shmmni = buf.shminfo.shmmni;
    shmp->shmseg = buf.shminfo.shmseg;
    shmp->shmall = buf.shminfo.shmall;
    return 0;
}

int
refresh_sem_info(sem_info_t *semp)
{
    struct seminfo seminfo = {0};
    union semun arg = {0};

    arg.__buf = &seminfo;
    if (semctl(0, 0, SEM_INFO, arg) < 0)
	return -oserror();

    semp->semusz = seminfo.semusz;
    semp->semaem = seminfo.semaem;
    return 0;
}

int
refresh_sem_limits(sem_limits_t *semp)
{
    struct seminfo seminfo = {0};
    union semun arg = {0};

    arg.array = (unsigned short *)&seminfo;
    if (semctl(0, 0, IPC_INFO, arg) < 0)
	return -oserror();

    semp->semmap = seminfo.semmap;
    semp->semmni = seminfo.semmni;
    semp->semmns = seminfo.semmns;
    semp->semmnu = seminfo.semmnu;
    semp->semmsl = seminfo.semmsl;
    semp->semopm = seminfo.semopm;
    semp->semume = seminfo.semume;
    semp->semusz = seminfo.semusz;
    semp->semvmx = seminfo.semvmx;
    semp->semaem = seminfo.semaem;
    return 0;
}

int
refresh_msg_info(msg_info_t *msgp)
{
    struct msginfo msginfo = {0};

    if (msgctl(0, MSG_INFO, (struct msqid_ds *) &msginfo) < 0)
	return -oserror();

    msgp->msgpool = msginfo.msgpool;
    msgp->msgmap = msginfo.msgmap;
    msgp->msgtql = msginfo.msgtql;
    return 0;
}

int
refresh_msg_limits(msg_limits_t *msgp)
{
    struct msginfo msginfo = {0};

    if (msgctl(0, IPC_INFO, (struct msqid_ds *) &msginfo) < 0)
	return -oserror();

    msgp->msgpool = msginfo.msgpool;
    msgp->msgmap = msginfo.msgmap;
    msgp->msgmax = msginfo.msgmax;
    msgp->msgmnb = msginfo.msgmnb;
    msgp->msgmni = msginfo.msgmni;
    msgp->msgssz = msginfo.msgssz;
    msgp->msgtql = msginfo.msgtql;
    msgp->msgseg = msginfo.msgseg;
    return 0;
}

/* obtain username for uid, with fallback */
static void
extract_owner(unsigned int uid, char *owner)
{
    struct passwd	*pw = NULL;

    if ((pw = getpwuid(uid)) != NULL)
	pmsprintf(owner, IPC_OWNERLEN, "%s", pw->pw_name);
    else
	pmsprintf(owner, IPC_OWNERLEN, "%u", uid);
}

/* convert octal to decimal after masking */
static void
extract_perms(unsigned int imode, unsigned int *omode)
{
    char		buf[32];

    pmsprintf(buf, sizeof(buf), "%o", imode & 0777);
    *omode = atoi(buf);
}

int
refresh_shm_stat(pmInDom shm_indom)
{
    shm_stat_t		*shmp, sbuf = {0};
    unsigned long long	unusedll;
    unsigned int	unused;
    char		shmid[IPC_KEYLEN]; 
    char		buf[512];
    FILE		*fp;
    int			sts, needsave = 0;

    pmdaCacheOp(shm_indom, PMDA_CACHE_INACTIVE);

    if ((fp = linux_statsfile("/proc/sysvipc/shm", buf, sizeof(buf))) == NULL)
	return -oserror();

    /* skip header, then iterate over shared memory segments adding to cache */
    if (fgets(buf, sizeof(buf), fp) != NULL) {
	/* key shmid perms size cpid lpid nattch uid gid cuid cgid atime dtime ctime [rss swap] */
	while ((sts = fscanf(fp, "%d %u %o %llu %u %u %u %u %u %u %u %llu %llu %llu %llu %llu",
			&sbuf.key, &sbuf.id, &sbuf.perms, &sbuf.bytes,
			&sbuf.cpid, &sbuf.lpid, &sbuf.nattach,
			&sbuf.uid, &unused, &unused, &unused, &unusedll,
			&unusedll, &unusedll, &unusedll, &unusedll)) >= 8) {
	    pmsprintf(shmid, sizeof(shmid), "%d", sbuf.id);
	    sts = pmdaCacheLookupName(shm_indom, shmid, NULL, (void **)&shmp);
	    if (sts == PMDA_CACHE_ACTIVE)
		continue;
	    if (sts != PMDA_CACHE_INACTIVE)
		needsave = 1;
	    if (sts != PMDA_CACHE_INACTIVE &&
		(shmp = (shm_stat_t *)calloc(1, sizeof(shm_stat_t))) == NULL)
		continue;

	    sbuf.dest = (sbuf.perms & SHM_DEST) ? 1 : 0;
	    sbuf.locked = (sbuf.perms & SHM_LOCKED) ? 1 : 0;
	    extract_owner(sbuf.uid, &sbuf.owner[0]);
	    extract_perms(sbuf.perms, &sbuf.perms);
	    pmsprintf(sbuf.keyid, IPC_KEYLEN, "0x%08x", sbuf.key); 
	    memcpy(shmp, &sbuf, sizeof(shm_stat_t));

	    sts = pmdaCacheStore(shm_indom, PMDA_CACHE_ADD, shmid, (void *)shmp);
	    if (sts < 0) {
		fprintf(stderr, "Warning: %s: pmdaCacheStore(%s, %s): %s\n",
			__FUNCTION__, shmid, shmp->keyid, pmErrStr(sts));
		free(shmp);
	    }	
	}
    }
    fclose(fp);

    if (needsave)
	pmdaCacheOp(shm_indom, PMDA_CACHE_SAVE);
    return 0;
}

int 
refresh_msg_queue(pmInDom msg_indom)
{
    msg_queue_t		*mqp, mbuf = {0};
    unsigned long long	unusedll;
    unsigned int	unused;
    char		msgid[IPC_KEYLEN]; 
    char		buf[512];
    FILE		*fp;
    int			sts, needsave = 0;

    pmdaCacheOp(msg_indom, PMDA_CACHE_INACTIVE);

    if ((fp = linux_statsfile("/proc/sysvipc/msg", buf, sizeof(buf))) == NULL)
	return -oserror();

    /* skip header, then iterate over each message queue line adding to cache */
    if (fgets(buf, sizeof(buf), fp) != NULL) {
	/* key msqid perms cbytes qnum lspid lrpid uid gid cuid cgid stime rtime ctime */
	while (fscanf(fp, "%d %u %o %u %u %u %u %u %u %u %u %llu %llu %llu",
			&mbuf.key, &mbuf.id, &mbuf.perms, &mbuf.bytes,
			&mbuf.messages, &mbuf.lspid, &mbuf.lrpid,
			&mbuf.uid, &unused, &unused, &unused,
			&unusedll, &unusedll, &unusedll) >= 8) {
	    pmsprintf(msgid, sizeof(msgid), "%d", mbuf.id);
	    sts = pmdaCacheLookupName(msg_indom, msgid, NULL, (void **)&mqp);
	    if (sts == PMDA_CACHE_ACTIVE)
		continue;
	    if (sts != PMDA_CACHE_INACTIVE)
		needsave = 1;
	    if (sts != PMDA_CACHE_INACTIVE &&
		(mqp = (msg_queue_t *)calloc(1, sizeof(msg_queue_t))) == NULL)
		continue;

	    extract_owner(mbuf.uid, &mbuf.owner[0]);
	    extract_perms(mbuf.perms, &mbuf.perms);
	    pmsprintf(mbuf.keyid, IPC_KEYLEN, "0x%08x", mbuf.key); 
	    memcpy(mqp, &mbuf, sizeof(msg_queue_t));

	    sts = pmdaCacheStore(msg_indom, PMDA_CACHE_ADD, msgid, (void *)mqp);
	    if (sts < 0) {
		fprintf(stderr, "Warning: %s: pmdaCacheStore(%s, %s): %s\n",
			__FUNCTION__, msgid, mqp->keyid, pmErrStr(sts));
		free(mqp);
	    }	
	}
    }
    fclose(fp);

    if (needsave)
	pmdaCacheOp(msg_indom, PMDA_CACHE_SAVE);
    return 0;
}

int
refresh_sem_array(pmInDom sem_indom)
{
    sem_array_t		*semp, sbuf = {0};
    unsigned long long	unusedll;
    unsigned int	unused;
    char		semid[IPC_KEYLEN]; 
    char		buf[512];
    FILE		*fp;
    int			sts, needsave = 0;

    pmdaCacheOp(sem_indom, PMDA_CACHE_INACTIVE);

    if ((fp = linux_statsfile("/proc/sysvipc/sem", buf, sizeof(buf))) == NULL)
	return -oserror();

    /* skip header, then iterate over each semaphore line adding to cache */
    if (fgets(buf, sizeof(buf), fp) != NULL) {
	/* key semid perms nsems uid gid cuid cgid [otime ctime] */
	while (fscanf(fp, "%d %u %o %u %u %u %u %u %llu %llu",
			&sbuf.key, &sbuf.id, &sbuf.perms, &sbuf.nsems,
			&sbuf.uid, &unused, &unused, &unused,
			&unusedll, &unusedll) >= 5) {
	    pmsprintf(semid, sizeof(semid), "%d", sbuf.id);
	    sts = pmdaCacheLookupName(sem_indom, semid, NULL, (void **)&semp);
	    if (sts == PMDA_CACHE_ACTIVE)
		continue;
	    if (sts != PMDA_CACHE_INACTIVE)
		needsave = 1;
	    if (sts != PMDA_CACHE_INACTIVE &&
		(semp = (sem_array_t *)calloc(1, sizeof(sem_array_t))) == NULL)
		continue;

	    extract_owner(sbuf.uid, &sbuf.owner[0]);
	    extract_perms(sbuf.perms, &sbuf.perms);
	    pmsprintf(sbuf.keyid, IPC_KEYLEN, "0x%08x", sbuf.key); 
	    memcpy(semp, &sbuf, sizeof(sem_array_t));

	    sts = pmdaCacheStore(sem_indom, PMDA_CACHE_ADD, semid, (void *)semp);
	    if (sts < 0) {
		fprintf(stderr, "Warning: %s: pmdaCacheStore(%s, %s): %s\n",
			__FUNCTION__, semid, semp->keyid, pmErrStr(sts));
		free(semp);
	    }	
	}
    }
    fclose(fp);

    if (needsave)
	pmdaCacheOp(sem_indom, PMDA_CACHE_SAVE);
    return 0;
}
