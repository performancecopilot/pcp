/*
 * Linux Filesystem Cluster
 *
 * Copyright (c) 2014-2015 Red Hat.
 * Copyright (c) 2000,2004,2007-2008 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "impl.h"
#include "pmda.h"
#include "indom.h"
#include "filesys.h"

char *
scan_filesys_options(const char *options, const char *option)
{
    static char buffer[128];
    char *s;

    strncpy(buffer, options, sizeof(buffer));
    buffer[sizeof(buffer)-1] = '\0';

    s = strtok(buffer, ",");
    while (s) {
	if (strcmp(s, option) == 0)
	    return s;
        s = strtok(NULL, ",");
    }
    return NULL;
}

int
refresh_filesys(pmInDom filesys_indom, pmInDom tmpfs_indom,
		struct linux_container *cp)
{
    char buf[MAXPATHLEN];
    char src[MAXPATHLEN];
    filesys_t *fs;
    pmInDom indom;
    FILE *fp;
    char *path, *device, *type, *options;
    int sts;

    pmdaCacheOp(tmpfs_indom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(filesys_indom, PMDA_CACHE_INACTIVE);

    /*
     * When operating within a container namespace, cannot refer
     * to "self" due to it being a symlinked pid from the host.
     */
    snprintf(src, sizeof(src), "%s/proc/%s/mounts",
				linux_statspath, cp ? "1" : "self");
    if ((fp = fopen(src, "r")) == NULL)
	return -oserror();

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((device = strtok(buf, " ")) == 0)
	    continue;

	path = strtok(NULL, " ");
	type = strtok(NULL, " ");
	options = strtok(NULL, " ");
	if (strcmp(type, "proc") == 0 ||
	    strcmp(type, "nfs") == 0 ||
	    strcmp(type, "devfs") == 0 ||
	    strcmp(type, "devpts") == 0 ||
	    strcmp(type, "devtmpfs") == 0 ||
	    strcmp(type, "selinuxfs") == 0 ||
	    strcmp(type, "securityfs") == 0 ||
	    strcmp(type, "configfs") == 0 ||
	    strcmp(type, "cgroup") == 0 ||
	    strcmp(type, "sysfs") == 0 ||
	    strncmp(type, "auto", 4) == 0)
	    continue;

	indom = filesys_indom;
	if (strcmp(type, "tmpfs") == 0) {
	    indom = tmpfs_indom;
	    device = path;
	}
	else if (strncmp(device, "/dev", 4) != 0)
	    continue;
	if (realpath(device, src) != NULL)
	    device = src;

	sts = pmdaCacheLookupName(indom, device, NULL, (void **)&fs);
	if (sts == PMDA_CACHE_ACTIVE)	/* repeated line in /proc/mounts? */
	    continue;
	if (sts == PMDA_CACHE_INACTIVE) { /* re-activate an old mount */
	    pmdaCacheStore(indom, PMDA_CACHE_ADD, device, fs);
	    if (strcmp(path, fs->path) != 0) {	/* old device, new path */
		free(fs->path);
		fs->path = strdup(path);
	    }
	    if (strcmp(options, fs->options) != 0) {	/* old device, new opts */
		free(fs->options);
		fs->options = strdup(options);
	    }
	}
	else {	/* new mount */
	    if ((fs = malloc(sizeof(filesys_t))) == NULL)
		continue;
	    fs->device = strdup(device);
	    fs->path = strdup(path);
	    fs->options = strdup(options);
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
		fprintf(stderr, "refresh_filesys: add \"%s\" \"%s\"\n",
		    fs->path, device);
	    }
#endif
	    pmdaCacheStore(indom, PMDA_CACHE_ADD, device, fs);
	}
	fs->flags = 0;
    }

    /*
     * success
     * Note: we do not call statfs() here since only some instances
     * may be requested (rather, we do it in linux_fetch, see pmda.c).
     */
    fclose(fp);
    return 0;
}
