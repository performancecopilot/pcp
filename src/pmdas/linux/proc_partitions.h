/*
 * Linux /proc/partitions metrics cluster
 *
 * Copyright (c) 2016 Red Hat.
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

typedef struct {
    int			id;
    unsigned int	major;
    unsigned int	minor;
    unsigned long long	nr_blocks;    /* from /proc/partitions */
    char		*namebuf;     /* from /proc/{partitions,diskstats} */
    char		*udevnamebuf; /* from udev if we have it, else NULL */
    char		*dmname;      /* symlink from /dev/mapper, else NULL */
    char		*mdname;      /* symlink from /dev/md, else NULL */
    unsigned long	rd_ios;
    unsigned long	rd_merges;
    unsigned long long	rd_sectors;
    unsigned int	rd_ticks;
    unsigned long	wr_ios;
    unsigned long	wr_merges;
    unsigned long long	wr_sectors;
    unsigned int	wr_ticks;
    unsigned int	ios_in_flight;
    unsigned int	io_ticks;
    unsigned int	aveq;
} partitions_entry_t;

extern int refresh_proc_partitions(pmInDom disk_indom, pmInDom partitions_indom, pmInDom dm_indom, pmInDom md_indom, int, int);
extern int is_partitions_metric(pmID);
extern int is_capacity_metric(int, int);
extern int proc_partitions_fetch(pmdaMetric *, unsigned int, pmAtomValue *);
