/*
 * Linux /sys/kernel/mm/hugepages cluster
 *
 * Copyright (c) 2024, Red Hat.
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
#include "sysfs_hugepages.h"

static char *hugepage_fields[] = {
    [PAGESIZE_HUGEPAGES] = "", /* directory name */
    [FREE_HUGEPAGES] = "free_hugepages",
    [RESV_HUGEPAGES] = "resv_hugepages",
    [SURPLUS_HUGEPAGES] = "surplus_hugepages",
    [TOTALSIZE_HUGEPAGES] = "nr_hugepages",
    [OVERCOMMIT_HUGEPAGES] = "nr_overcommit",
    [HUGEPAGES_METRIC_COUNT] = NULL
};

int
refresh_sysfs_hugepages(pmInDom indom)
{
    char sysname[MAXPATHLEN];
    char statsname[MAXPATHLEN];
    char statsfile[MAXPATHLEN];
    char strvalue[64], *iname;
    DIR *sysdir, *hugepagesdir;
    struct hugepages *hugepage;
    struct dirent *sysentry, *hugepages;
    unsigned long long pagesize;
    static int setup;
    int i, sts, fd, needsave = 0;

    if (!setup) {
	pmdaCacheOp(indom, PMDA_CACHE_LOAD);
	setup = 1;
    }

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
    pmsprintf(sysname, sizeof(sysname), "%s/sys/kernel/mm/hugepages", linux_statspath);
    if ((sysdir = opendir(sysname)) == NULL)
	return -oserror();

    while ((sysentry = readdir(sysdir)) != NULL) {
	char *size = sysentry->d_name;

	if (size[0] == '.')
	    continue;
	if (sscanf(size, "hugepages-%llukB", &pagesize) != 1)
	    continue;

	/* look up this size in the cache, add it if not already present. */
	hugepage = NULL;
	iname = size + 10;
	sts = pmdaCacheLookupName(indom, iname, NULL, (void **)&hugepage);
	if (sts < 0 || hugepage == NULL) {
	    /* new hugepage size */
	    if ((hugepage = (hugepages_t *)calloc(1, sizeof(hugepages_t))) == NULL) {
		sts = -oserror();
		closedir(sysdir);
		return sts;
	    }
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "%s: added new hugepage size \"%s\"\n",
				__FUNCTION__, iname);
	    needsave = 1;
	}
	pmdaCacheStore(indom, PMDA_CACHE_ADD, iname, (void *)hugepage);

	/* now update the stats for the new or existing hugepage size */
	memset(hugepage->values, 0, sizeof(hugepage->values));
	hugepage->values[PAGESIZE_HUGEPAGES] = pagesize;

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
	    for (i=0; i < HUGEPAGES_METRIC_COUNT; i++) {
		if (strncmp(hugepage_fields[i], h, hlen) == 0) {
		    hugepage->values[i] = strtoull(strvalue, NULL, 0);
			hugepage->values[i] = 0;
		    break;
		}
	    }
	    close(fd);
	}
	closedir(hugepagesdir);
    }
    closedir(sysdir);

    if (needsave)
	pmdaCacheOp(indom, PMDA_CACHE_SAVE);

    return 0;
}
