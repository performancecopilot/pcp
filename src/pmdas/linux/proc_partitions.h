/*
 * Linux /proc/partitions metrics cluster
 *
 * Copyright (c) 2016,2020 Red Hat.
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
    unsigned long long	failed_reads;
    unsigned long long	failed_writes;
    unsigned long long	invalid_io;
    unsigned long long	notify_free;
} zram_io_stat_t;

typedef struct {
    unsigned long long	original;
    unsigned long long	compressed;
    unsigned long long	mem_used;
    unsigned long long	mem_limit;
    unsigned long long	max_used;
    unsigned long long	same_pages;
    unsigned long long	compacted_pages;
    unsigned long long	huge_pages;
} zram_mm_stat_t;

typedef struct {
    unsigned long long	count;
    unsigned long long	reads;
    unsigned long long	writes;
} zram_bd_stat_t;

typedef enum {
    ZRAM_IO = 0x1, ZRAM_MM = 0x2, ZRAM_BD = 0x4, ZRAM_SIZE = 0x8
} zram_state_t;

typedef struct {
    zram_state_t	uptodate;
    zram_io_stat_t	iostat;
    zram_mm_stat_t	mmstat;
    zram_bd_stat_t	bdstat;
} zram_stat_t;

typedef struct {
    int			id;
    unsigned int	major;
    unsigned int	minor;
    unsigned long long	nr_blocks;    /* from /proc/partitions */
    char		*namebuf;     /* from /proc/{partitions,diskstats} */
    char		*udevnamebuf; /* from udev if we have it, else NULL */
    char		*dmname;      /* symlink from /dev/mapper, else NULL */
    char		*mdname;      /* symlink from /dev/md, else NULL */
    char		*wwidname;    /* wwid of sd path, else NULL */
    char		*ctlr;        /* from /sys/block symlink, else NULL */
    char		*model;       /* from /sys/devices/..., else NULL */
    zram_stat_t		*zram;
    unsigned long long	rd_ios;
    unsigned long long	rd_merges;
    unsigned long long	rd_sectors;
    unsigned int	rd_ticks;
    unsigned long long	wr_ios;
    unsigned long long	wr_merges;
    unsigned long long	wr_sectors;
    unsigned int	wr_ticks;
    unsigned int	ios_in_flight;
    unsigned int	io_ticks;
    unsigned int	aveq;
    unsigned long long	ds_ios;
    unsigned long long	ds_merges;
    unsigned long long	ds_sectors;
    unsigned int	ds_ticks;
    unsigned long long	fl_ios;
    unsigned int	fl_ticks;
} partitions_entry_t;

extern int refresh_proc_partitions(pmInDom, pmInDom, pmInDom, pmInDom, pmInDom, pmInDom, int, int);
extern int is_partitions_metric(pmID);
extern int is_capacity_metric(int, int);
extern int proc_partitions_fetch(pmdaMetric *, unsigned int, pmAtomValue *);
