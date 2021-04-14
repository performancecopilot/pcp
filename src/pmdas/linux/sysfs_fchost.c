/*
 * Linux sysfs_fchost (Fiber Channel) cluster
 *
 * Copyright (c) 2021 Red Hat.
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
#include "sysfs_fchost.h"

static struct {
    char	*name;
    int		field;
    int		shift;
} fchost_fields[] = {
    { "rx_frames", 	FCHOST_IN_FRAMES },
    { "rx_words", 	FCHOST_IN_BYTES, 2 },
    { "tx_frames",	FCHOST_OUT_FRAMES },
    { "tx_words",	FCHOST_OUT_BYTES, 2 },
    { "lip_count", 	FCHOST_LIP_COUNT },
    { "nos_count", 	FCHOST_NOS_COUNT },
    { "error_frames",	FCHOST_ERROR_FRAMES },
    { "dumped_frames",	FCHOST_DUMPED_FRAMES },

    { NULL,		FCHOST_COUNT },
};

int
refresh_sysfs_fchosts(pmInDom fchost_indom)
{
    char sysname[MAXPATHLEN];
    char statsname[MAXPATHLEN];
    char statsfile[MAXPATHLEN];
    char strvalue[64];
    DIR *sysdir, *fchostsdir;
    struct fchost *fchost;
    struct dirent *sysentry, *fchosts;
    int i, sts, fd;

    pmdaCacheOp(fchost_indom, PMDA_CACHE_INACTIVE);

    pmsprintf(sysname, sizeof(sysname), "%s/sys/class/fc_host", linux_statspath);
    if ((sysdir = opendir(sysname)) == NULL)
	return -oserror();

    while ((sysentry = readdir(sysdir)) != NULL) {
	char *host = sysentry->d_name;

	if (host[0] == '.')
	    continue;

	pmsprintf(statsname, sizeof(statsname), "%s/%s/statistics", sysname, host);
	if ((fchostsdir = opendir(statsname)) == NULL)
	    continue; /* no stats for this device? */
	/* look up this fchost in the cache, add it if not already present. */
	fchost = NULL;
	sts = pmdaCacheLookupName(fchost_indom, host, NULL, (void **)&fchost);
	if (sts < 0 || fchost == NULL) {
	    /* new fibre channel host */
	    if ((fchost = (fchost_t *)calloc(1, sizeof(fchost_t))) == NULL) {
		sts = -oserror();
		closedir(sysdir);
		closedir(fchostsdir);
		return sts;
	    }
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "%s: added new FC host \"%s\"\n",
				"refresh_sysfs_fchosts", host);
	}
	pmdaCacheStore(fchost_indom, PMDA_CACHE_ADD, host, (void *)fchost);

	/* now update the stats for the new (or existing) fibre channel host */
	memset(fchost->counts, 0, sizeof(fchost->counts));
	while ((fchosts = readdir(fchostsdir)) != NULL) {
	    ssize_t n;
	    char *h = fchosts->d_name;
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
	    for (i=0; i < FCHOST_COUNT; i++) {
		if (strncmp(fchost_fields[i].name, h, hlen) == 0) {
		    fchost->counts[i] = strtoull(strvalue, NULL, 0);
		    if (fchost->counts[i] == ULONGLONG_MAX)
			fchost->counts[i] = 0;
		    if (fchost_fields[i].shift)
			fchost->counts[i] <<= fchost_fields[i].shift;
		    break;
		}
	    }
	    close(fd);
	}
	closedir(fchostsdir);
    }
    closedir(sysdir);
    return 0;
}
