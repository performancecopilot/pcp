/*
 * Linux JBD2 (ext3/ext4) driver metrics.
 *
 * Copyright (C) 2013 Red Hat.
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
#include <dirent.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "pmda.h"
#include "proc_jbd2.h"

enum {
    HEADER_STATS,
    SEEKING_STATS,
    AVERAGE_STATS,
};

static int
refresh_journal(const char *path, const char *dev, pmInDom indom)
{
    int n, state, indom_changed = 0;
    char buf[MAXPATHLEN], *id;
    unsigned long long value;
    proc_jbd2_t *jp;
    FILE *fp;

    if (dev[0] == '.')
	return 0;	/* only interest is in device files */
    if (snprintf(buf, sizeof(buf), "%s/%s/info", path, dev) == sizeof(buf))
	return 0;	/* ignore, dodgey command line args */
    if ((fp = fopen(buf, "r")) == NULL)
	return 0;	/* no permission, ignore this entry */

    if (pmdaCacheLookupName(indom, dev, &n, (void **)&jp) < 0 || !jp) {
	if ((jp = (proc_jbd2_t *)calloc(1, sizeof(proc_jbd2_t))) != NULL)
	    indom_changed++;
    }
    if (!jp) {
	fclose(fp);
	return 0;
    }

    state = HEADER_STATS;	/* seeking the header, initially */
    while (fgets(buf, sizeof(buf), fp) != NULL) {
	switch (state) {
	case HEADER_STATS:
	    if (sscanf(buf,
		"%llu transactions (%llu requested), each up to %u blocks\n",
			(unsigned long long *) &jp->tid,
			(unsigned long long *) &jp->requested,
			(unsigned int *) &jp->max_buffers) == 3) {
		state = SEEKING_STATS;
		jp->version = 3;	/* 3.x kernel header format */
	    }
	    else if (sscanf(buf,
		"%llu transaction, each up to %u blocks\n",
			(unsigned long long *) &jp->tid,
			(unsigned int *) &jp->max_buffers) == 2) {
		state = SEEKING_STATS;
		jp->version = 2;	/* 2.x kernel header format */
	    }
	    break;

	case SEEKING_STATS:
	    if (strncmp(buf, "average: \n", 8) == 0)
		state = AVERAGE_STATS;
	    break;

	case AVERAGE_STATS:
	    value = strtoull(buf, &id, 10);
	    if (id == buf)
		continue;
	    else if (strcmp(id, "ms waiting for transaction\n") == 0)
		jp->waiting = value;
	    else if (strcmp(id, "ms request delay\n") == 0)
		jp->request_delay = value;
	    else if (strcmp(id, "ms running transaction\n") == 0)
		jp->running = value;
	    else if (strcmp(id, "ms transaction was being locked\n") == 0)
		jp->locked = value;
	    else if (strcmp(id, "ms flushing data (in ordered mode)\n") == 0)
		jp->flushing = value;
	    else if (strcmp(id, "ms logging transaction\n") == 0)
		jp->logging = value;
	    else if (strcmp(id, "us average transaction commit time\n") == 0)
		jp->average_commit_time = value;
	    else if (strcmp(id, " handles per transaction\n") == 0)
		jp->handles = value;
	    else if (strcmp(id, " blocks per transaction\n") == 0)
		jp->blocks = value;
	    else if (strcmp(id, " logged blocks per transaction\n") == 0)
		jp->blocks_logged = value;
	    break;

	default:
	    break;
	}
    }
    fclose(fp);

    if (state != AVERAGE_STATS) {
	if (indom_changed)
	    free(jp);
	return 0;
    }

    pmdaCacheStore(indom, PMDA_CACHE_ADD, dev, jp);
    return indom_changed;
}

int
refresh_jbd2(const char *path, pmInDom jbd2_indom)
{
    DIR *dirp;
    struct dirent *dent;
    int indom_changes = 0;
    static int first = 1;

    if (first) {
	/* initialize the instance domain caches */
	pmdaCacheOp(jbd2_indom, PMDA_CACHE_LOAD);
	indom_changes = 1;
	first = 0;
    }

    pmdaCacheOp(jbd2_indom, PMDA_CACHE_INACTIVE);
    if ((dirp = opendir(path)) == NULL)
	return -ENOENT;
    while ((dent = readdir(dirp)) != NULL)
	indom_changes |= refresh_journal(path, dent->d_name, jbd2_indom);
    closedir(dirp);

    if (indom_changes)
	pmdaCacheOp(jbd2_indom, PMDA_CACHE_SAVE);
    return 0;
}
