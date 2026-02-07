/*
 * APFS statistics for Darwin PMDA - IOKit integration
 *
 * Copyright (c) 2026 Paul Smith.
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
#include <errno.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include "pmapi.h"
#include "pmda.h"

#include "darwin.h"
#include "apfs.h"

extern pmdaIndom indomtab[];

/*
 * Ensure we have space for containers in our pre-allocated pool
 */
static int
check_container_size(struct apfs_stats *stats, int count)
{
    if (count > stats->container_highwater) {
        if (stats->container_highwater == 0)
            stats->container_highwater = 4;
        else
            stats->container_highwater <<= 1;
        stats->containers = realloc(stats->containers,
                    stats->container_highwater * sizeof(struct apfs_container_stat));
        if (!stats->containers) {
            stats->container_highwater = 0;
            return -ENOMEM;
        }
    }
    return 0;
}

/*
 * Ensure we have space for volumes in our pre-allocated pool
 */
static int
check_volume_size(struct apfs_stats *stats, int count)
{
    if (count > stats->volume_highwater) {
        if (stats->volume_highwater == 0)
            stats->volume_highwater = 4;
        else
            stats->volume_highwater <<= 1;
        stats->volumes = realloc(stats->volumes,
                    stats->volume_highwater * sizeof(struct apfs_volume_stat));
        if (!stats->volumes) {
            stats->volume_highwater = 0;
            return -ENOMEM;
        }
    }
    return 0;
}

/*
 * Update container instance domain
 */
static int
update_container_indom(struct apfs_stats *stats, int count, pmdaIndom *indom)
{
    int i;
    pmdaInstid *instids;

    if (count == 0) {
        indom->it_numinst = 0;
        indom->it_set = NULL;
        return 0;
    }

    instids = malloc(count * sizeof(pmdaInstid));
    if (!instids)
        return -ENOMEM;

    for (i = 0; i < count; i++) {
        instids[i].i_inst = i;
        instids[i].i_name = stats->containers[i].name;
    }

    if (indom->it_set)
        free(indom->it_set);
    indom->it_numinst = count;
    indom->it_set = instids;
    return 0;
}

/*
 * Update volume instance domain
 */
static int
update_volume_indom(struct apfs_stats *stats, int count, pmdaIndom *indom)
{
    int i;
    pmdaInstid *instids;

    if (count == 0) {
        indom->it_numinst = 0;
        indom->it_set = NULL;
        return 0;
    }

    instids = malloc(count * sizeof(pmdaInstid));
    if (!instids)
        return -ENOMEM;

    for (i = 0; i < count; i++) {
        instids[i].i_inst = i;
        instids[i].i_name = stats->volumes[i].name;
    }

    if (indom->it_set)
        free(indom->it_set);
    indom->it_numinst = count;
    indom->it_set = instids;
    return 0;
}

/*
 * Get uint64 value from CFDictionary
 */
static uint64_t
get_cf_uint64(CFDictionaryRef dict, CFStringRef key)
{
    CFNumberRef number;
    uint64_t value = 0;

    number = CFDictionaryGetValue(dict, key);
    if (number && CFGetTypeID(number) == CFNumberGetTypeID()) {
        CFNumberGetValue(number, kCFNumberSInt64Type, &value);
    }
    return value;
}

/*
 * Get int value from CFDictionary (for booleans)
 */
static int
get_cf_int(CFDictionaryRef dict, CFStringRef key)
{
    CFNumberRef number;
    CFBooleanRef boolean;
    int value = 0;

    /* Try as CFNumber first */
    number = CFDictionaryGetValue(dict, key);
    if (number && CFGetTypeID(number) == CFNumberGetTypeID()) {
        CFNumberGetValue(number, kCFNumberIntType, &value);
        return value;
    }

    /* Try as CFBoolean */
    boolean = CFDictionaryGetValue(dict, key);
    if (boolean && CFGetTypeID(boolean) == CFBooleanGetTypeID()) {
        return CFBooleanGetValue(boolean) ? 1 : 0;
    }

    return 0;
}

/*
 * Extract APFS container statistics from IOKit
 */
