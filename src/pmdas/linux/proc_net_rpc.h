/*
 * Linux /proc/net/rpc metrics cluster
 *
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#define NR_RPC_COUNTERS		18
#define NR_RPC3_COUNTERS	22
#define NR_RPC4_CLI_COUNTERS	53
#define NR_RPC4_SVR_COUNTERS	59

typedef struct {
    struct {
	int		errcode;	/* error from last refresh */
	/* /proc/net/rpc/nfs  "net" */
	unsigned int	netcnt;
	unsigned int	netudpcnt;
	unsigned int	nettcpcnt;
	unsigned int	nettcpconn;

	/* /proc/net/rpc/nfs  "rpc" */
	unsigned int	rpccnt;
	unsigned int	rpcretrans;
	unsigned int	rpcauthrefresh;

	/* /proc/net/rpc/nfs  "proc2" */
	unsigned int reqcounts[NR_RPC_COUNTERS];

	/* /proc/net/rpc/nfs  "proc3" */
	unsigned int reqcounts3[NR_RPC3_COUNTERS];

	/* /proc/net/rpc/nfs  "proc4" */
	unsigned int reqcounts4[NR_RPC4_CLI_COUNTERS];
    } client;

    struct {
	int		errcode;	/* error from last refresh */
	/* /proc/net/rpc/nfsd "rc" and "fh" */
        unsigned int    rchits;         /* repcache hits */
        unsigned int    rcmisses;       /* repcache hits */
        unsigned int    rcnocache;      /* uncached reqs */
        unsigned int    fh_cached;      /* dentry cached */
        unsigned int    fh_valid;       /* dentry validated */
        unsigned int    fh_fixup;       /* dentry fixup validated */
        unsigned int    fh_lookup;      /* new lookup required */
        unsigned int    fh_stale;       /* FH stale error */
        unsigned int    fh_concurrent;  /* concurrent request */
	unsigned int    fh_anon;	/* anon file dentry returned */
	unsigned int	fh_nocache_dir;	/* dir filehandle not found in dcache */
	unsigned int	fh_nocache_nondir; /* nondir filehandle not in dcache */

	/* /proc/net/rpc/nfsd "io" */
	unsigned int	io_read;	/* bytes returned to read requests */
	unsigned int	io_write;	/* bytes passed in write requests */

	/* /proc/net/rpc/nfsd "th" */
	unsigned int	th_cnt;		/* available nfsd threads */
	unsigned int	th_fullcnt;	/* times last free thread used */
	/* float	th_usage[10];   -- always zero in modern kernels */

        /* /proc/net/rpc/nfsd "ra" */
	unsigned int	ra_size;	/* cache size */
	unsigned int	ra_hits;	/* sum of individual ra_depth counts */
	/* unsigned int	ra_depth[10];	- entry found at hash bucket depth */
	/* could create an indom for this, but setting aside at this stage */
	unsigned int	ra_misses;	/* not found in read-ahead cache */

	/* /proc/net/rpc/nfsd "net" */
	unsigned int	netcnt;
	unsigned int	netudpcnt;
	unsigned int	nettcpcnt;
	unsigned int	nettcpconn;

	/* /proc/net/rpc/nfsd "rpc" */
	unsigned int	rpccnt;
	unsigned int	rpcerr;
	unsigned int	rpcbadfmt;
	unsigned int	rpcbadauth;
	unsigned int	rpcbadclnt;

	/* /proc/net/rpc/nfsd "proc2" */
	unsigned int reqcounts[NR_RPC_COUNTERS];

	/* /proc/net/rpc/nfsd  "proc3" */
	unsigned int reqcounts3[NR_RPC3_COUNTERS];

	/* /proc/net/rpc/nfsd  "proc4" & "proc4ops" */
	unsigned int reqcounts4[NR_RPC4_SVR_COUNTERS+1];
	/* Note: the +1 is to deal with a quirk of the kernel code */

    } server;

} proc_net_rpc_t;

extern int refresh_proc_net_rpc(proc_net_rpc_t *);
