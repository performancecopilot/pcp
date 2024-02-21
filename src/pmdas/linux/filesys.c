/*
 * Linux Filesystem Cluster
 *
 * Copyright (c) 2014-2016 Red Hat.
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
#include "linux.h"
#include "filesys.h"
#include <strings.h>

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

static void
do_uuids(pmInDom filesys_indom)
{
    /*
     * Sort of one-trip logic here ... we do it once, then it is a
     * NOP until we see the mtime of the /dev/disk/by-uuid directory
     * has changed
     */

    int			sts;
    DIR			*dp;
    struct dirent	*dep;
    char		path[MAXPATHLEN];
    char		link[MAXPATHLEN];
    char		device[MAXPATHLEN];
    ssize_t		len;
    char		*devname;
    filesys_t		*fs;
    struct stat		sbuf;
    static struct timespec		mtim = { 0 };
    static int		seen_err = 0;

    pmsprintf(path, sizeof(path), "%s/dev/disk/by-uuid", linux_statspath);

    if (stat(path, &sbuf) < 0) {
	/*
	 * containers, esp in GitHub CI don't even have this directory,
	 * so silently move on ...
	 */
	if (pmDebugOptions.libpmda) {
	    if (!seen_err) {
		fprintf(stderr, "do_uuids: stat(%s) failed: %s\n", path, pmErrStr(-oserror()));
		seen_err = 1;
	    }
	}
	return;
    }
    if (mtim.tv_sec > 0 &&
        mtim.tv_sec == sbuf.st_mtim.tv_sec &&
        mtim.tv_nsec == sbuf.st_mtim.tv_nsec)
	return;

    mtim.tv_sec = sbuf.st_mtim.tv_sec;
    mtim.tv_nsec = sbuf.st_mtim.tv_nsec;

    if ((dp = opendir(path)) == NULL) {
	/* don't expect to get here, but report if -Dlibpmda */
	if (pmDebugOptions.libpmda) {
	    if (!seen_err) {
		fprintf(stderr, "do_uuids: opendir(%s) failed: %s\n", path, pmErrStr(-oserror()));
		seen_err = 1;
	    }
	}
	return;
    }

    while ((dep = readdir(dp)) != NULL) {
	if (strcmp(dep->d_name, ".") == 0) continue;
	if (strcmp(dep->d_name, "..") == 0) continue;
	pmsprintf(path, sizeof(path), "%s/dev/disk/by-uuid/%s", linux_statspath, dep->d_name);
	len = readlink(path, link, sizeof(link));
	link[len] = '\0';
	devname = rindex(link, '/');
	pmsprintf(device, sizeof(device), "/dev%s", devname);
	sts = pmdaCacheLookupName(filesys_indom, device, NULL, (void **)&fs);
	if (sts != PMDA_CACHE_ACTIVE) {
	    /*
	     * this is a block device that has a UUID, but is not a
	     * mounted filesystem ... either swap device or an unmounted
	     * filesystem, so ignore it
	     */
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "do_uuids: Warning: disk %s not in InDom Cache\n", device);
	    continue;
	}
	if (fs->uuid == NULL) {
	    /*
	     * first time for this device
	     */
	    fs->uuid = strdup(dep->d_name);
	    if (pmDebugOptions.libpmda) {
		fprintf(stderr, "do_uuids: add \"%s\" \"%s\"\n",
		    device, fs->uuid);
	    }
	}
	else if (strcmp(fs->uuid, dep->d_name) != 0) {
	    /*
	     * uuid changed ...
	     */
	    free(fs->uuid);
	    fs->uuid = strdup(dep->d_name);
	    if (pmDebugOptions.libpmda) {
		fprintf(stderr, "do_uuids: change \"%s\" \"%s\"\n",
		    device, fs->uuid);
	    }
	}
    }

    closedir(dp);

    return;
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
    pmsprintf(src, sizeof(src), "%s/proc/%s/mounts",
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
	    strcmp(type, "squashfs") == 0 ||
	    strcmp(type, "selinuxfs") == 0 ||
	    strcmp(type, "securityfs") == 0 ||
	    strcmp(type, "configfs") == 0 ||
	    strcmp(type, "cgroup") == 0 ||
	    strcmp(type, "sysfs") == 0 ||
	    strcmp(type, "tmpfs") == 0 ||
	    strncmp(type, "auto", 4) == 0)
	    continue;

	indom = filesys_indom;
	if (strcmp(type, "tmpfs") == 0) {
	    indom = tmpfs_indom;
	    device = path;
	}
	else if (strncmp(device, "/dev", 4) != 0 && strcmp(path, "/") != 0)
	    continue;

	/* keep dm and md persistent names, RHBZ#1349932 */
	if (strncmp(device, "/dev/mapper", 11) != 0 && strncmp(device, "/dev/md", 7) != 0) {
	    if (realpath(device, src) != NULL)
		device = src;
	}

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
	    fs->uuid = NULL;
	    if (pmDebugOptions.libpmda) {
		fprintf(stderr, "refresh_filesys: add \"%s\" \"%s\"\n",
		    fs->path, device);
	    }
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

    /*
     * update uuids
     */
    do_uuids(filesys_indom);

    return 0;
}
