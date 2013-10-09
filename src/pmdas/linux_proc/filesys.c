/*
 * Copyright (c) 2000,2004,2007-2008 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2012 Red Hat, Inc. All Rights Reserved.
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
refresh_filesys(pmInDom indom)
{
    char buf[MAXPATHLEN];
    filesys_t *fs;
    FILE *fp;
    char *path, *device, *type, *options;
    int sts;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    if ((fp = fopen("/proc/mounts", "r")) == (FILE *)NULL)
	return -oserror();

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	device = strtok(buf, " ");
	path = strtok(NULL, " ");
	type = strtok(NULL, " ");
	options = strtok(NULL, " ");
	if (strcmp(type, "cgroup") != 0)
	    continue;

	sts = pmdaCacheLookupName(indom, path, NULL, (void **)&fs);
	if (sts == PMDA_CACHE_ACTIVE)	/* repeated line in /proc/mounts? */
	    continue;
	if (sts == PMDA_CACHE_INACTIVE) { /* re-activate an old mount */
	    pmdaCacheStore(indom, PMDA_CACHE_ADD, path, fs);
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
	    pmdaCacheStore(indom, PMDA_CACHE_ADD, path, fs);
	}
	fs->flags = 0;
    }

    /*
     * success
     */
    fclose(fp);
    return 0;
}
