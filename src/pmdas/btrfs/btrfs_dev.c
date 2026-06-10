/*
 * Copyright (c) 2026 Red Hat.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "btrfs_utils.h"
#include "btrfs_dev.h"

static void
btrfs_dev_refresh_error_stats(const char *dev_path, btrfs_dev_t *dev)
{
    char path[MAXPATHLEN], *line = NULL;
    size_t len = 0;
    FILE *fp;

    dev->write_errs = 0;
    dev->read_errs = 0;
    dev->flush_errs = 0;
    dev->corruption_errs = 0;
    dev->generation_errs = 0;

    pmsprintf(path, sizeof(path), "%s%cerror_stats", dev_path, pmPathSeparator());
    fp = fopen(path, "r");
    if (fp == NULL)
	return;
    while (getline(&line, &len, fp) != -1) {
	if (strncmp(line, "write_errs", 10) == 0)
	    sscanf(line, "write_errs %llu", (unsigned long long *)&dev->write_errs);
	else if (strncmp(line, "read_errs", 9) == 0)
	    sscanf(line, "read_errs %llu", (unsigned long long *)&dev->read_errs);
	else if (strncmp(line, "flush_errs", 10) == 0)
	    sscanf(line, "flush_errs %llu", (unsigned long long *)&dev->flush_errs);
	else if (strncmp(line, "corruption_errs", 15) == 0)
	    sscanf(line, "corruption_errs %llu", (unsigned long long *)&dev->corruption_errs);
	else if (strncmp(line, "generation_errs", 15) == 0)
	    sscanf(line, "generation_errs %llu", (unsigned long long *)&dev->generation_errs);
    }
    free(line);
    fclose(fp);
}

void
btrfs_dev_refresh(pmInDom indom)
{
    DIR *fs_dp, *dev_dp;
    struct dirent *fs_ep, *dev_ep;
    struct stat sstat;
    char fs_path[MAXPATHLEN], devinfo_path[MAXPATHLEN], dev_path[MAXPATHLEN];
    char inst_name[128];
    int sts;
    btrfs_dev_t *dev;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    fs_dp = opendir(btrfs_path);
    if (fs_dp == NULL)
	return;

    while ((fs_ep = readdir(fs_dp)) != NULL) {
	if (fs_ep->d_name[0] == '.')
	    continue;
	pmsprintf(fs_path, sizeof(fs_path), "%s/%s",
		btrfs_path, fs_ep->d_name);
	if (stat(fs_path, &sstat) < 0 || !S_ISDIR(sstat.st_mode))
	    continue;

	pmsprintf(devinfo_path, sizeof(devinfo_path), "%s/devinfo",
		fs_path);
	dev_dp = opendir(devinfo_path);
	if (dev_dp == NULL)
	    continue;

	while ((dev_ep = readdir(dev_dp)) != NULL) {
	    if (dev_ep->d_name[0] == '.')
		continue;
	    pmsprintf(dev_path, sizeof(dev_path), "%s/%s",
		    devinfo_path, dev_ep->d_name);
	    if (stat(dev_path, &sstat) < 0 || !S_ISDIR(sstat.st_mode))
		continue;

	    pmsprintf(inst_name, sizeof(inst_name), "%s::%s",
		    fs_ep->d_name, dev_ep->d_name);

	    sts = pmdaCacheLookupName(indom, inst_name, NULL, (void **)&dev);
	    if (sts == PM_ERR_INST || (sts >= 0 && dev == NULL)) {
		dev = calloc(1, sizeof(*dev));
		if (dev == NULL) {
		    pmNoMem("btrfs_dev", sizeof(*dev), PM_RECOV_ERR);
		    continue;
		}
	    }
	    else if (sts < 0)
		continue;

	    pmstrncpy(dev->uuid, sizeof(dev->uuid), fs_ep->d_name);
	    pmstrncpy(dev->devid, sizeof(dev->devid), dev_ep->d_name);
	    btrfs_dev_refresh_error_stats(dev_path, dev);

	    pmdaCacheStore(indom, PMDA_CACHE_ADD, inst_name, (void *)dev);
	}
	closedir(dev_dp);
    }
    closedir(fs_dp);
}
