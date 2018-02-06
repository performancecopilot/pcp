/*
 * Linux sysfs_tapestats cluster
 *
 * Copyright (c) 2017-2018 Red Hat.
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
#include "sysfs_tapestats.h"

static struct {
    char	*name;
    int		field;
} tapestat_fields[] = {
    { "in_flight", 	TAPESTATS_IN_FLIGHT },
    { "io_ns",	 	TAPESTATS_IO_NS },
    { "other_cnt",	TAPESTATS_OTHER_CNT },
    { "read_byte_cnt",	TAPESTATS_READ_BYTE_CNT },
    { "read_cnt", 	TAPESTATS_READ_CNT },
    { "read_ns", 	TAPESTATS_READ_NS },
    { "resid_cnt",	TAPESTATS_RESID_CNT },
    { "write_byte_cnt",	TAPESTATS_WRITE_BYTE_CNT },
    { "write_cnt",	TAPESTATS_WRITE_CNT },
    { "write_ns",	TAPESTATS_WRITE_NS },

    { NULL,		TAPESTATS_COUNT },
};

int
refresh_sysfs_tapestats(pmInDom tape_indom)
{
    char sysname[MAXPATHLEN];
    char statsname[MAXPATHLEN];
    char statsfile[MAXPATHLEN];
    char strvalue[64];
    DIR *sysdir, *tapestatsdir;
    tapedev_t *device;
    struct dirent *sysentry, *tapestats;
    int i, sts, fd;

    pmdaCacheOp(tape_indom, PMDA_CACHE_INACTIVE);

    pmsprintf(sysname, sizeof(sysname), "%s/sys/class/scsi_tape", linux_statspath);
    if ((sysdir = opendir(sysname)) == NULL)
	return -oserror();

    while ((sysentry = readdir(sysdir)) != NULL) {
	char *sysdev = sysentry->d_name;

	if (sysdev[0] == '.')
	    continue;
	/*
	 * We're only interested in st[0-9]+ devices. The statistics are all the
	 * same for the derived devices, e.g. nst0 shares the same stats as st0.
	 */
	if (strncmp(sysdev, "st", 2) != 0 || !isdigit(sysdev[strlen(sysdev)-1]))
	    continue;

	pmsprintf(statsname, sizeof(statsname), "%s/%s/stats", sysname, sysdev);
	if ((tapestatsdir = opendir(statsname)) == NULL)
	    continue; /* no stats for this device? */
	/*
	 * Look up this tape device in the PMDA cache
	 * or add it if not already present.
	 */
	sts = pmdaCacheLookupName(tape_indom, sysdev, NULL, (void **)&device);
	if (sts < 0) {
	    /* new tape device */
	    if ((device = (tapedev_t *)malloc(sizeof(tapedev_t))) == NULL) {
		sts = -oserror();
		closedir(sysdir);
		closedir(tapestatsdir);
		return sts;
	    }
	    memset(device, 0, sizeof(tapedev_t));
	    strncpy(device->devname, sysdev, sizeof(device->devname) - 1);
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "refresh_sysfs_tapestats: added new tape device \"%s\"\n", sysdev);
	}
	device->devnum = pmdaCacheStore(tape_indom, PMDA_CACHE_ADD, device->devname, (void *)device);

	/* now update the stats for the new (or existing) tape device */
	memset(device->counts, 0, sizeof(device->counts));
	while ((tapestats = readdir(tapestatsdir)) != NULL) {
	    char *ts = tapestats->d_name;
	    int tslen = strlen(ts);

	    if (ts[0] == '.')
	    	continue;
	    pmsprintf(statsfile, sizeof(statsfile), "%s/%s", statsname, ts);
	    if ((fd = open(statsfile, O_RDONLY)) < 0)
	    	continue; /* should report this */
	    /*
	     * kernel bug - exported value is not terminated with NULL or \n
	     * so we have to zero our buffer prior to reading. sigh.
	     */
	    memset(strvalue, 0, sizeof(strvalue));
	    if (read(fd, strvalue, sizeof(strvalue)) <= 0) {
		close(fd);
	    	continue;
	    }
	    for (i=0; i < TAPESTATS_COUNT; i++) {
		if (strncmp(tapestat_fields[i].name, ts, tslen) == 0) {
		    device->counts[i] = strtoll(strvalue, NULL, 10);
		    break;
		}
	    }
	    close(fd);
	}
	closedir(tapestatsdir);
    }
    closedir(sysdir);
    return 0;
}
