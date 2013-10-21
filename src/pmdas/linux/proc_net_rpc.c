/*
 * Linux /proc/net_rpc metrics cluster
 *
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

#include "pmapi.h"
#include "proc_net_rpc.h"

int
refresh_proc_net_rpc(proc_net_rpc_t *proc_net_rpc)
{
    char buf[4096];
    FILE *fp;
    char *p;
    int i;

    memset(proc_net_rpc, 0, sizeof(proc_net_rpc_t));

    /*
     * client stats
     */
    if ((fp = fopen("/proc/net/rpc/nfs", "r")) == (FILE *)NULL) {
    	proc_net_rpc->client.errcode = -oserror();
    }
    else {
    	proc_net_rpc->client.errcode = 0;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
	    if (strncmp(buf, "net", 3) == 0)
		sscanf(buf, "net %u %u %u %u", 
		    &proc_net_rpc->client.netcnt,
		    &proc_net_rpc->client.netudpcnt,
		    &proc_net_rpc->client.nettcpcnt,
		    &proc_net_rpc->client.nettcpconn);
	    else
	    if (strncmp(buf, "rpc", 3) == 0)
		sscanf(buf, "rpc %u %u %u", 
		    &proc_net_rpc->client.rpccnt,
		    &proc_net_rpc->client.rpcretrans,
		    &proc_net_rpc->client.rpcauthrefresh);
	    else
	    if (strncmp(buf, "proc2", 5) == 0) {
		if ((p = strtok(buf, " ")) != NULL)
		    p = strtok(NULL, " ");
		for (i=0; p && i < NR_RPC_COUNTERS; i++) {
		    if ((p = strtok(NULL, " ")) == NULL)
			break;
		    proc_net_rpc->client.reqcounts[i] = strtoul(p, (char **)NULL, 10);
		}
	    }
	    else
	    if (strncmp(buf, "proc3", 5) == 0) {
		if ((p = strtok(buf, " ")) != NULL)
		    p = strtok(NULL, " ");
		for (i=0; p && i < NR_RPC3_COUNTERS; i++) {
		    if ((p = strtok(NULL, " ")) == NULL)
			break;
		    proc_net_rpc->client.reqcounts3[i] = strtoul(p, (char **)NULL, 10);
		}
	    }
	    else
	    if (strncmp(buf, "proc4", 5) == 0) {
		if ((p = strtok(buf, " ")) != NULL)
		    p = strtok(NULL, " ");
		for (i=0; p && i < NR_RPC4_CLI_COUNTERS; i++) {
		    if ((p = strtok(NULL, " ")) == NULL)
			break;
		    proc_net_rpc->client.reqcounts4[i] = strtoul(p, (char **)NULL, 10);
		}
	    }
	}

	fclose(fp);
    }

    /*
     * server stats
     */
    if ((fp = fopen("/proc/net/rpc/nfsd", "r")) == (FILE *)NULL) {
    	proc_net_rpc->server.errcode = -oserror();
    }
    else {
	proc_net_rpc->server.errcode = 0;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
	    if (strncmp(buf, "rc", 2) == 0)
		sscanf(buf, "rc %u %u %u %u %u %u %u %u %u",
		    &proc_net_rpc->server.rchits,
		    &proc_net_rpc->server.rcmisses,
		    &proc_net_rpc->server.rcnocache,
		    &proc_net_rpc->server.fh_cached,
		    &proc_net_rpc->server.fh_valid,
		    &proc_net_rpc->server.fh_fixup,
		    &proc_net_rpc->server.fh_lookup,
		    &proc_net_rpc->server.fh_stale,
		    &proc_net_rpc->server.fh_concurrent);
	    else
	    if (strncmp(buf, "fh", 2) == 0)
		sscanf(buf, "fh %u %u %u %u %u",
		    &proc_net_rpc->server.fh_stale,
		    &proc_net_rpc->server.fh_lookup,
		    &proc_net_rpc->server.fh_anon,
		    &proc_net_rpc->server.fh_nocache_dir,
		    &proc_net_rpc->server.fh_nocache_nondir);
	    else
	    if (strncmp(buf, "io", 2) == 0)
		sscanf(buf, "io %u %u",
		    &proc_net_rpc->server.io_read,
		    &proc_net_rpc->server.io_write);
	    else
	    if (strncmp(buf, "th", 2) == 0)
		sscanf(buf, "th %u %u",
		    &proc_net_rpc->server.th_cnt,
		    &proc_net_rpc->server.th_fullcnt);
	    else
	    if (strncmp(buf, "net", 3) == 0)
		sscanf(buf, "net %u %u %u %u", 
		    &proc_net_rpc->server.netcnt,
		    &proc_net_rpc->server.netudpcnt,
		    &proc_net_rpc->server.nettcpcnt,
		    &proc_net_rpc->server.nettcpconn);
	    else
	    if (strncmp(buf, "rpc", 3) == 0)
		sscanf(buf, "rpc %u %u %u", 
		    &proc_net_rpc->server.rpccnt,
		    &proc_net_rpc->server.rpcerr, /* always the sum of the following three fields */
		    &proc_net_rpc->server.rpcbadfmt);
	    else
	    if (strncmp(buf, "proc2", 5) == 0) {
		if ((p = strtok(buf, " ")) != NULL)
		    p = strtok(NULL, " ");
		for (i=0; p && i < NR_RPC_COUNTERS; i++) {
		    if ((p = strtok(NULL, " ")) == NULL)
			break;
		    proc_net_rpc->server.reqcounts[i] = strtoul(p, (char **)NULL, 10);
		}
	    }
	    else
	    if (strncmp(buf, "proc3", 5) == 0) {
		if ((p = strtok(buf, " ")) != NULL)
		    p = strtok(NULL, " ");
		for (i=0; p && i < NR_RPC3_COUNTERS; i++) {
		    if ((p = strtok(NULL, " ")) == NULL)
			break;
		    proc_net_rpc->server.reqcounts3[i] = strtoul(p, (char **)NULL, 10);
		}
	    }
	    else
	    if (strncmp(buf, "proc4ops", 8) == 0) {
		if ((p = strtok(buf, " ")) != NULL)
		    p = strtok(NULL, " ");

		/* Inst 0 is NULL count (below) */
		for (i=1; p && i < NR_RPC4_SVR_COUNTERS; i++) {
		    if ((p = strtok(NULL, " ")) == NULL)
			break;
		    proc_net_rpc->server.reqcounts4[i] = strtoul(p, (char **)NULL, 10);
		}
	    }
	    else
	    if (strncmp(buf, "proc4", 5) == 0) {
		if ((strtok(buf, " ")) != NULL &&
		    (strtok(NULL, " ")) != NULL &&
		    (p = strtok(NULL, " ")) != NULL) { /* 3rd token is NULL count */
		    proc_net_rpc->server.reqcounts4[0] = strtoul(p, (char **)NULL, 10);
		}
	    }
	}

	fclose(fp);
    }

    if (proc_net_rpc->client.errcode == 0 && proc_net_rpc->server.errcode == 0)
    	return 0;
    return -1;
}
