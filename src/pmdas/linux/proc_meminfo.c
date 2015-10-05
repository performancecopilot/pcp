/*
 * Linux /proc/meminfo metrics cluster
 *
 * Copyright (c) 2013-2015 Red Hat.
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

#include <ctype.h>
#include "pmapi.h"
#include "pmda.h"
#include "indom.h"
#include <sys/stat.h>
#include "proc_meminfo.h"

static proc_meminfo_t moff;
extern size_t _pm_system_pagesize;

static struct {
    char	*field;
    int64_t	*offset;
} meminfo_fields[] = {
    { "MemTotal",	&moff.MemTotal },
    { "MemFree",	&moff.MemFree },
    { "MemAvailable",	&moff.MemAvailable },
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
    { "Hugepagesize",	&moff.Hugepagesize },
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
    char	buf[1024];
    char	*bufp;
    int64_t	*p;
    int		i;
    FILE	*fp;

    for (i = 0; meminfo_fields[i].field != NULL; i++) {
	p = MOFFSET(i, proc_meminfo);
	*p = -1; /* marked as "no value available" */
    }

    if ((fp = linux_statsfile("/proc/meminfo", buf, sizeof(buf))) == NULL)
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

    /*
     * MemAvailable is only in 3.x or later kernels but we can calculate it
     * using other values, similar to upstream kernel commit 34e431b0ae.
     * The environment variable is for QA purposes.
     */
    if (!MEMINFO_VALID_VALUE(proc_meminfo->MemAvailable) ||
    	getenv("PCP_QA_ESTIMATE_MEMAVAILABLE") != NULL) {
	if (MEMINFO_VALID_VALUE(proc_meminfo->MemTotal) &&
	    MEMINFO_VALID_VALUE(proc_meminfo->MemFree) &&
	    MEMINFO_VALID_VALUE(proc_meminfo->Active_file) &&
	    MEMINFO_VALID_VALUE(proc_meminfo->Inactive_file) &&
	    MEMINFO_VALID_VALUE(proc_meminfo->SlabReclaimable)) {

	    int64_t pagecache;
	    int64_t wmark_low = 0;

	    /*
	     * sum for each zone->watermark[WMARK_LOW];
	     */
	    if ((fp = fopen("/proc/zoneinfo", "r")) != NULL) {
		while (fgets(buf, sizeof(buf), fp) != NULL) {
		    if ((bufp = strstr(buf, "low ")) != NULL) {
			int64_t low;
		    	if (sscanf(bufp+4, "%lld", (long long int *)&low) == 1)
			    wmark_low += low;
		    }
		}
		fclose(fp);
		wmark_low *= _pm_system_pagesize;
	    }

	    /*  
	     * Free memory cannot be taken below the low watermark, before the
	     * system starts swapping.
	     */
	    proc_meminfo->MemAvailable = proc_meminfo->MemFree - wmark_low;

	    /*
	     * Not all the page cache can be freed, otherwise the system will
	     * start swapping. Assume at least half of the page cache, or the
	     * low watermark worth of cache, needs to stay.
	     */
	    pagecache = proc_meminfo->Active_file + proc_meminfo->Inactive_file;
	    pagecache -= MIN(pagecache / 2, wmark_low);
	    proc_meminfo->MemAvailable += pagecache;

	    /*
	     * Part of the reclaimable slab consists of items that are in use,
	     * and cannot be freed. Cap this estimate at the low watermark.
	     */
	    proc_meminfo->MemAvailable += proc_meminfo->SlabReclaimable;
	    proc_meminfo->MemAvailable -= MIN(proc_meminfo->SlabReclaimable / 2, wmark_low);

	    if (proc_meminfo->MemAvailable < 0)
	    	proc_meminfo->MemAvailable = 0;
	}
    }

    /* success */
    return 0;
}