static int
refresh_apfs_containers(struct apfs_stats *stats)
{
    io_iterator_t iter;
    io_registry_entry_t entry;
    CFDictionaryRef props;
    CFDictionaryRef statistics;
    int count = 0;
    int status;

    status = IOServiceGetMatchingServices(kIOMainPortDefault,
                IOServiceMatching("AppleAPFSContainer"), &iter);
    if (status != KERN_SUCCESS)
        return 0;  /* No APFS containers found - not an error */

    while ((entry = IOIteratorNext(iter))) {
        status = check_container_size(stats, count + 1);
        if (status < 0) {
            IOObjectRelease(entry);
            IOObjectRelease(iter);
            return status;
        }

        status = IORegistryEntryCreateCFProperties(entry, (CFMutableDictionaryRef *)&props,
                    kCFAllocatorDefault, kNilOptions);
        if (status != KERN_SUCCESS) {
            IOObjectRelease(entry);
            continue;
        }

        /* Initialize the container stats */
        memset(&stats->containers[count], 0, sizeof(struct apfs_container_stat));

        /* Generate container name */
        snprintf(stats->containers[count].name, APFS_NAME_MAX,
                "container%d", count);

        /* Get block size */
        stats->containers[count].block_size =
            get_cf_uint64(props, CFSTR("ContainerBlockSize"));

        /* Get statistics dictionary */
        statistics = CFDictionaryGetValue(props, CFSTR("Statistics"));
        if (statistics && CFGetTypeID(statistics) == CFDictionaryGetTypeID()) {
            stats->containers[count].bytes_read =
                get_cf_uint64(statistics, CFSTR("Bytes read from block device"));
            stats->containers[count].bytes_written =
                get_cf_uint64(statistics, CFSTR("Bytes written to block device"));
            stats->containers[count].read_requests =
                get_cf_uint64(statistics, CFSTR("Read requests sent to block device"));
            stats->containers[count].write_requests =
                get_cf_uint64(statistics, CFSTR("Write requests sent to block device"));
            stats->containers[count].transactions =
                get_cf_uint64(statistics, CFSTR("Number of transactions flushed"));
            stats->containers[count].cache_hits =
                get_cf_uint64(statistics, CFSTR("Object cache: Number of hits"));
            stats->containers[count].cache_evictions =
                get_cf_uint64(statistics, CFSTR("Object cache: Number of evictions"));
            stats->containers[count].read_errors =
                get_cf_uint64(statistics, CFSTR("Metadata: Number of read errors"));
            stats->containers[count].write_errors =
                get_cf_uint64(statistics, CFSTR("Metadata: Number of write errors"));
        }

        CFRelease(props);
        IOObjectRelease(entry);
        count++;
    }

    IOObjectRelease(iter);
    return count;
}

/*
 * Extract APFS volume statistics from IOKit
 */
static int
refresh_apfs_volumes(struct apfs_stats *stats)
{
    io_iterator_t iter;
    io_registry_entry_t entry;
    CFDictionaryRef props;
    CFStringRef name_ref;
    int count = 0;
    int status;

    status = IOServiceGetMatchingServices(kIOMainPortDefault,
                IOServiceMatching("AppleAPFSVolume"), &iter);
    if (status != KERN_SUCCESS)
        return 0;  /* No APFS volumes found - not an error */

    while ((entry = IOIteratorNext(iter))) {
        status = check_volume_size(stats, count + 1);
        if (status < 0) {
            IOObjectRelease(entry);
            IOObjectRelease(iter);
            return status;
        }

        status = IORegistryEntryCreateCFProperties(entry, (CFMutableDictionaryRef *)&props,
                    kCFAllocatorDefault, kNilOptions);
        if (status != KERN_SUCCESS) {
            IOObjectRelease(entry);
            continue;
        }

        /* Initialize the volume stats */
        memset(&stats->volumes[count], 0, sizeof(struct apfs_volume_stat));

        /* Get volume name (BSD name or volume name) */
        name_ref = CFDictionaryGetValue(props, CFSTR("BSD Name"));
        if (!name_ref)
            name_ref = CFDictionaryGetValue(props, CFSTR("Volume Name"));
        if (name_ref && CFGetTypeID(name_ref) == CFStringGetTypeID()) {
            CFStringGetCString(name_ref, stats->volumes[count].name,
                              APFS_NAME_MAX, kCFStringEncodingUTF8);
        } else {
            /* Fallback to generated name */
            snprintf(stats->volumes[count].name, APFS_NAME_MAX,
                    "volume%d", count);
        }

        /* Get encrypted and locked status */
        stats->volumes[count].encrypted =
            get_cf_int(props, CFSTR("Encrypted"));
        stats->volumes[count].locked =
            get_cf_int(props, CFSTR("Locked"));

        CFRelease(props);
        IOObjectRelease(entry);
        count++;
    }

    IOObjectRelease(iter);
    return count;
}

