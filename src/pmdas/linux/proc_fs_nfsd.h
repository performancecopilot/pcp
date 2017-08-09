/* 
 * Linux /proc/fs/nfsd metrics cluster
 *
 * Copyright (c) 2017, Red Hat.
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
