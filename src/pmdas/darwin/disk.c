/*
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <mach/mach.h>
#define IOKIT 1
#include <device/device_types.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/IOBSD.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include "disk.h"

/*
 * Ensure we have space for the next device in our pre-allocated
 * device pool.  If not, make some or pass on the error.
 */
static int
check_stats_size(struct diskstats *stats, int count)
{
    if (count > stats->highwater) {
	stats->highwater++;
	stats->disks = realloc(stats->disks,
				stats->highwater * sizeof(struct diskstat));
	if (!stats->disks) {
	    stats->highwater = 0;
	    return -ENOMEM;
	}
    }
    return 0;
}

/*
 * Insert all disks into the global disk instance domain.
 */
static int
update_disk_indom(struct diskstats *all, int count, pmdaIndom *indom)
{
    int	i;

    if (count > 0 && count != indom->it_numinst) {
	i = sizeof(pmdaInstid) * count;
	if ((indom->it_set = realloc(indom->it_set, i)) == NULL) {
	    indom->it_numinst = 0;
	    return -ENOMEM;
	}
    }
    for (i = 0; i < count; i++) {
	indom->it_set[i].i_name = all->disks[i].name;
	indom->it_set[i].i_inst = i;
    }
    indom->it_numinst = count;
    return 0;
}

/*
 * Update the global counters with values from one disk.
 */
static void
update_disk_totals(struct diskstats *all, struct diskstat *disk)
{
    all->read		+= disk->read;
    all->write		+= disk->write;
    all->read_bytes	+= disk->read_bytes;
    all->write_bytes	+= disk->write_bytes;
    all->blkread	+= disk->read_bytes / disk->blocksize;
    all->blkwrite	+= disk->write_bytes / disk->blocksize;
    all->read_time	+= disk->read_time;
    all->write_time	+= disk->write_time;
}

static void
clear_disk_totals(struct diskstats *all)
{
    all->read		= 0;
    all->write		= 0;
    all->read_bytes	= 0;
    all->write_bytes	= 0;
    all->blkread	= 0;
    all->blkwrite	= 0;
    all->read_time	= 0;
    all->write_time	= 0;
}

/*
 * Update the counters associated with a single disk.
 */
static void
update_disk_stats(struct diskstat *disk,
		  CFDictionaryRef pproperties, CFDictionaryRef properties)
{
    CFDictionaryRef	statistics;
    CFStringRef		name;
    CFNumberRef		number;

    memset(disk, 0, sizeof(struct diskstat));

    /* Get name from the drive properties */
    name = (CFStringRef) CFDictionaryGetValue(pproperties,
			CFSTR(kIOBSDNameKey));
    if(name == NULL)
	return; /* Not much we can do with no name */

    CFStringGetCString(name, disk->name, DEVNAMEMAX,
			CFStringGetSystemEncoding());

    /* Get the blocksize from the drive properties */
    number = (CFNumberRef) CFDictionaryGetValue(pproperties,
			CFSTR(kIOMediaPreferredBlockSizeKey));
    if(number == NULL)
	return; /* Not much we can do with no number */
    CFNumberGetValue(number, kCFNumberSInt64Type, &disk->blocksize);

    /* Get the statistics from the device properties. */
    statistics = (CFDictionaryRef) CFDictionaryGetValue(properties,
			CFSTR(kIOBlockStorageDriverStatisticsKey));
    if (statistics) {
	number = (CFNumberRef) CFDictionaryGetValue(statistics,
			CFSTR(kIOBlockStorageDriverStatisticsReadsKey));
	if (number)
	    CFNumberGetValue(number, kCFNumberSInt64Type,
					&disk->read);
	number = (CFNumberRef) CFDictionaryGetValue(statistics,
			CFSTR(kIOBlockStorageDriverStatisticsWritesKey));
	if (number)
	    CFNumberGetValue(number, kCFNumberSInt64Type,
					&disk->write);
	number = (CFNumberRef) CFDictionaryGetValue(statistics,
		CFSTR(kIOBlockStorageDriverStatisticsBytesReadKey));
	if (number)
	    CFNumberGetValue(number, kCFNumberSInt64Type,
					&disk->read_bytes);
	number = (CFNumberRef) CFDictionaryGetValue(statistics,
		CFSTR(kIOBlockStorageDriverStatisticsBytesWrittenKey));
	if (number)
	    CFNumberGetValue(number, kCFNumberSInt64Type,
					&disk->write_bytes);
	number = (CFNumberRef) CFDictionaryGetValue(statistics,
		CFSTR(kIOBlockStorageDriverStatisticsLatentReadTimeKey));
	if (number)
	    CFNumberGetValue(number, kCFNumberSInt64Type,
					&disk->read_time);
	number = (CFNumberRef) CFDictionaryGetValue(statistics,
		CFSTR(kIOBlockStorageDriverStatisticsLatentWriteTimeKey));
	if (number)
	    CFNumberGetValue(number, kCFNumberSInt64Type,
					&disk->write_time);
    }
}

