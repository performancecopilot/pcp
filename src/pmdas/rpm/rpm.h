/*
 * Copyright (c) 2013-2014 Red Hat.
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
#ifndef RPM_H
#define RPM_H

/*
 * Instance domain handling
 */
enum {
    RPM_INDOM			= 0,	/* active RPM packages */
    CACHE_INDOM			= 1,	/* pseudo-indom for refreshing */
    STRINGS_INDOM		= 2,	/* pseudo-indom for string sharing */
};

/*
 * Metrics describing internals of pmdarpm operation  (Cluster 0)
 */
enum {
    REFRESH_COUNT_ID		= 0,
    REFRESH_TIME_USER_ID	= 1,
    REFRESH_TIME_KERNEL_ID	= 2,
    REFRESH_TIME_ELAPSED_ID	= 3,
    DATASIZE_ID			= 4,
};

/*
 * List of metrics corresponding to rpm --querytags  (Cluster 1)
 */
enum {
    ARCH_ID			= 0,
    BUILDHOST_ID		= 1,
    BUILDTIME_ID		= 2,
    DESCRIPTION_ID		= 3,
    EPOCH_ID			= 4,
    GROUP_ID			= 5,
    INSTALLTIME_ID		= 6,
    LICENSE_ID			= 7,
    PACKAGER_ID			= 8,
    RELEASE_ID			= 9,
    SIZE_ID			= 10,
    SOURCERPM_ID		= 11,
    SUMMARY_ID			= 12,
    URL_ID			= 13,
    VENDOR_ID			= 14,
    VERSION_ID			= 15,
    NAME_ID			= 16,
};

/*
 * Metrics describing cumulative pmdarpm totals  (Cluster 2)
 */
enum {
    TOTAL_COUNT_ID		= 0,
    TOTAL_BYTES_ID		= 1,
};

/*
 * Package metadata stored for each installed RPM
 *
 * A "refresh" count is stored to indicate whether this entry
 * is out of date with respect to the global "refresh" count.
 * If its value is greater-than-or-equal-to a global refresh
 * count, the entry is current - otherwise it is out-of-date
 * and must not be reported in the active instance domain.
 *
 * Note that many of the structure entries (below) are string
 * dictionary keys (int), allowing sharing of the memory used
 * to hold the values.  It also further reduces the footprint
 * on 64 bit systems, instead of storing 64bit pointers.
 */

typedef struct metadata {
    int		name;
    int		arch;
    int		buildhost;
    int		buildtime;
    int		description;
    int		epoch;
    int		group;
    int		installtime;
    int		license;
    int		packager;
    int		release;
    __uint64_t	longsize;
    int		sourcerpm;
    int		summary;
    int		url;
    int		vendor;
    int		version;
} metadata;
    
typedef struct package {
    __uint64_t	refresh;
    metadata	values;
} package;

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

#endif	/* RPM_H */