/*
 * Refresh APFS statistics
 * Called before each metric fetch
 */
int
refresh_apfs(struct apfs_stats *stats,
             pmdaIndom *container_indom,
             pmdaIndom *volume_indom)
{
    int container_count, volume_count;
    int status;

    /* Refresh container statistics */
    container_count = refresh_apfs_containers(stats);
    if (container_count < 0)
        return container_count;
    stats->container_count = container_count;

    /* Refresh volume statistics */
    volume_count = refresh_apfs_volumes(stats);
    if (volume_count < 0)
        return volume_count;
    stats->volume_count = volume_count;

    /* Update instance domains */
    status = update_container_indom(stats, container_count, container_indom);
    if (status < 0)
        return status;

    status = update_volume_indom(stats, volume_count, volume_indom);
    if (status < 0)
        return status;

    return 0;
}

/*
 * Fetch APFS metrics
 */
int
fetch_apfs(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    extern struct apfs_stats mach_apfs;
    extern int mach_apfs_error;
    extern pmdaIndom indomtab[];

    if (mach_apfs_error)
        return mach_apfs_error;

    switch (item) {
    case 87: /* disk.apfs.ncontainer */
        atom->ul = mach_apfs.container_count;
        return 1;

    case 88: /* disk.apfs.nvolume */
        atom->ul = mach_apfs.volume_count;
        return 1;

    /* Container metrics require valid instance */
    case 89: /* disk.apfs.container.block_size */
        if (inst < 0 || inst >= mach_apfs.container_count)
            return PM_ERR_INST;
        atom->ull = mach_apfs.containers[inst].block_size;
        return 1;

    case 90: /* disk.apfs.container.bytes_read */
        if (inst < 0 || inst >= mach_apfs.container_count)
            return PM_ERR_INST;
        atom->ull = mach_apfs.containers[inst].bytes_read;
        return 1;

    case 91: /* disk.apfs.container.bytes_written */
        if (inst < 0 || inst >= mach_apfs.container_count)
            return PM_ERR_INST;
        atom->ull = mach_apfs.containers[inst].bytes_written;
        return 1;

    case 92: /* disk.apfs.container.read_requests */
        if (inst < 0 || inst >= mach_apfs.container_count)
            return PM_ERR_INST;
        atom->ull = mach_apfs.containers[inst].read_requests;
        return 1;

    case 93: /* disk.apfs.container.write_requests */
        if (inst < 0 || inst >= mach_apfs.container_count)
            return PM_ERR_INST;
        atom->ull = mach_apfs.containers[inst].write_requests;
        return 1;

    case 94: /* disk.apfs.container.transactions */
        if (inst < 0 || inst >= mach_apfs.container_count)
            return PM_ERR_INST;
        atom->ull = mach_apfs.containers[inst].transactions;
        return 1;

    case 95: /* disk.apfs.container.cache_hits */
        if (inst < 0 || inst >= mach_apfs.container_count)
            return PM_ERR_INST;
        atom->ull = mach_apfs.containers[inst].cache_hits;
        return 1;

    case 96: /* disk.apfs.container.cache_evictions */
        if (inst < 0 || inst >= mach_apfs.container_count)
            return PM_ERR_INST;
        atom->ull = mach_apfs.containers[inst].cache_evictions;
        return 1;

    case 97: /* disk.apfs.container.read_errors */
        if (inst < 0 || inst >= mach_apfs.container_count)
            return PM_ERR_INST;
        atom->ull = mach_apfs.containers[inst].read_errors;
        return 1;

    case 98: /* disk.apfs.container.write_errors */
        if (inst < 0 || inst >= mach_apfs.container_count)
            return PM_ERR_INST;
        atom->ull = mach_apfs.containers[inst].write_errors;
        return 1;

    /* Volume metrics require valid instance */
    case 99: /* disk.apfs.volume.encrypted */
        if (inst < 0 || inst >= mach_apfs.volume_count)
            return PM_ERR_INST;
        atom->ul = mach_apfs.volumes[inst].encrypted;
        return 1;

    case 100: /* disk.apfs.volume.locked */
        if (inst < 0 || inst >= mach_apfs.volume_count)
            return PM_ERR_INST;
        atom->ul = mach_apfs.volumes[inst].locked;
        return 1;
    }

    return PM_ERR_PMID;
}
