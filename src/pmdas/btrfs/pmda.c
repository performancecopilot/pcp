/*
 * Btrfs PMDA for Linux
 *
 * Copyright (c) 2026 Red Hat.
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

#include <stdint.h>

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "domain.h"
#include "clusters.h"
#include "btrfs_utils.h"
#include "btrfs_fs.h"
#include "btrfs_dev.h"

static char BTRFS_DEFAULT_PATH[] = "/sys/fs/btrfs";

static int _isDSO = 1;

static pmdaIndom indomtab[] = {
    { .it_indom = BTRFS_FS_INDOM },
    { .it_indom = BTRFS_DEV_INDOM },
};

#define INDOM(x) (indomtab[x].it_indom)

static pmdaMetric metrictab[] = {
/*---------------------------------------------------------------------------*/
/*  FS INFO (cluster 0) - per-filesystem  */
/*---------------------------------------------------------------------------*/
/* label */
    { NULL,
      { PMDA_PMID(BTRFS_INFO_CLUST, BTRFS_INFO_LABEL), PM_TYPE_STRING, BTRFS_FS_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* nodesize */
    { NULL,
      { PMDA_PMID(BTRFS_INFO_CLUST, BTRFS_INFO_NODESIZE), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* sectorsize */
    { NULL,
      { PMDA_PMID(BTRFS_INFO_CLUST, BTRFS_INFO_SECTORSIZE), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* generation */
    { NULL,
      { PMDA_PMID(BTRFS_INFO_CLUST, BTRFS_INFO_GENERATION), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* checksum */
    { NULL,
      { PMDA_PMID(BTRFS_INFO_CLUST, BTRFS_INFO_CHECKSUM), PM_TYPE_STRING, BTRFS_FS_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* metadata_uuid */
    { NULL,
      { PMDA_PMID(BTRFS_INFO_CLUST, BTRFS_INFO_METADATA_UUID), PM_TYPE_STRING, BTRFS_FS_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },

/*---------------------------------------------------------------------------*/
/*  COMMIT STATS (cluster 1) - per-filesystem  */
/*---------------------------------------------------------------------------*/
/* count */
    { NULL,
      { PMDA_PMID(BTRFS_COMMIT_CLUST, BTRFS_COMMIT_COUNT), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* last_time */
    { NULL,
      { PMDA_PMID(BTRFS_COMMIT_CLUST, BTRFS_COMMIT_LAST_TIME), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0) } },
/* max_time */
    { NULL,
      { PMDA_PMID(BTRFS_COMMIT_CLUST, BTRFS_COMMIT_MAX_TIME), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0) } },
/* total_time */
    { NULL,
      { PMDA_PMID(BTRFS_COMMIT_CLUST, BTRFS_COMMIT_TOTAL_TIME), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0) } },

/*---------------------------------------------------------------------------*/
/*  ALLOCATION DATA (cluster 2) - per-filesystem  */
/*---------------------------------------------------------------------------*/
/* total_bytes */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_DATA_CLUST, BTRFS_ALLOC_TOTAL_BYTES), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* bytes_used */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_DATA_CLUST, BTRFS_ALLOC_BYTES_USED), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* bytes_pinned */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_DATA_CLUST, BTRFS_ALLOC_BYTES_PINNED), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* bytes_reserved */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_DATA_CLUST, BTRFS_ALLOC_BYTES_RESERVED), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* bytes_readonly */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_DATA_CLUST, BTRFS_ALLOC_BYTES_READONLY), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* disk_used */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_DATA_CLUST, BTRFS_ALLOC_DISK_USED), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* disk_total */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_DATA_CLUST, BTRFS_ALLOC_DISK_TOTAL), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/*---------------------------------------------------------------------------*/
/*  ALLOCATION METADATA (cluster 3) - per-filesystem  */
/*---------------------------------------------------------------------------*/
/* total_bytes */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_META_CLUST, BTRFS_ALLOC_TOTAL_BYTES), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* bytes_used */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_META_CLUST, BTRFS_ALLOC_BYTES_USED), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* bytes_pinned */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_META_CLUST, BTRFS_ALLOC_BYTES_PINNED), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* bytes_reserved */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_META_CLUST, BTRFS_ALLOC_BYTES_RESERVED), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* bytes_readonly */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_META_CLUST, BTRFS_ALLOC_BYTES_READONLY), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* disk_used */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_META_CLUST, BTRFS_ALLOC_DISK_USED), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* disk_total */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_META_CLUST, BTRFS_ALLOC_DISK_TOTAL), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/*---------------------------------------------------------------------------*/
/*  ALLOCATION SYSTEM (cluster 4) - per-filesystem  */
/*---------------------------------------------------------------------------*/
/* total_bytes */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_SYS_CLUST, BTRFS_ALLOC_TOTAL_BYTES), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* bytes_used */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_SYS_CLUST, BTRFS_ALLOC_BYTES_USED), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* bytes_pinned */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_SYS_CLUST, BTRFS_ALLOC_BYTES_PINNED), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* bytes_reserved */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_SYS_CLUST, BTRFS_ALLOC_BYTES_RESERVED), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* bytes_readonly */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_SYS_CLUST, BTRFS_ALLOC_BYTES_READONLY), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* disk_used */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_SYS_CLUST, BTRFS_ALLOC_DISK_USED), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* disk_total */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_SYS_CLUST, BTRFS_ALLOC_DISK_TOTAL), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/*---------------------------------------------------------------------------*/
/*  ALLOCATION GLOBAL RSV (cluster 5) - per-filesystem  */
/*---------------------------------------------------------------------------*/
/* global_rsv_size */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_GLOBAL_CLUST, BTRFS_GLOBAL_RSV_SIZE), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* global_rsv_reserved */
    { NULL,
      { PMDA_PMID(BTRFS_ALLOC_GLOBAL_CLUST, BTRFS_GLOBAL_RSV_RESERVED), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/*---------------------------------------------------------------------------*/
/*  DEVICE ERROR STATS (cluster 6) - per-device  */
/*---------------------------------------------------------------------------*/
/* write_errs */
    { NULL,
      { PMDA_PMID(BTRFS_DEV_CLUST, BTRFS_DEV_WRITE_ERRS), PM_TYPE_U64, BTRFS_DEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* read_errs */
    { NULL,
      { PMDA_PMID(BTRFS_DEV_CLUST, BTRFS_DEV_READ_ERRS), PM_TYPE_U64, BTRFS_DEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* flush_errs */
    { NULL,
      { PMDA_PMID(BTRFS_DEV_CLUST, BTRFS_DEV_FLUSH_ERRS), PM_TYPE_U64, BTRFS_DEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* corruption_errs */
    { NULL,
      { PMDA_PMID(BTRFS_DEV_CLUST, BTRFS_DEV_CORRUPTION_ERRS), PM_TYPE_U64, BTRFS_DEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* generation_errs */
    { NULL,
      { PMDA_PMID(BTRFS_DEV_CLUST, BTRFS_DEV_GENERATION_ERRS), PM_TYPE_U64, BTRFS_DEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/*---------------------------------------------------------------------------*/
/*  DISCARD STATS (cluster 7) - per-filesystem  */
/*---------------------------------------------------------------------------*/
/* discardable_bytes */
    { NULL,
      { PMDA_PMID(BTRFS_DISCARD_CLUST, BTRFS_DISCARD_DISCARDABLE_BYTES), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* discardable_extents */
    { NULL,
      { PMDA_PMID(BTRFS_DISCARD_CLUST, BTRFS_DISCARD_DISCARDABLE_EXTENTS), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* discard_bitmap_bytes */
    { NULL,
      { PMDA_PMID(BTRFS_DISCARD_CLUST, BTRFS_DISCARD_BITMAP_BYTES), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* discard_extent_bytes */
    { NULL,
      { PMDA_PMID(BTRFS_DISCARD_CLUST, BTRFS_DISCARD_EXTENT_BYTES), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* discard_bytes_saved */
    { NULL,
      { PMDA_PMID(BTRFS_DISCARD_CLUST, BTRFS_DISCARD_BYTES_SAVED), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* iops_limit */
    { NULL,
      { PMDA_PMID(BTRFS_DISCARD_CLUST, BTRFS_DISCARD_IOPS_LIMIT), PM_TYPE_U32, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* kbps_limit */
    { NULL,
      { PMDA_PMID(BTRFS_DISCARD_CLUST, BTRFS_DISCARD_KBPS_LIMIT), PM_TYPE_U32, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, -1, 0, PM_SPACE_KBYTE, PM_TIME_SEC, 0) } },
/* max_discard_size */
    { NULL,
      { PMDA_PMID(BTRFS_DISCARD_CLUST, BTRFS_DISCARD_MAX_SIZE), PM_TYPE_U64, BTRFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
};

static int
btrfs_fetch(int numpmid, pmID *pmidlist, pmdaResult **resp, pmdaExt *pmda)
{
    int i;
    __pmID_int *idp;
    int need_fs = 0, need_dev = 0;

    for (i = 0; i < numpmid; i++) {
	idp = (__pmID_int *)&(pmidlist[i]);
	if (idp->cluster == BTRFS_DEV_CLUST)
	    need_dev = 1;
	else
	    need_fs = 1;
    }

    if (need_fs)
	btrfs_fs_refresh(INDOM(BTRFS_FS_INDOM));
    if (need_dev)
	btrfs_dev_refresh(INDOM(BTRFS_DEV_INDOM));

    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
btrfs_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int *idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    btrfs_alloc_t *alloc;
    btrfs_fs_t *fs;
    btrfs_dev_t *dev;
    int sts;

    if (idp->cluster == BTRFS_DEV_CLUST) {
	sts = pmdaCacheLookup(INDOM(BTRFS_DEV_INDOM), inst, NULL, (void **)&dev);
	if (sts < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	    return PM_ERR_INST;

	switch (idp->item) {
	case BTRFS_DEV_WRITE_ERRS:
	    atom->ull = dev->write_errs; break;
	case BTRFS_DEV_READ_ERRS:
	    atom->ull = dev->read_errs; break;
	case BTRFS_DEV_FLUSH_ERRS:
	    atom->ull = dev->flush_errs; break;
	case BTRFS_DEV_CORRUPTION_ERRS:
	    atom->ull = dev->corruption_errs; break;
	case BTRFS_DEV_GENERATION_ERRS:
	    atom->ull = dev->generation_errs; break;
	default:
	    return PM_ERR_PMID;
	}
	return PMDA_FETCH_STATIC;
    }

    sts = pmdaCacheLookup(INDOM(BTRFS_FS_INDOM), inst, NULL, (void **)&fs);
    if (sts < 0)
	return sts;
    if (sts != PMDA_CACHE_ACTIVE)
	return PM_ERR_INST;

    switch (idp->cluster) {
    case BTRFS_INFO_CLUST:
	switch (idp->item) {
	case BTRFS_INFO_LABEL:
	    atom->cp = fs->info.label; break;
	case BTRFS_INFO_NODESIZE:
	    atom->ull = fs->info.nodesize; break;
	case BTRFS_INFO_SECTORSIZE:
	    atom->ull = fs->info.sectorsize; break;
	case BTRFS_INFO_GENERATION:
	    atom->ull = fs->info.generation; break;
	case BTRFS_INFO_CHECKSUM:
	    atom->cp = fs->info.checksum; break;
	case BTRFS_INFO_METADATA_UUID:
	    atom->cp = fs->info.metadata_uuid; break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case BTRFS_COMMIT_CLUST:
	switch (idp->item) {
	case BTRFS_COMMIT_COUNT:
	    atom->ull = fs->commit.commits; break;
	case BTRFS_COMMIT_LAST_TIME:
	    atom->ull = fs->commit.last_commit_ms; break;
	case BTRFS_COMMIT_MAX_TIME:
	    atom->ull = fs->commit.max_commit_ms; break;
	case BTRFS_COMMIT_TOTAL_TIME:
	    atom->ull = fs->commit.total_commit_ms; break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case BTRFS_ALLOC_DATA_CLUST:
    case BTRFS_ALLOC_META_CLUST:
    case BTRFS_ALLOC_SYS_CLUST:
	if (idp->cluster == BTRFS_ALLOC_DATA_CLUST)
	    alloc = &fs->alloc_data;
	else if (idp->cluster == BTRFS_ALLOC_META_CLUST)
	    alloc = &fs->alloc_metadata;
	else
	    alloc = &fs->alloc_system;

	switch (idp->item) {
	case BTRFS_ALLOC_TOTAL_BYTES:
	    atom->ull = alloc->total_bytes; break;
	case BTRFS_ALLOC_BYTES_USED:
	    atom->ull = alloc->bytes_used; break;
	case BTRFS_ALLOC_BYTES_PINNED:
	    atom->ull = alloc->bytes_pinned; break;
	case BTRFS_ALLOC_BYTES_RESERVED:
	    atom->ull = alloc->bytes_reserved; break;
	case BTRFS_ALLOC_BYTES_READONLY:
	    atom->ull = alloc->bytes_readonly; break;
	case BTRFS_ALLOC_DISK_USED:
	    atom->ull = alloc->disk_used; break;
	case BTRFS_ALLOC_DISK_TOTAL:
	    atom->ull = alloc->disk_total; break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case BTRFS_ALLOC_GLOBAL_CLUST:
	switch (idp->item) {
	case BTRFS_GLOBAL_RSV_SIZE:
	    atom->ull = fs->global_rsv.size; break;
	case BTRFS_GLOBAL_RSV_RESERVED:
	    atom->ull = fs->global_rsv.reserved; break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case BTRFS_DISCARD_CLUST:
	switch (idp->item) {
	case BTRFS_DISCARD_DISCARDABLE_BYTES:
	    atom->ull = fs->discard.discardable_bytes; break;
	case BTRFS_DISCARD_DISCARDABLE_EXTENTS:
	    atom->ull = fs->discard.discardable_extents; break;
	case BTRFS_DISCARD_BITMAP_BYTES:
	    atom->ull = fs->discard.discard_bitmap_bytes; break;
	case BTRFS_DISCARD_EXTENT_BYTES:
	    atom->ull = fs->discard.discard_extent_bytes; break;
	case BTRFS_DISCARD_BYTES_SAVED:
	    atom->ull = fs->discard.discard_bytes_saved; break;
	case BTRFS_DISCARD_IOPS_LIMIT:
	    atom->ul = fs->discard.iops_limit; break;
	case BTRFS_DISCARD_KBPS_LIMIT:
	    atom->ul = fs->discard.kbps_limit; break;
	case BTRFS_DISCARD_MAX_SIZE:
	    atom->ull = fs->discard.max_discard_size; break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    default:
	return PM_ERR_PMID;
    }
    return PMDA_FETCH_STATIC;
}

static int
btrfs_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
btrfs_labelCallBack(pmInDom indom, unsigned int inst, pmLabelSet **lp)
{
    btrfs_fs_t *fs;
    btrfs_dev_t *dev;
    int sts;

    if (indom == INDOM(BTRFS_FS_INDOM)) {
	sts = pmdaCacheLookup(indom, inst, NULL, (void **)&fs);
	if (sts == PMDA_CACHE_ACTIVE && fs != NULL) {
	    pmdaAddLabels(lp, "{\"uuid\":\"%s\"}", fs->uuid);
	    if (fs->info.label[0] != '\0')
		pmdaAddLabels(lp, "{\"label\":\"%s\"}", fs->info.label);
	}
	return 0;
    }
    if (indom == INDOM(BTRFS_DEV_INDOM)) {
	sts = pmdaCacheLookup(indom, inst, NULL, (void **)&dev);
	if (sts == PMDA_CACHE_ACTIVE && dev != NULL) {
	    pmdaAddLabels(lp, "{\"uuid\":\"%s\"}", dev->uuid);
	    pmdaAddLabels(lp, "{\"devid\":\"%s\"}", dev->devid);
	}
	return 0;
    }
    return 0;
}

static int
btrfs_label(int ident, int type, pmLabelSet **lpp, pmdaExt *pmda)
{
    if (type == PM_LABEL_DOMAIN)
	pmdaAddLabels(lpp, "{\"filesystem\":\"btrfs\"}");
    return pmdaLabel(ident, type, lpp, pmda);
}

void
__PMDA_INIT_CALL
btrfs_init(pmdaInterface *dp)
{
    char helppath[MAXPATHLEN];
    int sep = pmPathSeparator();
    char *envpath;

    envpath = getenv("BTRFS_PATH");
    if (envpath == NULL || *envpath == '\0')
	envpath = BTRFS_DEFAULT_PATH;
    pmstrncpy(btrfs_path, MAXPATHLEN, envpath);

    if (_isDSO) {
	pmsprintf(helppath, sizeof(helppath), "%s%c" "btrfs" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_7, "Btrfs DSO", helppath);
    }

    if (dp->status != 0)
	return;

    dp->version.seven.fetch = btrfs_fetch;
    dp->version.seven.instance = btrfs_instance;
    dp->version.seven.label = btrfs_label;
    pmdaSetFetchCallBack(dp, btrfs_fetchCallBack);
    pmdaSetLabelCallBack(dp, btrfs_labelCallBack);
    pmdaInit(dp,
	    indomtab, sizeof(indomtab)/sizeof(indomtab[0]),
	    metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
}

pmLongOptions	longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions	opts = {
    .short_options = "D:d:l:?",
    .long_options = longopts,
};

int
main(int argc, char **argv)
{
    int		sep = pmPathSeparator();
    pmdaInterface dispatch;
    char	helppath[MAXPATHLEN];

    _isDSO = 0;
    pmSetProgname(argv[0]);
    pmsprintf(helppath, sizeof(helppath), "%s%c" "btrfs" "%c" "help",
	    pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), BTRFS, "btrfs.log", helppath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    pmdaOpenLog(&dispatch);
    btrfs_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
