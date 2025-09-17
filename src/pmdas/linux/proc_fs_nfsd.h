/* 
 * Linux /proc/fs/nfsd metrics cluster
 *
 * Copyright (c) 2017-2025, Red Hat.
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
    int	errcode;	/* error from previous refresh */

	/* /proc/fs/nfsd/pool_threads */
	unsigned int    th_cnt;         /* total threads */

	/* /proc/fs/nfsd/pool_stats */
	unsigned int	pool_cnt;	/* how many thread pools did we see */
	unsigned long   pkts_arrived;   /* count of total requests received */
	unsigned long   sock_enqueued;  /* times request processing delayed */
	unsigned long   th_woken;       /* times request processed immediately */
	unsigned long   th_timedout;    /* threads timed out as unused */

} proc_fs_nfsd_t;

extern int refresh_proc_fs_nfsd(proc_fs_nfsd_t *);

enum {
    NFS4_CLIENT_CLIENT_ID = 0,
    NFS4_CLIENT_ADDR,
    NFS4_CLIENT_STATUS,
    NFS4_CLIENT_HOSTNAME,
    NFS4_CLIENT_CALLBACK_STATE,
    NFS4_CLIENT_CALLBACK_ADDR,
    NFS4_CLIENT_ADMIN_REVOKED_STATES,
    NFS4_CLIENT_SESSION_SLOTS,
    NFS4_CLIENT_SESSION_TARGET_SLOTS,
    NUM_NFS4_CLIENT_STATS
};

enum {
    NFS4_CLIENT_OPENS_INODE = 0,
    NFS4_CLIENT_OPENS_TYPE,
    NFS4_CLIENT_OPENS_ACCESS,
    NFS4_CLIENT_OPENS_FILENAME,
    NFS4_CLIENT_OPENS_CLIENT_ID,
    NFS4_CLIENT_OPENS_CLIENT_ADDR,
    NFS4_CLIENT_OPENS_CLIENT_HOSTNAME,
    NUM_NFS4_CLIENT_OPENS_STATS
};

typedef struct {
	/* /proc/fs/nfsd/<client>/info */
	char			client_id[19];
	char			client_addr[52];
	char			status[16];
	char			hostname[254];
	char			callback_state[16];
	char			callback_addr[52];
	unsigned long long	admin_revoked_states;
	unsigned long long	session_slots;
	unsigned long long	session_target_slots;
} nfs4_svr_client_t;

typedef struct {
	/* /proc/fs/nfsd/<client>/states */
	unsigned long long	inode;
	char			type[7];
	char			access[16];
	char			filename[256];
	char			client_id[19];
	char			client_addr[52];
	char			client_hostname[254];
} nfs4_svr_open_t;

extern int nfs4_svr_client_fetch(int, nfs4_svr_client_t *, pmAtomValue *);
extern int refresh_nfs4_svr_client(pmInDom);

extern int nfs4_svr_client_opens_fetch(int, nfs4_svr_open_t *, pmAtomValue *);
extern int refresh_nfs4_svr_client_opens(pmInDom);
