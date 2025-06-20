/*
 * ZFS PMDA for Linux
 *
 * Copyright (c) 2021 Red Hat.
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
#include "zfs_utils.h"
#include "zfs_arcstats.h"
#include "zfs_abdstats.h"
#include "zfs_dbufstats.h"
#include "zfs_dmu_tx.h"
#include "zfs_dnodestats.h"
#include "zfs_fmstats.h"
#include "zfs_vdev_cachestats.h"
#include "zfs_vdev_mirrorstats.h"
#include "zfs_xuiostats.h"
#include "zfs_zfetchstats.h"
#include "zfs_zilstats.h"
#include "zfs_pools.h"

static char ZFS_DEFAULT_PATH[] = "/proc/spl/kstat/zfs";

static int _isDSO = 1; /* PMDA launched mode 1/0 for DSO/daemon */
static zfs_arcstats_t arcstats;
static zfs_abdstats_t abdstats;
static zfs_dbufstats_t dbufstats;
static zfs_dmu_tx_t dmu_tx;
static zfs_dnodestats_t dnodestats;
static zfs_fmstats_t fmstats;
static zfs_xuiostats_t xuiostats;
static zfs_vdev_cachestats_t vdev_cachestats;
static zfs_vdev_mirrorstats_t vdev_mirrorstats;
static zfs_zfetchstats_t zfetchstats;
static zfs_zilstats_t zilstats;
static zfs_poolstats_t *poolstats;
static pmdaInstid *pools;
static pmdaIndom indomtab[] = {
    { ZFS_POOL_INDOM, 0, NULL }
};

