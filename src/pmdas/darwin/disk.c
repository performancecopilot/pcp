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
#include "pmda.h"

#include "darwin.h"
#include "disk.h"

/*
 * Ensure we have space for the next device in our pre-allocated
 * device pool.  If not, make some or pass on the error.
 */
static int
check_stats_size(struct diskstats *stats, int count)
{
    if (count > stats->highwater) {
	if (stats->highwater == 0)
	    stats->highwater = 1;
	else
	    stats->highwater <<= 1;
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
    all->read_errors	+= disk->read_errors;
    all->write_errors	+= disk->write_errors;
    all->read_retries	+= disk->read_retries;
    all->write_retries	+= disk->write_retries;
    all->total_read_time	+= disk->total_read_time;
    all->total_write_time	+= disk->total_write_time;
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
    all->read_errors	= 0;
    all->write_errors	= 0;
    all->read_retries	= 0;
    all->write_retries	= 0;
    all->total_read_time	= 0;
    all->total_write_time	= 0;
}

/*
 * Update the counters associated with a single disk.
 */
static int
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
    if (name == NULL || CFStringGetLength(name) == 0)
	return -ENOENT; /* Not much we can do with no name */

    CFStringGetCString(name, disk->name, DEVNAMEMAX,
			CFStringGetSystemEncoding());
    if (disk->name[0] == '\0')
	return -ENOENT; /* Not much we can do with no name */

    /* Get the blocksize from the drive properties */
    number = (CFNumberRef) CFDictionaryGetValue(pproperties,
			CFSTR(kIOMediaPreferredBlockSizeKey));
    if (number == NULL)
	return -ENOENT; /* Not much we can do with no number */
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
	number = (CFNumberRef) CFDictionaryGetValue(statistics,
		CFSTR(kIOBlockStorageDriverStatisticsReadErrorsKey));
	if (number)
	    CFNumberGetValue(number, kCFNumberSInt64Type,
					&disk->read_errors);
	number = (CFNumberRef) CFDictionaryGetValue(statistics,
		CFSTR(kIOBlockStorageDriverStatisticsWriteErrorsKey));
	if (number)
	    CFNumberGetValue(number, kCFNumberSInt64Type,
					&disk->write_errors);
	number = (CFNumberRef) CFDictionaryGetValue(statistics,
		CFSTR(kIOBlockStorageDriverStatisticsReadRetriesKey));
	if (number)
	    CFNumberGetValue(number, kCFNumberSInt64Type,
					&disk->read_retries);
	number = (CFNumberRef) CFDictionaryGetValue(statistics,
		CFSTR(kIOBlockStorageDriverStatisticsWriteRetriesKey));
	if (number)
	    CFNumberGetValue(number, kCFNumberSInt64Type,
					&disk->write_retries);
	number = (CFNumberRef) CFDictionaryGetValue(statistics,
		CFSTR(kIOBlockStorageDriverStatisticsTotalReadTimeKey));
	if (number)
	    CFNumberGetValue(number, kCFNumberSInt64Type,
					&disk->total_read_time);
	number = (CFNumberRef) CFDictionaryGetValue(statistics,
		CFSTR(kIOBlockStorageDriverStatisticsTotalWriteTimeKey));
	if (number)
	    CFNumberGetValue(number, kCFNumberSInt64Type,
					&disk->total_write_time);
    }
    return 0;
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
	return -ENOENT;
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
	status = update_disk_stats(&stats->disks[index], pproperties, properties);
	if (status < 0)
	    IOObjectRelease(device);
	else
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
    unsigned int		count;
    int				status;
    static int			inited = 0;
    static mach_port_t		mach_port;
    static io_iterator_t	mach_device_list;

    if (!inited) {
	/* Get ports and services for device statistics. */
	if (IOMasterPort(bootstrap_port, &mach_port)) {
	    fprintf(stderr, "%s: IOMasterPort error\n", __FUNCTION__);
	    return -oserror();
	}
	memset(stats, 0, sizeof(struct diskstats));
	inited = 1;
    }

    /* Get an interator for IOMedia objects (drives). */
    match = IOServiceMatching("IOMedia");
    CFDictionaryAddValue(match, CFSTR(kIOMediaWholeKey), kCFBooleanTrue);
    status = IOServiceGetMatchingServices(mach_port, match, &mach_device_list);
    if (status != KERN_SUCCESS) {
	    fprintf(stderr, "%s: IOServiceGetMatchingServices error\n",
			__FUNCTION__);
	    return -oserror();
    }

    count = indom->it_numinst = 0;
    clear_disk_totals(stats);

    while ((drive = IOIteratorNext(mach_device_list)) != 0) {
	status = update_disk(stats, drive, count);
	if (status < 0)
	    continue;
	IOObjectRelease(drive);
	count++;
    }
    IOIteratorReset(mach_device_list);

    if (count)
	status = update_disk_indom(stats, count, indom);
    return status;
}

