/*
 * Linux /proc/meminfo metrics cluster
 *
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#include "pmapi.h"
#include "proc_meminfo.h"

static proc_meminfo_t moff;

static struct {
    char	*field;
    int64_t	*offset;
} meminfo_fields[] = {
    { "MemTotal",	&moff.MemTotal },
    { "MemFree",	&moff.MemFree },
    { "MemShared",	&moff.MemShared },
    { "Buffers",	&moff.Buffers },
    { "Cached",		&moff.Cached },
    { "SwapCached",	&moff.SwapCached },
    { "Active",		&moff.Active },
    { "Inactive",	&moff.Inactive },
    { "Active(anon)",	&moff.Active_anon },
    { "Inactive(anon)", &moff.Inactive_anon },
    { "Active(file)",   &moff.Active_file },
    { "Inactive(file)", &moff.Inactive_file },
    { "Unevictable",	&moff.Unevictable },
    { "Mlocked",	&moff.Mlocked },
    { "HighTotal",	&moff.HighTotal },
    { "HighFree",	&moff.HighFree },
    { "LowTotal",	&moff.LowTotal },
    { "LowFree",	&moff.LowFree },
    { "MmapCopy",	&moff.MmapCopy },
    { "SwapTotal",	&moff.SwapTotal },
    { "SwapFree",	&moff.SwapFree },
    { "Dirty",		&moff.Dirty },
    { "Writeback",	&moff.Writeback },
    { "AnonPages",	&moff.AnonPages },
    { "Mapped",		&moff.Mapped },
    { "Shmem",		&moff.Shmem },
    { "Slab",		&moff.Slab },
    { "SReclaimable",	&moff.SlabReclaimable },
    { "SUnreclaim",	&moff.SlabUnreclaimable },
    { "KernelStack",	&moff.KernelStack },
    { "PageTables",	&moff.PageTables },
    { "Quicklists",	&moff.Quicklists },
    { "NFS_Unstable",	&moff.NFS_Unstable },
    { "Bounce",		&moff.Bounce },
    { "WritebackTmp",	&moff.WritebackTmp },
    { "CommitLimit",	&moff.CommitLimit },
    { "Committed_AS",	&moff.Committed_AS },
    { "VmallocTotal",	&moff.VmallocTotal },
    { "VmallocUsed",	&moff.VmallocUsed },
    { "VmallocChunk",	&moff.VmallocChunk },
    { "HardwareCorrupted", &moff.HardwareCorrupted },
    { "AnonHugePages",	&moff.AnonHugePages },
    /* vendor kernel patches, some outdated now */
    { "MemShared",	&moff.MemShared },
    { "ReverseMaps",	&moff.ReverseMaps },
    { "HugePages_Total", &moff.HugepagesTotal },
    { "HugePages_Free",	&moff.HugepagesFree },
    { "HugePages_Rsvd",	&moff.HugepagesRsvd },
    { "HugePages_Surp",	&moff.HugepagesSurp },
    { "DirectMap4k",	&moff.directMap4k },
    { "DirectMap2M",	&moff.directMap2M },
    { "DirectMap1G",	&moff.directMap1G },
    { NULL, NULL }
};

#define MOFFSET(ii, pp) (int64_t *)((char *)pp + \
    (__psint_t)meminfo_fields[ii].offset - (__psint_t)&moff)

int
refresh_proc_meminfo(proc_meminfo_t *proc_meminfo)
{
    static int	started;
    char	buf[1024];
    char	*bufp;
    int64_t	*p;
    int		i;
    FILE	*fp;

    if (!started) {
	started = 1;
	memset(proc_meminfo, 0, sizeof(*proc_meminfo));
    }

    for (i=0; meminfo_fields[i].field != NULL; i++) {
	p = MOFFSET(i, proc_meminfo);
	*p = -1; /* marked as "no value available" */
    }

    if ((fp = fopen("/proc/meminfo", "r")) == (FILE *)0)
	return -oserror();

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((bufp = strchr(buf, ':')) == NULL)
	    continue;
	*bufp = '\0';
	for (i=0; meminfo_fields[i].field != NULL; i++) {
	    if (strcmp(buf, meminfo_fields[i].field) != 0)
		continue;
	    p = MOFFSET(i, proc_meminfo);
	    for (bufp++; *bufp; bufp++) {
	    	if (isdigit((int)*bufp)) {
		    sscanf(bufp, "%llu", (unsigned long long *)p);
		    *p *= 1024; /* kbytes -> bytes */
		    break;
		}
	    }
	}
    }

    fclose(fp);

    /* success */
    return 0;
}
