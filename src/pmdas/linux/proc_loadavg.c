/*
 * Linux /proc/loadavg metrics cluster
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

#include "pmapi.h"
#include "pmda.h"
#include "indom.h"
#include "proc_loadavg.h"

int
refresh_proc_loadavg(proc_loadavg_t *proc_loadavg)
{
    char buf[1024];
    FILE *fp;
    int sts = 0;

    if ((fp = linux_statsfile("/proc/loadavg", buf, sizeof(buf))) == NULL)
	return -oserror();

    if (fgets(buf, sizeof(buf), fp) == NULL)
	sts = -oserror();
    fclose(fp);

    if (sts == 0) {
	/*
	 * 0.00 0.00 0.05 1/67 17563
	 * Lastpid added by Mike Mason <mmlnx@us.ibm.com>
	 */
	sscanf((const char *)buf, "%f %f %f %u/%u %u",
		&proc_loadavg->loadavg[0], &proc_loadavg->loadavg[1], 
		&proc_loadavg->loadavg[2], &proc_loadavg->runnable,
		&proc_loadavg->nprocs, &proc_loadavg->lastpid);
    }
    return sts;
}
