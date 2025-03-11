/*
 * Copyright (c) 2016-2018 Red Hat.
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include "pmapi.h"
#include "indom.h"
#include "getinfo.h"

/*
 * Holds /proc/tty/drivers details, for looking up tty by dev_t
 * Sample file format -
 *   /dev/tty             /dev/tty        5     0 system:/dev/tty
 *   rfcomm               /dev/rfcomm   216 0-255 serial
 *   serial               /dev/ttyS       4 64-95 serial
 *   unknown              /dev/tty        4  1-63 console
 */
typedef struct tty_driver {
    char		*devpath;
    unsigned int	major;
    unsigned int	minor;
    unsigned int	range;
} tty_driver_t;

static tty_driver_t	*tty_drivers;
static unsigned int	tty_driver_count;

void
tty_driver_init(void)
{
    tty_driver_t	*tty, *tmp;
    char		path[MAXPATHLEN];
    FILE		*file;
    char		*unused, *device, *range, *end;
    int			maj, n;
    int			sts;

    /* create a data structure of tty drivers for faster lookups */
    pmsprintf(path, sizeof(path), "%s/proc/tty/drivers", proc_statspath);
    if ((file = fopen(path, "r")) == NULL)
	return;
    /*
     * /proc/tty/drivers lines look like ...
     * /dev/tty             /dev/tty        5       0 system:/dev/tty
     * ...
     * serial               /dev/ttyS       4 64-111 serial
     */
    while (!feof(file)) {

	if ((sts = pmfstring(file, &unused)) < 1) {
	    if (sts == -1)
		break;
	    goto bad;
	}
	free(unused);
	if (pmfstring(file, &device) < 1)
	    goto bad;
	n = fscanf(file, "%d", &maj);
	if (n != 1) {
	    free(device);
	    goto bad;
	}
	if (pmfstring(file, &range) < 1) {
	    free(device);
	    goto bad;
	}
	if (pmfstring(file, &unused) < 1) {
	    free(device);
	    free(range);
	    goto bad;
	}
	free(unused);

	n = (tty_driver_count + 1) * sizeof(tty_driver_t);
	if ((tmp = (tty_driver_t *)realloc(tty_drivers, n)) == NULL) {
	    pmNoMem("tty_driver_init: realloc", n, PM_RECOV_ERR);
	    free(device);
	    free(range);
	    break;
	}

	tty = &tmp[tty_driver_count];
	if (strncmp(end = device, "/dev/", 5) == 0)
	    end += 5;
	tty->devpath = strdup(end);
	tty->major = maj;
	tty->minor = strtoul(range, &end, 10);
	if (*end != '-')
	    tty->range = tty->minor;
	else
	    tty->range = strtoul(end + 1, &end, 10);

	tty_drivers = tmp;
	tty_driver_count++;

	free(device);
	free(range);
	continue;

bad:
	fprintf(stderr, "%s: bad format at %s:%d\n", __func__, path, tty_driver_count+1);
	break;
    }
    fclose(file);
}

static char *
lookup_ttyname(dev_t dev)
{
    tty_driver_t	*tty;
    unsigned int	i, maj = major(dev), min = minor(dev);
    static char		devpath[256];

    for (i = 0; i < tty_driver_count; i++) {
	tty = &tty_drivers[i];
	if (tty->major != maj)
	    continue;
	if (min == tty->minor && min == tty->range)
	    return tty->devpath;
	if (min < tty->minor || min > tty->range)
	    break;
	pmsprintf(devpath, sizeof(devpath), "%s/%u", tty->devpath, min);
	return devpath;
    }
    return strcpy(devpath, "?");
}

/*
 * Attempt to map a device number to a tty for a given process.
 *
 * Previously this was much more elaborate, scanning all open fds
 * for a match on the device; but that is expensive for processes
 * with many open fds, and we end up stat'ing all sorts of files
 * unrelated to the job at hand (which SElinux blocks and reports
 * as poor form).
 *
 * Returns a pointer into a static buffer, so no free'ing needed.
 */
static char *
get_ttyname(dev_t dev, char *devpath)
{
    static char	ttyname[MAXPATHLEN];
    char	fullpath[MAXPATHLEN];
    struct stat	statbuf;
    char	*path;
    struct dirent *drp;
    DIR		*rundir;

    strcpy(ttyname, "?");
    if ((rundir = opendir(devpath)) == NULL)
	return ttyname;

    while ((drp = readdir(rundir)) != NULL) {
	if (*(path = &drp->d_name[0]) == '.')
	    continue;
	pmsprintf(fullpath, sizeof(fullpath), "%s/%s", devpath, path);
	fullpath[sizeof(fullpath)-1] = '\0';
	if (stat(fullpath, &statbuf) != 0) {
	    if (pmDebugOptions.libpmda && pmDebugOptions.desperate)
		fprintf(stderr, "get_ttyname %s stat: %s\n",
			fullpath, strerror(errno));
	}
	else if (S_ISCHR(statbuf.st_mode) && dev == statbuf.st_rdev) {
	   /* note - this depends on all paths being /dev prefixed */
	   pmstrncpy(ttyname, sizeof(ttyname), &fullpath[5]);
	   break;
	}
    }
    closedir(rundir);
    return ttyname;
}

/*
 * Use kernels device number encoding (dev_t) to
 * before searching for matching tty name.
 */
char *
get_ttyname_info(dev_t dev)
{
    char	*name;

    name = lookup_ttyname(dev);
    if (*name != '?')
	return name;
    name = get_ttyname(dev, "/dev/pts");
    if (*name != '?')
	return name;
    return get_ttyname(dev, "/dev");
}