static pmdaMetric metrictab[] = {
/*---------------------------------------------------------------------------*/
/*  ARCSTATS  */
/*---------------------------------------------------------------------------*/
/* hits */
    { &arcstats.hits,
      { PMDA_PMID(ZFS_ARC_CLUST, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* misses */
    { &arcstats.misses,
      { PMDA_PMID(ZFS_ARC_CLUST, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* demand_data_hits */
    { &arcstats.demand_data_hits,
      { PMDA_PMID(ZFS_ARC_CLUST, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* demand_data_misses */
    { &arcstats.demand_data_misses,
      { PMDA_PMID(ZFS_ARC_CLUST, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* demand_metadata_hits */
    { &arcstats.demand_metadata_hits,
      { PMDA_PMID(ZFS_ARC_CLUST, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* demand_metadata_misses */
    { &arcstats.demand_metadata_misses,
      { PMDA_PMID(ZFS_ARC_CLUST, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* prefetch_data_hits */
    { &arcstats.prefetch_data_hits,
      { PMDA_PMID(ZFS_ARC_CLUST, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* prefetch_data_misses */
    { &arcstats.prefetch_data_misses,
      { PMDA_PMID(ZFS_ARC_CLUST, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* prefetch_metadata_hits */
    { &arcstats.prefetch_metadata_hits,
      { PMDA_PMID(ZFS_ARC_CLUST, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* prefetch_metadata_misses */
    { &arcstats.prefetch_metadata_misses,
      { PMDA_PMID(ZFS_ARC_CLUST, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mru_hits */
    { &arcstats.mru_hits,
      { PMDA_PMID(ZFS_ARC_CLUST, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mru_ghost_hits */
    { &arcstats.mru_ghost_hits,
      { PMDA_PMID(ZFS_ARC_CLUST, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mfu_hits */
    { &arcstats.mfu_hits,
      { PMDA_PMID(ZFS_ARC_CLUST, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mfu_ghost_hits */
    { &arcstats.mfu_ghost_hits,
      { PMDA_PMID(ZFS_ARC_CLUST, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* deleted */
    { &arcstats.deleted,
      { PMDA_PMID(ZFS_ARC_CLUST, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mutex_miss */
    { &arcstats.mutex_miss,
      { PMDA_PMID(ZFS_ARC_CLUST, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* access_skip */
    { &arcstats.access_skip,
      { PMDA_PMID(ZFS_ARC_CLUST, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* evict_skip */
    { &arcstats.evict_skip,
      { PMDA_PMID(ZFS_ARC_CLUST, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* evict_not_enough */
    { &arcstats.evict_not_enough,
      { PMDA_PMID(ZFS_ARC_CLUST, 18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* evict_l2_cached */
    { &arcstats.evict_l2_cached,
      { PMDA_PMID(ZFS_ARC_CLUST, 19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* evict_l2_eligible */
    { &arcstats.evict_l2_eligible,
      { PMDA_PMID(ZFS_ARC_CLUST, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* evict_l2_ineligible */
    { &arcstats.evict_l2_ineligible,
      { PMDA_PMID(ZFS_ARC_CLUST, 21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* evict_l2_skip */
    { &arcstats.evict_l2_skip,
      { PMDA_PMID(ZFS_ARC_CLUST, 22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_elements */
    { &arcstats.hash_elements,
      { PMDA_PMID(ZFS_ARC_CLUST, 23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_elements_max */
    { &arcstats.hash_elements_max,
      { PMDA_PMID(ZFS_ARC_CLUST, 24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_collisions */
    { &arcstats.hash_collisions,
      { PMDA_PMID(ZFS_ARC_CLUST, 25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_chains */
    { &arcstats.hash_chains,
      { PMDA_PMID(ZFS_ARC_CLUST, 26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_chain_max */
    { &arcstats.hash_chain_max,
      { PMDA_PMID(ZFS_ARC_CLUST, 27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* p */
    { &arcstats.p,
      { PMDA_PMID(ZFS_ARC_CLUST, 28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* c */
    { &arcstats.c,
      { PMDA_PMID(ZFS_ARC_CLUST, 29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* c_min */
    { &arcstats.c_min,
      { PMDA_PMID(ZFS_ARC_CLUST, 30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* c_max */
    { &arcstats.c_max,
      { PMDA_PMID(ZFS_ARC_CLUST, 31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* size */
    { &arcstats.size,
      { PMDA_PMID(ZFS_ARC_CLUST, 32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* compressed_size */
    { &arcstats.compressed_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* uncompressed_size */
    { &arcstats.uncompressed_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* overhead_size */
    { &arcstats.overhead_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* hdr_size */
    { &arcstats.hdr_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* data_size */
    { &arcstats.data_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* metadata_size */
    { &arcstats.metadata_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* dbuf_size */
    { &arcstats.dbuf_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* dnode_size */
    { &arcstats.dnode_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* bonus_size */
    { &arcstats.bonus_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* anon_size */
    { &arcstats.anon_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* anon_evictable_data */
    { &arcstats.anon_evictable_data,
      { PMDA_PMID(ZFS_ARC_CLUST, 43), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* anon_evictable_metadata */
    { &arcstats.anon_evictable_metadata,
      { PMDA_PMID(ZFS_ARC_CLUST, 44), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mru_size */
    { &arcstats.mru_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mru_evictable_data */
    { &arcstats.mru_evictable_data,
      { PMDA_PMID(ZFS_ARC_CLUST, 46), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mru_evictable_metadata */
    { &arcstats.mru_evictable_metadata,
      { PMDA_PMID(ZFS_ARC_CLUST, 47), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mru_ghost_size */
    { &arcstats.mru_ghost_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mru_ghost_evictable_data */
    { &arcstats.mru_ghost_evictable_data,
      { PMDA_PMID(ZFS_ARC_CLUST, 49), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mru_ghost_evictable_metadata */
    { &arcstats.mru_ghost_evictable_metadata,
      { PMDA_PMID(ZFS_ARC_CLUST, 50), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mfu_size */
    { &arcstats.mfu_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mfu_evictable_data */
    { &arcstats.mfu_evictable_data,
      { PMDA_PMID(ZFS_ARC_CLUST, 52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mfu_evictable_metadata */
    { &arcstats.mfu_evictable_metadata,
      { PMDA_PMID(ZFS_ARC_CLUST, 53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mfu_ghost_size */
    { &arcstats.mfu_ghost_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mfu_ghost_evictable_data */
    { &arcstats.mfu_ghost_evictable_data,
      { PMDA_PMID(ZFS_ARC_CLUST, 55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mfu_ghost_evictable_metadata */
    { &arcstats.mfu_ghost_evictable_metadata,
      { PMDA_PMID(ZFS_ARC_CLUST, 56), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* l2_hits */
    { &arcstats.l2_hits,
      { PMDA_PMID(ZFS_ARC_CLUST, 57), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_misses */
    { &arcstats.l2_misses,
      { PMDA_PMID(ZFS_ARC_CLUST, 58), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_feeds */
    { &arcstats.l2_feeds,
      { PMDA_PMID(ZFS_ARC_CLUST, 59), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_rw_clash */
    { &arcstats.l2_rw_clash,
      { PMDA_PMID(ZFS_ARC_CLUST, 60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_read_bytes */
    { &arcstats.l2_read_bytes,
      { PMDA_PMID(ZFS_ARC_CLUST, 61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* l2_write_bytes */
    { &arcstats.l2_write_bytes,
      { PMDA_PMID(ZFS_ARC_CLUST, 62), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* l2_writes_sent */
    { &arcstats.l2_writes_sent,
      { PMDA_PMID(ZFS_ARC_CLUST, 63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_writes_done */
    { &arcstats.l2_writes_done,
      { PMDA_PMID(ZFS_ARC_CLUST, 64), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_writes_error */
    { &arcstats.l2_writes_error,
      { PMDA_PMID(ZFS_ARC_CLUST, 65), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_writes_lock_retry */
    { &arcstats.l2_writes_lock_retry,
      { PMDA_PMID(ZFS_ARC_CLUST, 66), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_evict_lock_retry */
    { &arcstats.l2_evict_lock_retry,
      { PMDA_PMID(ZFS_ARC_CLUST, 67), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_evict_reading */
    { &arcstats.l2_evict_reading,
      { PMDA_PMID(ZFS_ARC_CLUST, 68), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_evict_l1cached */
    { &arcstats.l2_evict_l1cached,
      { PMDA_PMID(ZFS_ARC_CLUST, 69), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_free_on_write */
    { &arcstats.l2_free_on_write,
      { PMDA_PMID(ZFS_ARC_CLUST, 70), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_abort_lowmem */
    { &arcstats.l2_abort_lowmem,
      { PMDA_PMID(ZFS_ARC_CLUST, 71), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_cksum_bad */
    { &arcstats.l2_cksum_bad,
      { PMDA_PMID(ZFS_ARC_CLUST, 72), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_io_error */
    { &arcstats.l2_io_error,
      { PMDA_PMID(ZFS_ARC_CLUST, 73), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_size */
    { &arcstats.l2_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 74), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* l2_asize */
    { &arcstats.l2_asize,
      { PMDA_PMID(ZFS_ARC_CLUST, 75), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* l2_hdr_size */
    { &arcstats.l2_hdr_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 76), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* memory_throttle_count */
    { &arcstats.memory_throttle_count,
      { PMDA_PMID(ZFS_ARC_CLUST, 77), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* memory_direct_count */
    { &arcstats.memory_direct_count,
      { PMDA_PMID(ZFS_ARC_CLUST, 78), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* memory_indirect_count */
    { &arcstats.memory_indirect_count,
      { PMDA_PMID(ZFS_ARC_CLUST, 79), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* memory_all_bytes */
    { &arcstats.memory_all_bytes,
      { PMDA_PMID(ZFS_ARC_CLUST, 80), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* memory_free_bytes */
    { &arcstats.memory_free_bytes,
      { PMDA_PMID(ZFS_ARC_CLUST, 81), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* memory_available_bytes */
    { &arcstats.memory_available_bytes,
      { PMDA_PMID(ZFS_ARC_CLUST, 82), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* arc_no_grow */
    { &arcstats.arc_no_grow,
      { PMDA_PMID(ZFS_ARC_CLUST, 83), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* arc_tempreserve */
    { &arcstats.arc_tempreserve,
      { PMDA_PMID(ZFS_ARC_CLUST, 84), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* arc_loaned_bytes */
    { &arcstats.arc_loaned_bytes,
      { PMDA_PMID(ZFS_ARC_CLUST, 85), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* arc_prune */
    { &arcstats.arc_prune,
      { PMDA_PMID(ZFS_ARC_CLUST, 86), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* arc_meta_used */
    { &arcstats.arc_meta_used,
      { PMDA_PMID(ZFS_ARC_CLUST, 87), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* arc_meta_limit */
    { &arcstats.arc_meta_limit,
      { PMDA_PMID(ZFS_ARC_CLUST, 88), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* arc_dnode_limit */
    { &arcstats.arc_dnode_limit,
      { PMDA_PMID(ZFS_ARC_CLUST, 89), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* arc_meta_max */
    { &arcstats.arc_meta_max,
      { PMDA_PMID(ZFS_ARC_CLUST, 90), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* arc_meta_min */
    { &arcstats.arc_meta_min,
      { PMDA_PMID(ZFS_ARC_CLUST, 91), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* async_upgrade_sync */
    { &arcstats.async_upgrade_sync,
      { PMDA_PMID(ZFS_ARC_CLUST, 92), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* demand_hit_predictive_prefetch */
    { &arcstats.demand_hit_predictive_prefetch,
      { PMDA_PMID(ZFS_ARC_CLUST, 93), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* demand_hit_prescient_prefetch */
    { &arcstats.demand_hit_prescient_prefetch,
      { PMDA_PMID(ZFS_ARC_CLUST, 94), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* arc_need_free */
    { &arcstats.arc_need_free,
      { PMDA_PMID(ZFS_ARC_CLUST, 95), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* arc_sys_free */
    { &arcstats.arc_sys_free,
      { PMDA_PMID(ZFS_ARC_CLUST, 96), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* arc_raw_size */
    { &arcstats.arc_raw_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 97), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/*---------------------------------------------------------------------------*/
/*  ABDSTATS  */
/*---------------------------------------------------------------------------*/
/* struct_size */
    { &abdstats.struct_size,
      { PMDA_PMID(ZFS_ABD_CLUST, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* linear_cnt */
    { &abdstats.linear_cnt,
      { PMDA_PMID(ZFS_ABD_CLUST, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* linear_data_size */
    { &abdstats.linear_data_size,
      { PMDA_PMID(ZFS_ABD_CLUST, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* scatter_cnt */
    { &abdstats.scatter_cnt,
      { PMDA_PMID(ZFS_ABD_CLUST, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_data_size */
    { &abdstats.scatter_data_size,
      { PMDA_PMID(ZFS_ABD_CLUST, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* scatter_chunk_waste */
    { &abdstats.scatter_chunk_waste,
      { PMDA_PMID(ZFS_ABD_CLUST, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* scatter_order_0 */
    { &abdstats.scatter_order_0,
      { PMDA_PMID(ZFS_ABD_CLUST, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_1 */
    { &abdstats.scatter_order_1,
      { PMDA_PMID(ZFS_ABD_CLUST, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_2 */
    { &abdstats.scatter_order_2,
      { PMDA_PMID(ZFS_ABD_CLUST, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_3 */
    { &abdstats.scatter_order_3,
      { PMDA_PMID(ZFS_ABD_CLUST, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_4 */
    { &abdstats.scatter_order_4,
      { PMDA_PMID(ZFS_ABD_CLUST, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_5 */
    { &abdstats.scatter_order_5,
      { PMDA_PMID(ZFS_ABD_CLUST, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_6 */
    { &abdstats.scatter_order_6,
      { PMDA_PMID(ZFS_ABD_CLUST, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_7 */
    { &abdstats.scatter_order_7,
      { PMDA_PMID(ZFS_ABD_CLUST, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_8 */
    { &abdstats.scatter_order_8,
      { PMDA_PMID(ZFS_ABD_CLUST, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_9 */
    { &abdstats.scatter_order_9,
      { PMDA_PMID(ZFS_ABD_CLUST, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_10 */
    { &abdstats.scatter_order_10,
      { PMDA_PMID(ZFS_ABD_CLUST, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_page_multi_chunk */
    { &abdstats.scatter_page_multi_chunk,
      { PMDA_PMID(ZFS_ABD_CLUST, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_page_multi_zone */
    { &abdstats.scatter_page_multi_zone,
      { PMDA_PMID(ZFS_ABD_CLUST, 18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_page_alloc_retry */
    { &abdstats.scatter_page_alloc_retry,
      { PMDA_PMID(ZFS_ABD_CLUST, 19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_sg_table_retry */
    { &abdstats.scatter_sg_table_retry,
      { PMDA_PMID(ZFS_ABD_CLUST, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  DBUFSTATS  */
/*---------------------------------------------------------------------------*/
/* cache_count */
    { &dbufstats.cache_count,
      { PMDA_PMID(ZFS_DBUF_CLUST, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_size_bytes */
    { &dbufstats.cache_size_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_size_bytes_max */
    { &dbufstats.cache_size_bytes_max,
      { PMDA_PMID(ZFS_DBUF_CLUST, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_target_bytes */
    { &dbufstats.cache_target_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_lowater_bytes */
    { &dbufstats.cache_lowater_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_hiwater_bytes */
    { &dbufstats.cache_hiwater_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_total_evicts */
    { &dbufstats.cache_total_evicts,
      { PMDA_PMID(ZFS_DBUF_CLUST, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_0 */
    { &dbufstats.cache_level_0,
      { PMDA_PMID(ZFS_DBUF_CLUST, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_1 */
    { &dbufstats.cache_level_1,
      { PMDA_PMID(ZFS_DBUF_CLUST, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_2 */
    { &dbufstats.cache_level_2,
      { PMDA_PMID(ZFS_DBUF_CLUST, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_3 */
    { &dbufstats.cache_level_3,
      { PMDA_PMID(ZFS_DBUF_CLUST, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_4 */
    { &dbufstats.cache_level_4,
      { PMDA_PMID(ZFS_DBUF_CLUST, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_5 */
    { &dbufstats.cache_level_5,
      { PMDA_PMID(ZFS_DBUF_CLUST, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_6 */
    { &dbufstats.cache_level_6,
      { PMDA_PMID(ZFS_DBUF_CLUST, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_7 */
    { &dbufstats.cache_level_7,
      { PMDA_PMID(ZFS_DBUF_CLUST, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_8 */
    { &dbufstats.cache_level_8,
      { PMDA_PMID(ZFS_DBUF_CLUST, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_9 */
    { &dbufstats.cache_level_9,
      { PMDA_PMID(ZFS_DBUF_CLUST, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_10 */
    { &dbufstats.cache_level_10,
      { PMDA_PMID(ZFS_DBUF_CLUST, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_11 */
    { &dbufstats.cache_level_11,
      { PMDA_PMID(ZFS_DBUF_CLUST, 18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_0_bytes */
    { &dbufstats.cache_level_0_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_level_1_bytes */
    { &dbufstats.cache_level_1_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_level_2_bytes */
    { &dbufstats.cache_level_2_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_level_3_bytes */
    { &dbufstats.cache_level_3_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_level_4_bytes */
    { &dbufstats.cache_level_4_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_level_5_bytes */
    { &dbufstats.cache_level_5_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_level_6_bytes */
    { &dbufstats.cache_level_6_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_level_7_bytes */
    { &dbufstats.cache_level_7_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_level_8_bytes */
    { &dbufstats.cache_level_8_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_level_9_bytes */
    { &dbufstats.cache_level_9_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_level_10_bytes */
    { &dbufstats.cache_level_10_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* cache_level_11_bytes */
    { &dbufstats.cache_level_11_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* hash_hits */
    { &dbufstats.hash_hits,
      { PMDA_PMID(ZFS_DBUF_CLUST, 31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_misses */
    { &dbufstats.hash_misses,
      { PMDA_PMID(ZFS_DBUF_CLUST, 32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_collisions */
    { &dbufstats.hash_collisions,
      { PMDA_PMID(ZFS_DBUF_CLUST, 33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_elements */
    { &dbufstats.hash_elements,
      { PMDA_PMID(ZFS_DBUF_CLUST, 34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_elements_max */
    { &dbufstats.hash_elements_max,
      { PMDA_PMID(ZFS_DBUF_CLUST, 35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_chains */
    { &dbufstats.hash_chains,
      { PMDA_PMID(ZFS_DBUF_CLUST, 36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_chain_max */
    { &dbufstats.hash_chain_max,
      { PMDA_PMID(ZFS_DBUF_CLUST, 37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_insert_race */
    { &dbufstats.hash_insert_race,
      { PMDA_PMID(ZFS_DBUF_CLUST, 38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* metadata_cache_count */
    { &dbufstats.metadata_cache_count,
      { PMDA_PMID(ZFS_DBUF_CLUST, 39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* metadata_cache_size_bytes */
    { &dbufstats.metadata_cache_size_bytes,
      { PMDA_PMID(ZFS_DBUF_CLUST, 40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* metadata_cache_size_bytes_max */
    { &dbufstats.metadata_cache_size_bytes_max,
      { PMDA_PMID(ZFS_DBUF_CLUST, 41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* metadata_cache_overflow */
    { &dbufstats.metadata_cache_overflow,
      { PMDA_PMID(ZFS_DBUF_CLUST, 42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/*---------------------------------------------------------------------------*/
/*  DMU_TX  */
/*---------------------------------------------------------------------------*/
/* dmu_tx_assigned */
    { &dmu_tx.assigned,
      { PMDA_PMID(ZFS_DMUTX_CLUST, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_delay */
    { &dmu_tx.delay,
      { PMDA_PMID(ZFS_DMUTX_CLUST, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0) } },
/* dmu_tx_error */
    { &dmu_tx.error,
      { PMDA_PMID(ZFS_DMUTX_CLUST, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_suspended */
    { &dmu_tx.suspended,
      { PMDA_PMID(ZFS_DMUTX_CLUST, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_group */
    { &dmu_tx.group,
      { PMDA_PMID(ZFS_DMUTX_CLUST, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_memory_reserve */
    { &dmu_tx.memory_reserve,
      { PMDA_PMID(ZFS_DMUTX_CLUST, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* dmu_tx_memory_reclaim */
    { &dmu_tx.memory_reclaim,
      { PMDA_PMID(ZFS_DMUTX_CLUST, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* dmu_tx_dirty_throttle */
    { &dmu_tx.dirty_throttle,
      { PMDA_PMID(ZFS_DMUTX_CLUST, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0) } },
/* dmu_tx_dirty_delay */
    { &dmu_tx.dirty_delay,
      { PMDA_PMID(ZFS_DMUTX_CLUST, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0) } },
/* dmu_tx_dirty_over_max */
    { &dmu_tx.dirty_over_max,
      { PMDA_PMID(ZFS_DMUTX_CLUST, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* dmu_tx_dirty_frees_delay */
    { &dmu_tx.dirty_frees_delay,
      { PMDA_PMID(ZFS_DMUTX_CLUST, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0) } },
/* dmu_tx_quota */
    { &dmu_tx.quota,
      { PMDA_PMID(ZFS_DMUTX_CLUST, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/*---------------------------------------------------------------------------*/
/*  DNODESTATS */
/*---------------------------------------------------------------------------*/
/* dnode_hold_dbuf_hold */
    { &dnodestats.hold_dbuf_hold,
      { PMDA_PMID(ZFS_DNODE_CLUST, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_dbuf_read */
    { &dnodestats.hold_dbuf_read,
      { PMDA_PMID(ZFS_DNODE_CLUST, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_alloc_hits */
    { &dnodestats.hold_alloc_hits,
      { PMDA_PMID(ZFS_DNODE_CLUST, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_alloc_misses */
    { &dnodestats.hold_alloc_misses,
      { PMDA_PMID(ZFS_DNODE_CLUST, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_alloc_interior */
    { &dnodestats.hold_alloc_interior,
      { PMDA_PMID(ZFS_DNODE_CLUST, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_alloc_lock_retry */
    { &dnodestats.hold_alloc_lock_retry,
      { PMDA_PMID(ZFS_DNODE_CLUST, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_alloc_lock_misses */
    { &dnodestats.hold_alloc_lock_misses,
      { PMDA_PMID(ZFS_DNODE_CLUST, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_alloc_type_none */
    { &dnodestats.hold_alloc_type_none,
      { PMDA_PMID(ZFS_DNODE_CLUST, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_free_hits */
    { &dnodestats.hold_free_hits,
      { PMDA_PMID(ZFS_DNODE_CLUST, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_free_misses */
    { &dnodestats.hold_free_misses,
      { PMDA_PMID(ZFS_DNODE_CLUST, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_free_lock_misses */
    { &dnodestats.hold_free_lock_misses,
      { PMDA_PMID(ZFS_DNODE_CLUST, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_free_lock_retry */
    { &dnodestats.hold_free_lock_retry,
      { PMDA_PMID(ZFS_DNODE_CLUST, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_free_overflow */
    { &dnodestats.hold_free_overflow,
      { PMDA_PMID(ZFS_DNODE_CLUST, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_free_refcount */
    { &dnodestats.hold_free_refcount,
      { PMDA_PMID(ZFS_DNODE_CLUST, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_free_interior_lock_retry */
    { &dnodestats.free_interior_lock_retry,
      { PMDA_PMID(ZFS_DNODE_CLUST, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_allocate */
    { &dnodestats.allocate,
      { PMDA_PMID(ZFS_DNODE_CLUST, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_reallocate */
    { &dnodestats.reallocate,
      { PMDA_PMID(ZFS_DNODE_CLUST, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_buf_evict */
    { &dnodestats.buf_evict,
      { PMDA_PMID(ZFS_DNODE_CLUST, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_alloc_next_chunk */
    { &dnodestats.alloc_next_chunk,
      { PMDA_PMID(ZFS_DNODE_CLUST, 18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_alloc_race */
    { &dnodestats.alloc_race,
      { PMDA_PMID(ZFS_DNODE_CLUST, 19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_alloc_next_block */
    { &dnodestats.alloc_next_block,
      { PMDA_PMID(ZFS_DNODE_CLUST, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_move_invalid */
    { &dnodestats.move_invalid,
      { PMDA_PMID(ZFS_DNODE_CLUST, 21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_move_recheck1 */
    { &dnodestats.move_recheck1,
      { PMDA_PMID(ZFS_DNODE_CLUST, 22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_move_recheck2 */
    { &dnodestats.move_recheck2,
      { PMDA_PMID(ZFS_DNODE_CLUST, 23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_move_special */
    { &dnodestats.move_special,
      { PMDA_PMID(ZFS_DNODE_CLUST, 24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_move_handle */
    { &dnodestats.move_handle,
      { PMDA_PMID(ZFS_DNODE_CLUST, 25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_move_rwlock */
    { &dnodestats.move_rwlock,
      { PMDA_PMID(ZFS_DNODE_CLUST, 26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_move_active */
    { &dnodestats.move_active,
      { PMDA_PMID(ZFS_DNODE_CLUST, 27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  FMSTATS  */
/*---------------------------------------------------------------------------*/
/* erpt_dropped */
    { &fmstats.erpt_dropped,
      { PMDA_PMID(ZFS_FM_CLUST, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* erpt_set_failed */
    { &fmstats.erpt_set_failed,
      { PMDA_PMID(ZFS_FM_CLUST, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* fmri_set_failed */
    { &fmstats.fmri_set_failed,
      { PMDA_PMID(ZFS_FM_CLUST, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* payload_set_failed */
    { &fmstats.payload_set_failed,
      { PMDA_PMID(ZFS_FM_CLUST, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  VDEVSTATS  */
/*---------------------------------------------------------------------------*/
/* delegations */
    { &vdev_cachestats.delegations,
      { PMDA_PMID(ZFS_VDEV_CLUST, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hits */
    { &vdev_cachestats.hits,
      { PMDA_PMID(ZFS_VDEV_CLUST, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* misses */
    { &vdev_cachestats.misses,
      { PMDA_PMID(ZFS_VDEV_CLUST, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* rotating_linear */
    { &vdev_mirrorstats.rotating_linear,
      { PMDA_PMID(ZFS_VDEV_CLUST, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* rotating_offset */
    { &vdev_mirrorstats.rotating_offset,
      { PMDA_PMID(ZFS_VDEV_CLUST, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* rotating_seek */
    { &vdev_mirrorstats.rotating_seek,
      { PMDA_PMID(ZFS_VDEV_CLUST, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* non_rotating_linear */
    { &vdev_mirrorstats.non_rotating_linear,
      { PMDA_PMID(ZFS_VDEV_CLUST, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* non_rotating_seek */
    { &vdev_mirrorstats.non_rotating_seek,
      { PMDA_PMID(ZFS_VDEV_CLUST, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* preferred_found */
    { &vdev_mirrorstats.preferred_found,
      { PMDA_PMID(ZFS_VDEV_CLUST, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* preferred_not_found */
    { &vdev_mirrorstats.preferred_not_found,
      { PMDA_PMID(ZFS_VDEV_CLUST, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  XUIOSTATS  */
/*---------------------------------------------------------------------------*/
/* onloan_read_buf */
    { &xuiostats.onloan_read_buf,
      { PMDA_PMID(ZFS_XUIO_CLUST, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* onloan_write_buf */
    { &xuiostats.onloan_write_buf,
      { PMDA_PMID(ZFS_XUIO_CLUST, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* read_buf_copied */
    { &xuiostats.read_buf_copied,
      { PMDA_PMID(ZFS_XUIO_CLUST, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* read_buf_nocopy */
    { &xuiostats.read_buf_nocopy,
      { PMDA_PMID(ZFS_XUIO_CLUST, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* write_buf_copied */
    { &xuiostats.write_buf_copied,
      { PMDA_PMID(ZFS_XUIO_CLUST, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* write_buf_nocopy */
    { &xuiostats.write_buf_nocopy,
      { PMDA_PMID(ZFS_XUIO_CLUST, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  ZFETCHSTATS  */
/*---------------------------------------------------------------------------*/
/* hits */
    { &zfetchstats.hits,
      { PMDA_PMID(ZFS_ZFETCH_CLUST, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* misses */
    { &zfetchstats.misses,
      { PMDA_PMID(ZFS_ZFETCH_CLUST, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* max_streams */
    { &zfetchstats.max_streams,
      { PMDA_PMID(ZFS_ZFETCH_CLUST, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  ZILSTATS  */
/*---------------------------------------------------------------------------*/
/* commit_count */
    { &zilstats.commit_count,
      { PMDA_PMID(ZFS_ZIL_CLUST, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* commit_writer_count */
    { &zilstats.commit_writer_count,
      { PMDA_PMID(ZFS_ZIL_CLUST, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_count */
    { &zilstats.itx_count,
      { PMDA_PMID(ZFS_ZIL_CLUST, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_indirect_count */
    { &zilstats.itx_indirect_count,
      { PMDA_PMID(ZFS_ZIL_CLUST, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_indirect_bytes */
    { &zilstats.itx_indirect_bytes,
      { PMDA_PMID(ZFS_ZIL_CLUST, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* itx_copied_count */
    { &zilstats.itx_copied_count,
      { PMDA_PMID(ZFS_ZIL_CLUST, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_copied_bytes */
    { &zilstats.itx_copied_bytes,
      { PMDA_PMID(ZFS_ZIL_CLUST, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* itx_needcopy_count */
    { &zilstats.itx_needcopy_count,
      { PMDA_PMID(ZFS_ZIL_CLUST, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_needcopy_bytes */
    { &zilstats.itx_needcopy_bytes,
      { PMDA_PMID(ZFS_ZIL_CLUST, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* itx_metaslab_normal_count */
    { &zilstats.itx_metaslab_normal_count,
      { PMDA_PMID(ZFS_ZIL_CLUST, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_metaslab_normal_bytes */
    { &zilstats.itx_metaslab_normal_bytes,
      { PMDA_PMID(ZFS_ZIL_CLUST, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* itx_metaslab_slog_count */
    { &zilstats.itx_metaslab_slog_count,
      { PMDA_PMID(ZFS_ZIL_CLUST, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_metaslab_slog_bytes */
    { &zilstats.itx_metaslab_slog_bytes,
      { PMDA_PMID(ZFS_ZIL_CLUST, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/*---------------------------------------------------------------------------*/
/*  POOLSTATS  */
/*---------------------------------------------------------------------------*/
/* state */
    { NULL,
      { PMDA_PMID(ZFS_POOL_CLUST, ZFS_POOL_STATE), PM_TYPE_U32, ZFS_POOL_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* nread */
    { NULL,
      { PMDA_PMID(ZFS_POOL_CLUST, ZFS_POOL_NREAD), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* nwritten */
    { NULL,
      { PMDA_PMID(ZFS_POOL_CLUST, ZFS_POOL_NWRITTEN), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* reads */
    { NULL,
      { PMDA_PMID(ZFS_POOL_CLUST, ZFS_POOL_READS), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* writes */
    { NULL,
      { PMDA_PMID(ZFS_POOL_CLUST, ZFS_POOL_WRITES), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* wtime */
    { NULL,
      { PMDA_PMID(ZFS_POOL_CLUST, ZFS_POOL_WTIME), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0) } },
/* wlentime */
    { NULL,
      { PMDA_PMID(ZFS_POOL_CLUST, ZFS_POOL_WLENTIME), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0) } },
/* wupdate */
    { NULL,
      { PMDA_PMID(ZFS_POOL_CLUST, ZFS_POOL_WUPDATE), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0) } },
/* rtime */
    { NULL,
      { PMDA_PMID(ZFS_POOL_CLUST, ZFS_POOL_RTIME), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0) } },
/* rlentime */
    { NULL,
      { PMDA_PMID(ZFS_POOL_CLUST, ZFS_POOL_RLENTIME), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0) } },
/* rupdate */
    { NULL,
      { PMDA_PMID(ZFS_POOL_CLUST, ZFS_POOL_RUPDATE), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0) } },
/* wcnt */
    { NULL,
      { PMDA_PMID(ZFS_POOL_CLUST, ZFS_POOL_WCNT), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* rcnt */
    { NULL,
      { PMDA_PMID(ZFS_POOL_CLUST, ZFS_POOL_RCNT), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  ARCSTATS introduced in OpenZFS v. 2  */
/*---------------------------------------------------------------------------*/
/* l2_log_blk_writes */
    { &arcstats.l2_log_blk_writes,
      { PMDA_PMID(ZFS_ARC_CLUST, 98), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_log_blk_avg_asize */
    { &arcstats.l2_log_blk_avg_asize,
      { PMDA_PMID(ZFS_ARC_CLUST, 99), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* l2_log_blk_asize */
    { &arcstats.l2_log_blk_asize,
      { PMDA_PMID(ZFS_ARC_CLUST, 100), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* l2_log_blk_count */
    { &arcstats.l2_log_blk_count,
      { PMDA_PMID(ZFS_ARC_CLUST, 101), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_data_to_meta_ratio */
    { &arcstats.l2_data_to_meta_ratio,
      { PMDA_PMID(ZFS_ARC_CLUST, 102), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_rebuild_success */
    { &arcstats.l2_rebuild_success,
      { PMDA_PMID(ZFS_ARC_CLUST, 103), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_rebuild_unsupported */
    { &arcstats.l2_rebuild_unsupported,
      { PMDA_PMID(ZFS_ARC_CLUST, 104), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_rebuild_io_errors */
    { &arcstats.l2_rebuild_io_errors,
      { PMDA_PMID(ZFS_ARC_CLUST, 105), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_rebuild_dh_errors */
    { &arcstats.l2_rebuild_dh_errors,
      { PMDA_PMID(ZFS_ARC_CLUST, 106), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_rebuild_cksum_lb_errors */
    { &arcstats.l2_rebuild_cksum_lb_errors,
      { PMDA_PMID(ZFS_ARC_CLUST, 107), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_rebuild_lowmem */
    { &arcstats.l2_rebuild_lowmem,
      { PMDA_PMID(ZFS_ARC_CLUST, 108), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_rebuild_size */
    { &arcstats.l2_rebuild_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 109), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* l2_rebuild_asize */
    { &arcstats.l2_rebuild_asize,
      { PMDA_PMID(ZFS_ARC_CLUST, 110), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* l2_rebuild_bufs */
    { &arcstats.l2_rebuild_bufs,
      { PMDA_PMID(ZFS_ARC_CLUST, 111), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_rebuild_bufs_precached */
    { &arcstats.l2_rebuild_bufs_precached,
      { PMDA_PMID(ZFS_ARC_CLUST, 112), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_rebuild_log_blks */
    { &arcstats.l2_rebuild_log_blks,
      { PMDA_PMID(ZFS_ARC_CLUST, 113), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cached_only_in_progress */
    { &arcstats.cached_only_in_progress,
      { PMDA_PMID(ZFS_ARC_CLUST, 114), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* abd_chunk_waste_size */
    { &arcstats.abd_chunk_waste_size,
      { PMDA_PMID(ZFS_ARC_CLUST, 115), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
};

static int
zfs_fetch(int numpmid, pmID *pmidlist, pmdaResult **resp, pmdaExt *pmda)
{
    int i;
    __pmID_int *idp;

    for (i = 0; i < numpmid; i++) {
        idp = (__pmID_int *)&(pmidlist[i]);
        switch (idp->cluster) {
        case ZFS_ARC_CLUST:
            zfs_arcstats_refresh(&arcstats);
            break;
        case ZFS_ABD_CLUST:
            zfs_abdstats_refresh(&abdstats);
            break;
        case ZFS_DBUF_CLUST:
            zfs_dbufstats_refresh(&dbufstats);
            break;
        case ZFS_DMUTX_CLUST:
            zfs_dmu_tx_refresh(&dmu_tx);
            break;
        case ZFS_DNODE_CLUST:
            zfs_dnodestats_refresh(&dnodestats);
            break;
        case ZFS_FM_CLUST:
            zfs_fmstats_refresh(&fmstats);
            break;
        case ZFS_VDEV_CLUST:
            zfs_vdev_cachestats_refresh(&vdev_cachestats);
            zfs_vdev_mirrorstats_refresh(&vdev_mirrorstats);
            break;
        case ZFS_XUIO_CLUST:
            zfs_xuiostats_refresh(&xuiostats);
            break;
        case ZFS_ZFETCH_CLUST:
            zfs_zfetchstats_refresh(&zfetchstats);
            break;
        case ZFS_ZIL_CLUST:
            zfs_zilstats_refresh(&zilstats);
            break;
        case ZFS_POOL_CLUST:
            zfs_poolstats_refresh(&poolstats, &pools, &indomtab[ZFS_POOL_INDOM]);
            break;
        }
    }
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
zfs_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int *idp = (__pmID_int *)&(mdesc->m_desc.pmid);

    if (idp->cluster == ZFS_POOL_CLUST) { // && mdesc->m_desc.indom == ZFS_POOL_INDOM) {
        switch (idp->item) {
        case ZFS_POOL_STATE:
            atom->l = (__int32_t)poolstats[inst].state;
            break;
        case ZFS_POOL_NREAD:
            atom->ull = (__uint64_t)poolstats[inst].nread;
            break;
        case ZFS_POOL_NWRITTEN:
            atom->ull = (__uint64_t)poolstats[inst].nwritten;
            break;
        case ZFS_POOL_READS:
            atom->ull = (__uint64_t)poolstats[inst].reads;
            break;
        case ZFS_POOL_WRITES:
            atom->ull = (__uint64_t)poolstats[inst].writes;
            break;
        case ZFS_POOL_WTIME:
            atom->ull = (__uint64_t)poolstats[inst].wtime;
            break;
        case ZFS_POOL_WLENTIME:
            atom->ull = (__uint64_t)poolstats[inst].wlentime;
            break;
        case ZFS_POOL_WUPDATE:
            atom->ull = (__uint64_t)poolstats[inst].wupdate;
            break;
        case ZFS_POOL_RTIME:
            atom->ull = (__uint64_t)poolstats[inst].rtime;
            break;
        case ZFS_POOL_RLENTIME:
            atom->ull = (__uint64_t)poolstats[inst].rlentime;
            break;
        case ZFS_POOL_RUPDATE:
            atom->ull = (__uint64_t)poolstats[inst].rupdate;
            break;
        case ZFS_POOL_WCNT:
            atom->ull = (__uint64_t)poolstats[inst].wcnt;
            break;
        case ZFS_POOL_RCNT:
            atom->ull = (__uint64_t)poolstats[inst].rcnt;
            break;
        default:
            return PM_ERR_PMID;
        }
    }
    else {
        switch (mdesc->m_desc.type) {
        case PM_TYPE_U32:
            atom->ul = *(__uint32_t *)mdesc->m_user;
            break;
        case PM_TYPE_U64:
            atom->ull = *(__uint64_t *)mdesc->m_user;
            break;
        case PM_TYPE_STRING:
            atom->cp = (char *)mdesc->m_user;
            break;
        default:
            return PM_ERR_TYPE;
        }
    }
    return 1;
}

static int
zfs_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    //zfs_pools_init(&poolstats, &pools, &indomtab[ZFS_POOL_INDOM]);
    return pmdaInstance(indom, inst, name, result, pmda);
}

void
__PMDA_INIT_CALL
zfs_init(pmdaInterface *dp)
{
    char helppath[MAXPATHLEN];
    int sep = pmPathSeparator();
    char *envpath;

    /* optional overrides of globals for testing */
    envpath = getenv("ZFS_PATH");
    if (envpath == NULL || *envpath == '\0')
	envpath = ZFS_DEFAULT_PATH;
    pmstrncpy(zfs_path, MAXPATHLEN, envpath);

    if (_isDSO) {
        pmsprintf(helppath, sizeof(helppath), "%s%c" "zfs" "%c" "help",
                pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
        pmdaDSO(dp, PMDA_INTERFACE_7, "ZFS DSO", helppath);
    }

    if (dp->status != 0)
        return;

    dp->version.any.instance = zfs_instance;
    dp->version.any.fetch = zfs_fetch;
    pmdaSetFetchCallBack(dp, zfs_fetchCallBack);
    pmdaInit(dp,
            indomtab, sizeof(indomtab)/sizeof(indomtab[0]),
            metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
}

pmLongOptions   longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions     opts = {
    .short_options = "D:d:l:?",
    .long_options = longopts,
};

int
main(int argc, char **argv)
{
    int            sep = pmPathSeparator();
    pmdaInterface  dispatch;
    char           helppath[MAXPATHLEN];

    _isDSO = 0;
    pmSetProgname(argv[0]);
    pmsprintf(helppath, sizeof(helppath), "%s%c" "zfs" "%c" "help",
            pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), ZFS, "zfs.log", helppath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
        pmdaUsageMessage(&opts);
        exit(1);
    }

    pmdaOpenLog(&dispatch);
    zfs_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