static int
update_disk(diskstats_t *stats, io_registry_entry_t drive, int index)
{
    io_registry_entry_t		device;
    CFDictionaryRef		pproperties, properties;
    int				status;

    /* Get the drives parent, from which we get statistics. */
    status = IORegistryEntryGetParentEntry(drive, kIOServicePlane, &device);
    if (status != KERN_SUCCESS)
	return -oserror();

    if (!IOObjectConformsTo(device, "IOBlockStorageDriver")) {
	IOObjectRelease(device);
	return 0;
    }

    /* Obtain the drive properties. */
    pproperties = 0;
    status = IORegistryEntryCreateCFProperties(drive,
			(CFMutableDictionaryRef *)&pproperties,
			kCFAllocatorDefault, kNilOptions);
    if (status != KERN_SUCCESS) {
	IOObjectRelease(device);
	return -oserror();
    }

    /* Obtain the device properties. */
    properties = 0;
    status = IORegistryEntryCreateCFProperties(device,
			(CFMutableDictionaryRef *)&properties,
			kCFAllocatorDefault, kNilOptions);
    if (status != KERN_SUCCESS) {
	IOObjectRelease(device);
	return -oserror();
    }

    /* Make space to store the actual values, then go get them. */
    status = check_stats_size(stats, index + 1);
    if (status < 0) {
	IOObjectRelease(device);
    } else {
	update_disk_stats(&stats->disks[index], pproperties, properties);
	update_disk_totals(stats, &stats->disks[index]);
    }
    CFRelease(pproperties);
    CFRelease(properties);
    return status;
}

int
refresh_disks(struct diskstats *stats, pmdaIndom *indom)
{
    io_registry_entry_t		drive;
    CFMutableDictionaryRef	match;
    int				i, status;
    static int			inited = 0;
    static mach_port_t		mach_master_port;
    static io_iterator_t	mach_device_list;

    if (!inited) {
	/* Get ports and services for device statistics. */
	if (IOMasterPort(bootstrap_port, &mach_master_port)) {
	    fprintf(stderr, "%s: IOMasterPort error\n", __FUNCTION__);
	    return -oserror();
	}
	memset(stats, 0, sizeof(struct diskstats));
	inited = 1;
    }

    /* Get an interator for IOMedia objects (drives). */
    match = IOServiceMatching("IOMedia");
    CFDictionaryAddValue(match, CFSTR(kIOMediaWholeKey), kCFBooleanTrue);
    status = IOServiceGetMatchingServices(mach_master_port,
						match, &mach_device_list);
    if (status != KERN_SUCCESS) {
	    fprintf(stderr, "%s: IOServiceGetMatchingServices error\n",
			__FUNCTION__);
	    return -oserror();
    }

    indom->it_numinst = 0;
    clear_disk_totals(stats);
    for (i = 0; (drive = IOIteratorNext(mach_device_list)) != 0; i++) {
	status = update_disk(stats, drive, i);
	if (status)
		break;
	IOObjectRelease(drive);
    }
    IOIteratorReset(mach_device_list);

    if (!status)
	status = update_disk_indom(stats, i, indom);
    return status;
}
