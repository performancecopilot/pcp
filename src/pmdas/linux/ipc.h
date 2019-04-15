/*
 * Copyright (C) 2015-2016,2019 Red Hat.
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
#define SHM_DEST	01000	/* segment will be destroyed on last detach */
#define SHM_LOCKED	02000	/* segment will not be swapped */

/*
 * X/OPEN (Jan 1987) does not define fields key, seq in struct ipc_perm;
 * glibc-1.09 has no support for SYSV IPC.
 * glibc 2 uses __key, __seq
 */
#if defined (__GLIBC__) && __GLIBC__ >= 2
# define KEY __key
#else
# define KEY key
#endif

typedef struct {
    unsigned int	shm_tot; /* total allocated shm */
    unsigned int	shm_rss; /* total resident shm */
    unsigned int	shm_swp; /* total swapped shm */
    unsigned int	used_ids; /* currently existing segments */
    unsigned int	swap_attempts; /* the count swap attempts */
    unsigned int	swap_successes; /* the count swap successes */
} shm_info_t;

extern int refresh_shm_info(shm_info_t *);

typedef struct {
    unsigned int	shmmax; /* maximum shared segment size (bytes) */
    unsigned int	shmmin; /* minimum shared segment size (bytes) */
    unsigned int	shmmni; /* maximum number of segments system wide */
    unsigned int	shmseg; /* maximum shared segments per process */
    unsigned int	shmall; /* maximum shared memory system wide (pages) */
} shm_limits_t;

extern int refresh_shm_limits(shm_limits_t *);

#ifdef _SEM_SEMUN_UNDEFINED
/* glibc 2.1 no longer defines semun, instead it defines
 * _SEM_SEMUN_UNDEFINED so users can define semun on their own.
 */
union semun {
    int			val;
    struct semid_ds	*buf;
    unsigned short int	*array;
    struct seminfo	*__buf;
};
#endif

typedef struct {
    unsigned int	semusz; /* the number of semaphore sets that
	                         * currently exist on the system */
    unsigned int	semaem; /* total number of semaphores
	                         * in all semaphore sets on the system */
} sem_info_t;

extern int refresh_sem_info(sem_info_t *);

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
    unsigned int	msgpool; /* the number of message queues 
	                          * that currently exist on the system */
    unsigned int	msgmap;  /* the total number of messages 
	                          * in all queues on the system */
    unsigned int	msgtql;  /* the total number of bytes in all 
	                          * messages in all queues on the system */
} msg_info_t;

extern int refresh_msg_info(msg_info_t *);

typedef struct {
    unsigned int	msgpool; /* size of message pool (kbytes) */
    unsigned int	msgmap;  /* # of entries in message map */
    unsigned int	msgmax;  /* maximum size of a message */
    unsigned int	msgmnb;  /* default maximum size of message queue */
    unsigned int	msgmni;  /* maximum # of message queue identifiers */
    unsigned int	msgssz;  /* message segment size */
    unsigned int	msgtql;  /* # of system message headers */
    unsigned int	msgseg;  /* maximum # of message segments */
} msg_limits_t;

extern int refresh_msg_limits(msg_limits_t *);

#define IPC_KEYLEN	16
#define IPC_OWNERLEN	128

typedef struct {
    unsigned int	id;			/* id of shm slot */
    unsigned int	key;			/* name of this shm slot */
    char		keyid[IPC_KEYLEN];	/* hex formatted slot name */
    char		owner[IPC_OWNERLEN];	/* username of owner */
    unsigned int	uid;			/* uid of owner */
    unsigned int	perms;			/* access permissions */
    unsigned long long	bytes;			/* segment size in bytes */
    unsigned int	cpid;			/* creator PID */
    unsigned int	lpid;			/* last access PID */
    unsigned int	nattach;		/* no. of current attaches */
    unsigned int	dest : 1;		/* destruct flag is set */
    unsigned int	locked : 1;		/* locked flag is set */
} shm_stat_t;

extern int refresh_shm_stat(pmInDom);

typedef struct {
    unsigned int	id;			/* id of messages slot */
    unsigned int	key;			/* name of these messages slot */
    char		keyid[IPC_KEYLEN];	/* hex formatted slot name */
    char		owner[IPC_OWNERLEN];	/* username of owner */
    unsigned int	uid;			/* uid of owner */
    unsigned int	perms;			/* access permissions */
    unsigned int	bytes;			/* used size in bytes */
    unsigned int	messages;		/* no. of messages */
    unsigned int	lspid;			/* last send PID */
    unsigned int	lrpid;			/* last recv PID */
} msg_queue_t;

extern int refresh_msg_queue(pmInDom);

typedef struct {
    unsigned int	id;			/* id of semaphore slot */
    unsigned int	key;			/* name of this semaphore slot */
    char		keyid[IPC_KEYLEN];	/* hex formatted slot name */
    char		owner[IPC_OWNERLEN];	/* username of owner */
    unsigned int	uid;			/* uid of owner */
    unsigned int	perms;			/* access permissions */
    unsigned int	nsems;			/* no. of semaphore */
} sem_array_t;

extern int refresh_sem_array(pmInDom);
