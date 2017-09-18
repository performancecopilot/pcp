/*
 * Linux /proc/sys/kernel metrics cluster
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
#include "proc_sys_kernel.h"

#define ENTROPY_AVAILABLE "/proc/sys/kernel/random/entropy_avail"
#define ENTROPY_POOLSIZE  "/proc/sys/kernel/random/poolsize"

int
refresh_proc_sys_kernel(proc_sys_kernel_t *proc_sys_kernel)
{
    static int err_reported;
    char buf[MAXPATHLEN];
    FILE *eavail = NULL;
    FILE *poolsz = NULL;

    memset(proc_sys_kernel, 0, sizeof(proc_sys_kernel_t));

    if ((eavail = linux_statsfile(ENTROPY_AVAILABLE, buf, sizeof(buf))) == NULL ||
	(poolsz = linux_statsfile(ENTROPY_POOLSIZE, buf, sizeof(buf))) == NULL) {
	proc_sys_kernel->errcode = -oserror();
	if (err_reported == 0)
	    fprintf(stderr, "Warning: entropy metrics are not available : %s\n",
		    osstrerror());
    }
    else {
	proc_sys_kernel->errcode = 0;
	if (fscanf(eavail, "%u", &proc_sys_kernel->entropy_avail) != 1)
	    proc_sys_kernel->errcode = PM_ERR_VALUE;
	if (fscanf(poolsz, "%u", &proc_sys_kernel->random_poolsize) != 1)
	    proc_sys_kernel->errcode = PM_ERR_VALUE;
	if (pmDebugOptions.libpmda) {
	    if (proc_sys_kernel->errcode == 0)
		fprintf(stderr, "refresh_proc_sys_kernel: found entropy metrics\n");
	    else
		fprintf(stderr, "refresh_proc_sys_kernel: botch! missing entropy metrics\n");
	}
    }
    if (eavail)
	fclose(eavail);
    if (poolsz)
	fclose(poolsz);

    if (!err_reported)
	err_reported = 1;

    if (proc_sys_kernel->errcode == 0)
	return 0;
    return -1;
}