int
fetch_disk(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
	extern diskstats_t mach_disk;
	extern int mach_disk_error;
	extern pmdaIndom indomtab[];

	if (mach_disk_error)
		return mach_disk_error;
	if (item == 46) {	/* hinv.ndisk */
		atom->ul = indomtab[DISK_INDOM].it_numinst;
		return 1;
	}
	if (indomtab[DISK_INDOM].it_numinst == 0)
		return 0;	/* no values available */
	if (item < 59 && (inst < 0 || inst >= indomtab[DISK_INDOM].it_numinst))
		return PM_ERR_INST;
	switch (item) {
	case 47: /* disk.dev.read */
		atom->ull = mach_disk.disks[inst].read;
		return 1;
	case 48: /* disk.dev.write */
		atom->ull = mach_disk.disks[inst].write;
		return 1;
	case 49: /* disk.dev.total */
		atom->ull = mach_disk.disks[inst].read + mach_disk.disks[inst].write;
		return 1;
	case 50: /* disk.dev.read_bytes */
		atom->ull = mach_disk.disks[inst].read_bytes >> 10;
		return 1;
	case 51: /* disk.dev.write_bytes */
		atom->ull = mach_disk.disks[inst].write_bytes >> 10;
		return 1;
	case 52: /* disk.dev.total_bytes */
		atom->ull = (mach_disk.disks[inst].read_bytes +
				mach_disk.disks[inst].write_bytes) >> 10;
		return 1;
	case 53: /* disk.dev.blkread */
		atom->ull = mach_disk.disks[inst].read_bytes /
				mach_disk.disks[inst].blocksize;
		return 1;
	case 54: /* disk.dev.blkwrite */
		atom->ull = mach_disk.disks[inst].write_bytes /
				mach_disk.disks[inst].blocksize;
		return 1;
	case 55: /* disk.dev.blktotal */
		atom->ull = (mach_disk.disks[inst].read_bytes +
				 mach_disk.disks[inst].write_bytes) /
					mach_disk.disks[inst].blocksize;
		return 1;
	case 56: /* disk.dev.read_time */
		atom->ull = mach_disk.disks[inst].read_time;
		return 1;
	case 57: /* disk.dev.write_time */
		atom->ull = mach_disk.disks[inst].write_time;
		return 1;
	case 58: /* disk.dev.total_time */
		atom->ull = mach_disk.disks[inst].read_time +
					mach_disk.disks[inst].write_time;
		return 1;
	case 71: /* disk.dev.read_errors */
		atom->ull = mach_disk.disks[inst].read_errors;
		return 1;
	case 72: /* disk.dev.write_errors */
		atom->ull = mach_disk.disks[inst].write_errors;
		return 1;
	case 73: /* disk.dev.read_retries */
		atom->ull = mach_disk.disks[inst].read_retries;
		return 1;
	case 74: /* disk.dev.write_retries */
		atom->ull = mach_disk.disks[inst].write_retries;
		return 1;
	case 75: /* disk.dev.total_read_time */
		atom->ull = mach_disk.disks[inst].total_read_time;
		return 1;
	case 76: /* disk.dev.total_write_time */
		atom->ull = mach_disk.disks[inst].total_write_time;
		return 1;
	case 77: /* disk.dev.avgrq_sz - derived metric */
		{
			__uint64_t total_ops = mach_disk.disks[inst].read +
						mach_disk.disks[inst].write;
			if (total_ops > 0) {
				atom->ull = (mach_disk.disks[inst].read_bytes +
						mach_disk.disks[inst].write_bytes) / total_ops;
			} else {
				atom->ull = 0;
			}
		}
		return 1;
	case 78: /* disk.dev.await - derived metric (in microseconds) */
		{
			__uint64_t total_ops = mach_disk.disks[inst].read +
						mach_disk.disks[inst].write;
			if (total_ops > 0) {
				atom->ull = (mach_disk.disks[inst].total_read_time +
						mach_disk.disks[inst].total_write_time) / total_ops;
			} else {
				atom->ull = 0;
			}
		}
		return 1;
	case 59: /* disk.all.read */
		atom->ull = mach_disk.read;
		return 1;
	case 60: /* disk.all.write */
		atom->ull = mach_disk.write;
		return 1;
	case 61: /* disk.all.total */
		atom->ull = mach_disk.read + mach_disk.write;
		return 1;
	case 62: /* disk.all.read_bytes */
		atom->ull = mach_disk.read_bytes >> 10;
		return 1;
	case 63: /* disk.all.write_bytes */
		atom->ull = mach_disk.write_bytes >> 10;
		return 1;
	case 64: /* disk.all.total_bytes */
		atom->ull = (mach_disk.read_bytes + mach_disk.write_bytes) >> 10;
		return 1;
	case 65: /* disk.all.blkread */
		atom->ull = mach_disk.blkread;
		return 1;
	case 66: /* disk.all.blkwrite */
		atom->ull = mach_disk.blkwrite;
		return 1;
	case 67: /* disk.all.blktotal */
		atom->ull = mach_disk.blkread + mach_disk.blkwrite;
		return 1;
	case 68: /* disk.all.read_time */
		atom->ull = mach_disk.read_time;
		return 1;
	case 69: /* disk.all.write_time */
		atom->ull = mach_disk.write_time;
		return 1;
	case 70: /* disk.all.total_time */
		atom->ull = mach_disk.read_time + mach_disk.write_time;
		return 1;
	case 79: /* disk.all.read_errors */
		atom->ull = mach_disk.read_errors;
		return 1;
	case 80: /* disk.all.write_errors */
		atom->ull = mach_disk.write_errors;
		return 1;
	case 81: /* disk.all.read_retries */
		atom->ull = mach_disk.read_retries;
		return 1;
	case 82: /* disk.all.write_retries */
		atom->ull = mach_disk.write_retries;
		return 1;
	case 83: /* disk.all.total_read_time */
		atom->ull = mach_disk.total_read_time;
		return 1;
	case 84: /* disk.all.total_write_time */
		atom->ull = mach_disk.total_write_time;
		return 1;
	case 85: /* disk.all.avgrq_sz - derived metric */
		{
			__uint64_t total_ops = mach_disk.read + mach_disk.write;
			if (total_ops > 0) {
				atom->ull = (mach_disk.read_bytes + mach_disk.write_bytes) / total_ops;
			} else {
				atom->ull = 0;
			}
		}
		return 1;
	case 86: /* disk.all.await - derived metric (in microseconds) */
		{
			__uint64_t total_ops = mach_disk.read + mach_disk.write;
			if (total_ops > 0) {
				atom->ull = (mach_disk.total_read_time + mach_disk.total_write_time) / total_ops;
			} else {
				atom->ull = 0;
			}
		}
		return 1;
	}
	return PM_ERR_PMID;
}
