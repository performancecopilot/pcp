/*
 * Linux /sys/{kernel/mm,devices/system/node/nodeN}/hugepages clusters
 *
 * Copyright (c) 2024-2025, Red Hat.
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
#include "linux.h"
#include "proc_stat.h"
#include "sysfs_hugepages.h"

static const char *hugepage_fields[] = {
    [PAGESIZE_HUGEPAGES] = "", /* directory name */
    [FREE_HUGEPAGES] = "free_hugepages",
    [RESV_HUGEPAGES] = "resv_hugepages",
    [SURPLUS_HUGEPAGES] = "surplus_hugepages",
    [TOTALSIZE_HUGEPAGES] = "nr_hugepages",
    [OVERCOMMIT_HUGEPAGES] = "nr_overcommit",
    [HUGEPAGES_METRIC_COUNT] = NULL
};

static const char *numa_hugepage_fields[] = {
    [PAGESIZE_NUMA_HUGEPAGES] = "", /* directory name */
    [FREE_NUMA_HUGEPAGES] = "free_hugepages",
    [SURPLUS_NUMA_HUGEPAGES] = "surplus_hugepages",
    [TOTALSIZE_NUMA_HUGEPAGES] = "nr_hugepages",
    [NUMA_HUGEPAGES_METRIC_COUNT] = NULL
};

/*
 * Scans a single directory for different hugepage size stats files.
 * Returns negative code on error, else 0/1 indicating whether the
 * instance domain has changed (and hence needs to be persisted).
 */
static int
scan_sysfs_hugepages_dir(const char *sysname, DIR *sysdir, pmInDom indom,
		const char *node, const char *fields[], unsigned int count)
{
    struct dirent *sysentry, *hugepages;
    char statsname[MAXPATHLEN];
    char statsfile[MAXPATHLEN];
    char strvalue[64], *iname;
    DIR *hugepagesdir;
    uint64_t *hugepage;
    unsigned long long pagesize;
    int i, sts, fd, needsave = 0;

    while ((sysentry = readdir(sysdir)) != NULL) {
	char *size = sysentry->d_name;

	if (size[0] == '.')
	    continue;
	if (sscanf(size, "hugepages-%llukB", &pagesize) != 1)
	    continue;

	/* look up this size in the cache, add it if not already present. */
	iname = size + 10;
	if (node) { /* add optional NUMA node prefix */
	    pmsprintf(statsname, sizeof(statsname), "%s::%s", node, iname);
	    iname = statsname;
	}
	hugepage = NULL;
	sts = pmdaCacheLookupName(indom, iname, NULL, (void **)&hugepage);
	if (sts < 0 || hugepage == NULL) {
	    /* new hugepage size */
	    if (!(hugepage = (uint64_t *)calloc(count, sizeof(uint64_t)))) {
		sts = -oserror();
		return sts;
	    }
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "%s: added new hugepage size \"%s\"\n",
				__FUNCTION__, iname);
	    needsave = 1;
	}
	pmdaCacheStore(indom, PMDA_CACHE_ADD, iname, (void *)hugepage);

	/* now update the stats for the new or existing hugepage size */
	hugepage[0] = pagesize;	/* pagesize is always the first array entry */

	pmsprintf(statsname, sizeof(statsname), "%s/%s", sysname, size);
	if ((hugepagesdir = opendir(statsname)) == NULL)
	    continue;

	while ((hugepages = readdir(hugepagesdir)) != NULL) {
	    ssize_t n;
	    char *h = hugepages->d_name;
	    int hlen = strlen(h);

	    if (h[0] == '.')
	    	continue;
	    pmsprintf(statsfile, sizeof(statsfile), "%s/%s", statsname, h);
	    if ((fd = open(statsfile, O_RDONLY)) < 0)
	    	continue;
	    if ((n = read(fd, strvalue, sizeof(strvalue)-1)) <= 0) {
		close(fd);
	    	continue;
	    }
	    strvalue[n] = '\0';
	    for (i=0; i < count; i++) {
		if (strncmp(fields[i], h, hlen) == 0) {
		    hugepage[i] = strtoull(strvalue, NULL, 0);
		    break;
		}
	    }
	    close(fd);
	}
	closedir(hugepagesdir);
    }

    return needsave;
}

int
refresh_sysfs_hugepages(pmInDom indom)
{
    static int setup;
    char sysname[MAXPATHLEN];
    DIR *sysdir;
    int sts;

    if (!setup) {
	pmdaCacheOp(indom, PMDA_CACHE_LOAD);
	setup = 1;
    }

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    pmsprintf(sysname, sizeof(sysname), "%s/sys/kernel/mm/hugepages", linux_statspath);
    if ((sysdir = opendir(sysname)) == NULL)
	return -oserror();
    sts = scan_sysfs_hugepages_dir(sysname, sysdir, indom, NULL,
			hugepage_fields, HUGEPAGES_METRIC_COUNT);
    closedir(sysdir);
    if (sts < 0)
	return sts;
    if (sts > 0)
	pmdaCacheOp(indom, PMDA_CACHE_SAVE);

    return 0;
}

/*
 * This refreshes a compound instance domain, having
 * per-NUMA-node and per-hugepage-size components.
 */
int
refresh_sysfs_numa_hugepages(pmInDom indom)
{
    int i, sts, save = 0;
    DIR *sysdir;
    char prefix[128];
    char sysname[MAXPATHLEN];
    pmInDom nodes = INDOM(NODE_INDOM);
    static int setup;

    if (!setup) {
	pmdaCacheOp(indom, PMDA_CACHE_LOAD);
	cpu_node_setup();
	setup = 1;
    }

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    for (pmdaCacheOp(nodes, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(nodes, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	pmsprintf(sysname, sizeof(sysname),
		    "%s/sys/devices/system/node/node%d/hugepages",
		    linux_statspath, i);
	if ((sysdir = opendir(sysname)) == NULL)
	    continue;
	pmsprintf(prefix, sizeof(prefix), "node%d", i);
	sts = scan_sysfs_hugepages_dir(sysname, sysdir, indom, prefix,
			numa_hugepage_fields, NUMA_HUGEPAGES_METRIC_COUNT);
	closedir(sysdir);
	if (sts > 0)
	    save = 1;
    }

    if (save)
	pmdaCacheOp(indom, PMDA_CACHE_SAVE);

    return 0;
}
