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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __PMDASOLARIS_COMMON_H
#define __PMDASOLARIS_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"
#include "domain.h"
#include "clusters.h"

#include <kstat.h>
#include <sys/sysinfo.h>

typedef struct {
    const char	*m_name;
    void	(*m_init)(int);
    void	(*m_prefetch)(void);
    int		(*m_fetch)(pmdaMetric *, int, pmAtomValue *);
    int		m_fetched;
    uint64_t	m_elapsed;
    uint64_t	m_hits;
} method_t;

extern method_t		methodtab[];
extern const int	methodtab_sz;

extern void init_data(int);

extern void sysinfo_init(int);
extern void sysinfo_prefetch(void);
extern int sysinfo_fetch(pmdaMetric *, int, pmAtomValue *);

extern void disk_init(int);
extern void disk_prefetch(void);
extern int disk_fetch(pmdaMetric *, int, pmAtomValue *);

void zpool_init(int);
void zpool_refresh(void);
int zpool_fetch(pmdaMetric *, int, pmAtomValue *);

void zfs_init(int);
void zfs_refresh(void);
int zfs_fetch(pmdaMetric *, int, pmAtomValue *);

void zpool_perdisk_init(int);
void zpool_perdisk_refresh(void);
int zpool_perdisk_fetch(pmdaMetric *, int, pmAtomValue *);

void netlink_init(int);
void netlink_refresh(void);
int netlink_fetch(pmdaMetric *, int, pmAtomValue *);

void kvm_init(int);
void kvm_refresh(void);
int kvm_fetch(pmdaMetric *, int, pmAtomValue *);

void arcstats_refresh(void);
int arcstats_fetch(pmdaMetric *, int, pmAtomValue *);

void vnops_init(int);
void vnops_refresh(void);
int vnops_fetch(pmdaMetric *, int, pmAtomValue *);

/*
 * metric descriptions
 */
typedef struct {
    const char	*md_name;
    pmDesc	md_desc;	// PMDA's idea of the semantics
    ptrdiff_t	md_offset;	// offset into kstat stats structure
    uint64_t	md_elapsed;
    uint64_t	md_hits;
} metricdesc_t;

extern metricdesc_t	metricdesc[];
extern pmdaMetric	*metrictab;
extern int		metrictab_sz;

#define DISK_INDOM	0
#define CPU_INDOM	1
#define NETIF_INDOM	2
#define ZPOOL_INDOM	3
#define ZFS_INDOM	4
#define ZPOOL_PERDISK_INDOM	5
#define NETLINK_INDOM	6
#define ZFS_SNAP_INDOM	7
#define LOADAVG_INDOM	8
#define PREFETCH_INDOM	9
#define METRIC_INDOM	10
#define FILESYS_INDOM	11
#define FSTYPE_INDOM	12

extern pmdaIndom	indomtab[];
extern int		indomtab_sz;

/*
 * kstat() control
 */
kstat_ctl_t *kstat_ctl_update(void);
void kstat_ctl_needs_update(void);
int kstat_named_to_pmAtom(const kstat_named_t *, pmAtomValue *);
int kstat_named_to_typed_atom(const kstat_named_t *, int, pmAtomValue *);

/* Snarfed from usr/src/uts/common/fs/fsflush.c in OpenSolaris source tree */
typedef struct {
        ulong_t fsf_scan;       /* number of pages scanned */
        ulong_t fsf_examined;   /* number of page_t's actually examined, can */
                                /* be less than fsf_scan due to large pages */
        ulong_t fsf_locked;     /* pages we actually page_lock()ed */
        ulong_t fsf_modified;   /* number of modified pages found */
        ulong_t fsf_coalesce;   /* number of page coalesces done */
        ulong_t fsf_time;       /* nanoseconds of run time */
        ulong_t fsf_releases;   /* number of page_release() done */
} fsf_stat_t;

#endif
