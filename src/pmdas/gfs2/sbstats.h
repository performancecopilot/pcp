/*
 * GFS2 sbstats sysfs file statistics.
 *
 * Copyright (c) 2013 Red Hat.
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

#ifndef SBSTATS_H
#define SBSTATS_H

enum {
	LOCKSTAT_SRTT = 0,	/* Non-blocking smoothed round trip time */
	LOCKSTAT_SRTTVAR,	/* Non-blocking smoothed variance */
	LOCKSTAT_SRTTB,		/* Blocking smoothed round trip time */
	LOCKSTAT_SRTTVARB,	/* Blocking smoothed variance */
	LOCKSTAT_SIRT,		/* Smoothed inter-request time */
	LOCKSTAT_SIRTVAR,	/* Smoothed inter-request variance */
	LOCKSTAT_DCOUNT,	/* Count of DLM requests */
	LOCKSTAT_QCOUNT,	/* Count of gfs2_holder queues */
	NUM_LOCKSTATS 
};

enum {
	LOCKTYPE_RESERVED = 0,
	LOCKTYPE_NONDISK,
	LOCKTYPE_INODE,
	LOCKTYPE_RGRB,
	LOCKTYPE_META,
	LOCKTYPE_IOPEN,
	LOCKTYPE_FLOCK,
	LOCKTYPE_PLOCK,
	LOCKTYPE_QUOTA,
	LOCKTYPE_JOURNAL,
	NUM_LOCKTYPES 
};

#define SBSTATS_COUNT	(NUM_LOCKSTATS*NUM_LOCKTYPES)

struct sbstats {
	__uint64_t	values[SBSTATS_COUNT];
};

extern void gfs2_sbstats_init(pmdaExt *, pmdaMetric *, int);
extern int gfs2_sbstats_fetch(int, struct sbstats *, pmAtomValue *);
extern int gfs2_refresh_sbstats(const char *, const char *, struct sbstats *);

#endif	/* SBSTATS_H */
