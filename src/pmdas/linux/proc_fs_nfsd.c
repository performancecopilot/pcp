/*
 * Linux /proc/fs/nfsd metrics cluster
 *
 * Copyright (c) 2017 Red Hat.
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
#include "proc_fs_nfsd.h"

int
refresh_proc_fs_nfsd(proc_fs_nfsd_t *proc_fs_nfsd)
{
    static int err_reported;
    char buf[MAXPATHLEN];
    FILE *statsp = NULL;
    FILE *threadsp = NULL;

    memset(proc_fs_nfsd, 0, sizeof(proc_fs_nfsd_t));

    if ((threadsp = linux_statsfile("/proc/fs/nfsd/pool_threads",
				    buf, sizeof(buf))) == NULL) {
	proc_fs_nfsd->errcode = -oserror();
	if (pmDebugOptions.libpmda) {
	    if (err_reported == 0)
		fprintf(stderr, "Warning: nfsd thread metrics are not available : %s\n",
			osstrerror());
	}
    }
    else {
	proc_fs_nfsd->errcode = 0;
	if (fscanf(threadsp,  "%d", &proc_fs_nfsd->th_cnt) != 1)
	    proc_fs_nfsd->errcode = PM_ERR_VALUE;

	/* pool_stats is only valid if nfsd is running */
	if (proc_fs_nfsd->th_cnt > 0) {
	    if ((statsp = linux_statsfile("/proc/fs/nfsd/pool_stats",
					    buf, sizeof(buf))) == NULL) {
	        proc_fs_nfsd->errcode = -oserror();
	        if (err_reported == 0)
	            fprintf(stderr, "Error: missing pool_stats when thread count != 0 : %s\n",
	  	            osstrerror());
            } else {
	    	unsigned int poolid;
	    	unsigned long arrived, enqueued, woken, timedout;
	    	int ret;

		/* first line is headers, read and discard */
	    	if (fscanf(statsp, "#%*[^\n]\n") != 0)
			fprintf(stderr, "Error: parsing /proc/fs/nfsd/pool_stats headers: %s\n", 
				osstrerror());

	    	/* default is one pool, but there might be more.
	     	 * aggregate all the pool stats */
	    	while ((ret=fscanf(statsp,  "%u %lu %lu %lu %lu",
			&poolid, &arrived, &enqueued,
			&woken, &timedout)) == 5) {

		    /* poolid is not important, just count them */
		    proc_fs_nfsd->pool_cnt++;
		    proc_fs_nfsd->pkts_arrived += arrived;
		    proc_fs_nfsd->sock_enqueued += enqueued;
		    proc_fs_nfsd->th_woken += woken;
		    proc_fs_nfsd->th_timedout += timedout;
	    	}
	    	if (proc_fs_nfsd->pool_cnt < 1)
	            proc_fs_nfsd->errcode = PM_ERR_VALUE;
	     }
	}
    }

	if (pmDebugOptions.libpmda) {
	    if (proc_fs_nfsd->errcode == 0)
		fprintf(stderr, "refresh_proc_fs_nfsd: found nfsd thread metrics\n");
	    else
		fprintf(stderr, "refresh_proc_fs_nfsd: botch! missing nfsd thread metrics\n");
	}

    if (threadsp)
	fclose(threadsp);
    if (statsp)
	fclose(statsp);

    if (!err_reported)
	err_reported = 1;

    if (proc_fs_nfsd->errcode == 0)
	return 0;
    return -1;
}
