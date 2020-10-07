#include <stdint.h>
#include <regex.h>
#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "domain.h"
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

static int		_isDSO = 1; /* PMDA launched mode 1/0 for DSO/daemon */
regex_t rgx_row;
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
static zfs_poolstats_t *poolstats = NULL;
static pmdaInstid *pools = NULL;
static pmdaIndom indomtab[] = {
        { ZFS_POOL_INDOM, 0, NULL }
};

static pmdaMetric metrictab[] = {
/*---------------------------------------------------------------------------*/
/*  ARCSTATS  */
/*---------------------------------------------------------------------------*/
/* hits */
	{ &arcstats.hits,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* misses */
	{ &arcstats.misses,
	  { PMDA_PMID(0, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* demand_data_hits */
	{ &arcstats.demand_data_hits,
	  { PMDA_PMID(0, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* demand_data_misses */
	{ &arcstats.demand_data_misses,
	  { PMDA_PMID(0, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* demand_metadata_hits */
	{ &arcstats.demand_metadata_hits,
	  { PMDA_PMID(0, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* demand_metadata_misses */
	{ &arcstats.demand_metadata_misses,
	  { PMDA_PMID(0, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* prefetch_data_hits */
	{ &arcstats.prefetch_data_hits,
	  { PMDA_PMID(0, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* prefetch_data_misses */
	{ &arcstats.prefetch_data_misses,
	  { PMDA_PMID(0, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* prefetch_metadata_hits */
	{ &arcstats.prefetch_metadata_hits,
	  { PMDA_PMID(0, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* prefetch_metadata_misses */
	{ &arcstats.prefetch_metadata_misses,
	  { PMDA_PMID(0, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mru_hits */
	{ &arcstats.mru_hits,
	  { PMDA_PMID(0, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mru_ghost_hits */
	{ &arcstats.mru_ghost_hits,
	  { PMDA_PMID(0, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mfu_hits */
	{ &arcstats.mfu_hits,
	  { PMDA_PMID(0, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mfu_ghost_hits */
	{ &arcstats.mfu_ghost_hits,
	  { PMDA_PMID(0, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* deleted */
	{ &arcstats.deleted,
	  { PMDA_PMID(0, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mutex_miss */
	{ &arcstats.mutex_miss,
	  { PMDA_PMID(0, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* access_skip */
	{ &arcstats.access_skip,
	  { PMDA_PMID(0, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* evict_skip */
	{ &arcstats.evict_skip,
	  { PMDA_PMID(0, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* evict_not_enough */
	{ &arcstats.evict_not_enough,
	  { PMDA_PMID(0, 18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* evict_l2_cached */
	{ &arcstats.evict_l2_cached,
	  { PMDA_PMID(0, 19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* evict_l2_eligible */
	{ &arcstats.evict_l2_eligible,
	  { PMDA_PMID(0, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* evict_l2_ineligible */
	{ &arcstats.evict_l2_ineligible,
	  { PMDA_PMID(0, 21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* evict_l2_skip */
	{ &arcstats.evict_l2_skip,
	  { PMDA_PMID(0, 22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_elements */
	{ &arcstats.hash_elements,
	  { PMDA_PMID(0, 23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_elements_max */
	{ &arcstats.hash_elements_max,
	  { PMDA_PMID(0, 24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_collisions */
	{ &arcstats.hash_collisions,
	  { PMDA_PMID(0, 25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_chains */
	{ &arcstats.hash_chains,
	  { PMDA_PMID(0, 26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_chain_max */
	{ &arcstats.hash_chain_max,
	  { PMDA_PMID(0, 27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* p */
	{ &arcstats.p,
	  { PMDA_PMID(0, 28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* c */
	{ &arcstats.c,
	  { PMDA_PMID(0, 29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* c_min */
	{ &arcstats.c_min,
	  { PMDA_PMID(0, 30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* c_max */
	{ &arcstats.c_max,
	  { PMDA_PMID(0, 31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* size */
	{ &arcstats.size,
	  { PMDA_PMID(0, 32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* compressed_size */
	{ &arcstats.compressed_size,
	  { PMDA_PMID(0, 33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* uncompressed_size */
	{ &arcstats.uncompressed_size,
	  { PMDA_PMID(0, 34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* overhead_size */
	{ &arcstats.overhead_size,
	  { PMDA_PMID(0, 35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hdr_size */
	{ &arcstats.hdr_size,
	  { PMDA_PMID(0, 36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* data_size */
	{ &arcstats.data_size,
	  { PMDA_PMID(0, 37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* metadata_size */
	{ &arcstats.metadata_size,
	  { PMDA_PMID(0, 38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dbuf_size */
	{ &arcstats.dbuf_size,
	  { PMDA_PMID(0, 39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_size */
	{ &arcstats.dnode_size,
	  { PMDA_PMID(0, 40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* bonus_size */
	{ &arcstats.bonus_size,
	  { PMDA_PMID(0, 41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* anon_size */
	{ &arcstats.anon_size,
	  { PMDA_PMID(0, 42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* anon_evictable_data */
	{ &arcstats.anon_evictable_data,
	  { PMDA_PMID(0, 43), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* anon_evictable_metadata */
	{ &arcstats.anon_evictable_metadata,
	  { PMDA_PMID(0, 44), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mru_size */
	{ &arcstats.mru_size,
	  { PMDA_PMID(0, 45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mru_evictable_data */
	{ &arcstats.mru_evictable_data,
	  { PMDA_PMID(0, 46), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mru_evictable_metadata */
	{ &arcstats.mru_evictable_metadata,
	  { PMDA_PMID(0, 47), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mru_ghost_size */
	{ &arcstats.mru_ghost_size,
	  { PMDA_PMID(0, 48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mru_ghost_evictable_data */
	{ &arcstats.mru_ghost_evictable_data,
	  { PMDA_PMID(0, 49), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mru_ghost_evictable_metadata */
	{ &arcstats.mru_ghost_evictable_metadata,
	  { PMDA_PMID(0, 50), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mfu_size */
	{ &arcstats.mfu_size,
	  { PMDA_PMID(0, 51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mfu_evictable_data */
	{ &arcstats.mfu_evictable_data,
	  { PMDA_PMID(0, 52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mfu_evictable_metadata */
	{ &arcstats.mfu_evictable_metadata,
	  { PMDA_PMID(0, 53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mfu_ghost_size */
	{ &arcstats.mfu_ghost_size,
	  { PMDA_PMID(0, 54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mfu_ghost_evictable_data */
	{ &arcstats.mfu_ghost_evictable_data,
	  { PMDA_PMID(0, 55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* mfu_ghost_evictable_metadata */
	{ &arcstats.mfu_ghost_evictable_metadata,
	  { PMDA_PMID(0, 56), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_hits */
	{ &arcstats.l2_hits,
	  { PMDA_PMID(0, 57), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_misses */
	{ &arcstats.l2_misses,
	  { PMDA_PMID(0, 58), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_feeds */
	{ &arcstats.l2_feeds,
	  { PMDA_PMID(0, 59), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_rw_clash */
	{ &arcstats.l2_rw_clash,
	  { PMDA_PMID(0, 60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_read_bytes */
	{ &arcstats.l2_read_bytes,
	  { PMDA_PMID(0, 61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_write_bytes */
	{ &arcstats.l2_write_bytes,
	  { PMDA_PMID(0, 62), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_writes_sent */
	{ &arcstats.l2_writes_sent,
	  { PMDA_PMID(0, 63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_writes_done */
	{ &arcstats.l2_writes_done,
	  { PMDA_PMID(0, 64), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_writes_error */
	{ &arcstats.l2_writes_error,
	  { PMDA_PMID(0, 65), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_writes_lock_retry */
	{ &arcstats.l2_writes_lock_retry,
	  { PMDA_PMID(0, 66), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_evict_lock_retry */
	{ &arcstats.l2_evict_lock_retry,
	  { PMDA_PMID(0, 67), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_evict_reading */
	{ &arcstats.l2_evict_reading,
	  { PMDA_PMID(0, 68), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_evict_l1cached */
	{ &arcstats.l2_evict_l1cached,
	  { PMDA_PMID(0, 69), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_free_on_write */
	{ &arcstats.l2_free_on_write,
	  { PMDA_PMID(0, 70), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_abort_lowmem */
	{ &arcstats.l2_abort_lowmem,
	  { PMDA_PMID(0, 71), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_cksum_bad */
	{ &arcstats.l2_cksum_bad,
	  { PMDA_PMID(0, 72), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_io_error */
	{ &arcstats.l2_io_error,
	  { PMDA_PMID(0, 73), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_size */
	{ &arcstats.l2_size,
	  { PMDA_PMID(0, 74), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_asize */
	{ &arcstats.l2_asize,
	  { PMDA_PMID(0, 75), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* l2_hdr_size */
	{ &arcstats.l2_hdr_size,
	  { PMDA_PMID(0, 76), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* memory_throttle_count */
	{ &arcstats.memory_throttle_count,
	  { PMDA_PMID(0, 77), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* memory_direct_count */
	{ &arcstats.memory_direct_count,
	  { PMDA_PMID(0, 78), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* memory_indirect_count */
	{ &arcstats.memory_indirect_count,
	  { PMDA_PMID(0, 79), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* memory_all_bytes */
	{ &arcstats.memory_all_bytes,
	  { PMDA_PMID(0, 80), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* memory_free_bytes */
	{ &arcstats.memory_free_bytes,
	  { PMDA_PMID(0, 81), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* memory_available_bytes */
	{ &arcstats.memory_available_bytes,
	  { PMDA_PMID(0, 82), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* arc_no_grow */
	{ &arcstats.arc_no_grow,
	  { PMDA_PMID(0, 83), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* arc_tempreserve */
	{ &arcstats.arc_tempreserve,
	  { PMDA_PMID(0, 84), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* arc_loaned_bytes */
	{ &arcstats.arc_loaned_bytes,
	  { PMDA_PMID(0, 85), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* arc_prune */
	{ &arcstats.arc_prune,
	  { PMDA_PMID(0, 86), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* arc_meta_used */
	{ &arcstats.arc_meta_used,
	  { PMDA_PMID(0, 87), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* arc_meta_limit */
	{ &arcstats.arc_meta_limit,
	  { PMDA_PMID(0, 88), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* arc_dnode_limit */
	{ &arcstats.arc_dnode_limit,
	  { PMDA_PMID(0, 89), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* arc_meta_max */
	{ &arcstats.arc_meta_max,
	  { PMDA_PMID(0, 90), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* arc_meta_min */
	{ &arcstats.arc_meta_min,
	  { PMDA_PMID(0, 91), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* async_upgrade_sync */
	{ &arcstats.async_upgrade_sync,
	  { PMDA_PMID(0, 92), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* demand_hit_predictive_prefetch */
	{ &arcstats.demand_hit_predictive_prefetch,
	  { PMDA_PMID(0, 93), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* demand_hit_prescient_prefetch */
	{ &arcstats.demand_hit_prescient_prefetch,
	  { PMDA_PMID(0, 94), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* arc_need_free */
	{ &arcstats.arc_need_free,
	  { PMDA_PMID(0, 95), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* arc_sys_free */
	{ &arcstats.arc_sys_free,
	  { PMDA_PMID(0, 96), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* arc_raw_size */
	{ &arcstats.arc_raw_size,
	  { PMDA_PMID(0, 97), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  ABDSTATS  */
/*---------------------------------------------------------------------------*/
/* struct_size */
	{ &abdstats.struct_size,
	  { PMDA_PMID(1, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* linear_cnt */
	{ &abdstats.linear_cnt,
	  { PMDA_PMID(1, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* linear_data_size */
	{ &abdstats.linear_data_size,
	  { PMDA_PMID(1, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_cnt */
	{ &abdstats.scatter_cnt,
	  { PMDA_PMID(1, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_data_size */
	{ &abdstats.scatter_data_size,
	  { PMDA_PMID(1, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_chunk_waste */
	{ &abdstats.scatter_chunk_waste,
	  { PMDA_PMID(1, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_0 */
	{ &abdstats.scatter_order_0,
	  { PMDA_PMID(1, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_1 */
	{ &abdstats.scatter_order_1,
	  { PMDA_PMID(1, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_2 */
	{ &abdstats.scatter_order_2,
	  { PMDA_PMID(1, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_3 */
	{ &abdstats.scatter_order_3,
	  { PMDA_PMID(1, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_4 */
	{ &abdstats.scatter_order_4,
	  { PMDA_PMID(1, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_5 */
	{ &abdstats.scatter_order_5,
	  { PMDA_PMID(1, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_6 */
	{ &abdstats.scatter_order_6,
	  { PMDA_PMID(1, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_7 */
	{ &abdstats.scatter_order_7,
	  { PMDA_PMID(1, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_8 */
	{ &abdstats.scatter_order_8,
	  { PMDA_PMID(1, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_9 */
	{ &abdstats.scatter_order_9,
	  { PMDA_PMID(1, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_order_10 */
	{ &abdstats.scatter_order_10,
	  { PMDA_PMID(1, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_page_multi_chunk */
	{ &abdstats.scatter_page_multi_chunk,
	  { PMDA_PMID(1, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_page_multi_zone */
	{ &abdstats.scatter_page_multi_zone,
	  { PMDA_PMID(1, 18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_page_alloc_retry */
	{ &abdstats.scatter_page_alloc_retry,
	  { PMDA_PMID(1, 19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* scatter_sg_table_retry */
	{ &abdstats.scatter_sg_table_retry,
	  { PMDA_PMID(1, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  DBUFSTATS  */
/*---------------------------------------------------------------------------*/
/* cache_count */
	{ &dbufstats.cache_count,
	  { PMDA_PMID(2, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_size_bytes */
	{ &dbufstats.cache_size_bytes,
	  { PMDA_PMID(2, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_size_bytes_max */
	{ &dbufstats.cache_size_bytes_max,
	  { PMDA_PMID(2, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_target_bytes */
	{ &dbufstats.cache_target_bytes,
	  { PMDA_PMID(2, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_lowater_bytes */
	{ &dbufstats.cache_lowater_bytes,
	  { PMDA_PMID(2, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_hiwater_bytes */
	{ &dbufstats.cache_hiwater_bytes,
	  { PMDA_PMID(2, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_total_evicts */
	{ &dbufstats.cache_total_evicts,
	  { PMDA_PMID(2, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_0 */
	{ &dbufstats.cache_level_0,
	  { PMDA_PMID(2, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_1 */
	{ &dbufstats.cache_level_1,
	  { PMDA_PMID(2, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_2 */
	{ &dbufstats.cache_level_2,
	  { PMDA_PMID(2, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_3 */
	{ &dbufstats.cache_level_3,
	  { PMDA_PMID(2, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_4 */
	{ &dbufstats.cache_level_4,
	  { PMDA_PMID(2, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_5 */
	{ &dbufstats.cache_level_5,
	  { PMDA_PMID(2, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_6 */
	{ &dbufstats.cache_level_6,
	  { PMDA_PMID(2, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_7 */
	{ &dbufstats.cache_level_7,
	  { PMDA_PMID(2, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_8 */
	{ &dbufstats.cache_level_8,
	  { PMDA_PMID(2, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_9 */
	{ &dbufstats.cache_level_9,
	  { PMDA_PMID(2, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_10 */
	{ &dbufstats.cache_level_10,
	  { PMDA_PMID(2, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_11 */
	{ &dbufstats.cache_level_11,
	  { PMDA_PMID(2, 18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_0_bytes */
	{ &dbufstats.cache_level_0_bytes,
	  { PMDA_PMID(2, 19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_1_bytes */
	{ &dbufstats.cache_level_1_bytes,
	  { PMDA_PMID(2, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_2_bytes */
	{ &dbufstats.cache_level_2_bytes,
	  { PMDA_PMID(2, 21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_3_bytes */
	{ &dbufstats.cache_level_3_bytes,
	  { PMDA_PMID(2, 22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_4_bytes */
	{ &dbufstats.cache_level_4_bytes,
	  { PMDA_PMID(2, 23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_5_bytes */
	{ &dbufstats.cache_level_5_bytes,
	  { PMDA_PMID(2, 24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_6_bytes */
	{ &dbufstats.cache_level_6_bytes,
	  { PMDA_PMID(2, 25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_7_bytes */
	{ &dbufstats.cache_level_7_bytes,
	  { PMDA_PMID(2, 26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_8_bytes */
	{ &dbufstats.cache_level_8_bytes,
	  { PMDA_PMID(2, 27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_9_bytes */
	{ &dbufstats.cache_level_9_bytes,
	  { PMDA_PMID(2, 28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_10_bytes */
	{ &dbufstats.cache_level_10_bytes,
	  { PMDA_PMID(2, 29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* cache_level_11_bytes */
	{ &dbufstats.cache_level_11_bytes,
	  { PMDA_PMID(2, 30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_hits */
	{ &dbufstats.hash_hits,
	  { PMDA_PMID(2, 31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_misses */
	{ &dbufstats.hash_misses,
	  { PMDA_PMID(2, 32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_collisions */
	{ &dbufstats.hash_collisions,
	  { PMDA_PMID(2, 33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_elements */
	{ &dbufstats.hash_elements,
	  { PMDA_PMID(2, 34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_elements_max */
	{ &dbufstats.hash_elements_max,
	  { PMDA_PMID(2, 35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_chains */
	{ &dbufstats.hash_chains,
	  { PMDA_PMID(2, 36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_chain_max */
	{ &dbufstats.hash_chain_max,
	  { PMDA_PMID(2, 37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hash_insert_race */
	{ &dbufstats.hash_insert_race,
	  { PMDA_PMID(2, 38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* metadata_cache_count */
	{ &dbufstats.metadata_cache_count,
	  { PMDA_PMID(2, 39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* metadata_cache_size_bytes */
	{ &dbufstats.metadata_cache_size_bytes,
	  { PMDA_PMID(2, 40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* metadata_cache_size_bytes_max */
	{ &dbufstats.metadata_cache_size_bytes_max,
	  { PMDA_PMID(2, 41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* metadata_cache_overflow */
	{ &dbufstats.metadata_cache_overflow,
	  { PMDA_PMID(2, 42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  DMU_TX  */
/*---------------------------------------------------------------------------*/
/* dmu_tx_assigned */
	{ &dmu_tx.assigned,
	  { PMDA_PMID(3, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_delay */
	{ &dmu_tx.delay,
	  { PMDA_PMID(3, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_error */
	{ &dmu_tx.error,
	  { PMDA_PMID(3, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_suspended */
	{ &dmu_tx.suspended,
	  { PMDA_PMID(3, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_group */
	{ &dmu_tx.group,
	  { PMDA_PMID(3, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_memory_reserve */
	{ &dmu_tx.memory_reserve,
	  { PMDA_PMID(3, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_memory_reclaim */
	{ &dmu_tx.memory_reclaim,
	  { PMDA_PMID(3, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_dirty_throttle */
	{ &dmu_tx.dirty_throttle,
	  { PMDA_PMID(3, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_dirty_delay */
	{ &dmu_tx.dirty_delay,
	  { PMDA_PMID(3, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_dirty_over_max */
	{ &dmu_tx.dirty_over_max,
	  { PMDA_PMID(3, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_dirty_frees_delay */
	{ &dmu_tx.dirty_frees_delay,
	  { PMDA_PMID(3, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dmu_tx_quota */
	{ &dmu_tx.quota,
	  { PMDA_PMID(3, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  DNODESTATS */
/*---------------------------------------------------------------------------*/
/* dnode_hold_dbuf_hold */
	{ &dnodestats.hold_dbuf_hold,
	  { PMDA_PMID(4, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_dbuf_read */
	{ &dnodestats.hold_dbuf_read,
	  { PMDA_PMID(4, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_alloc_hits */
	{ &dnodestats.hold_alloc_hits,
	  { PMDA_PMID(4, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_alloc_misses */
	{ &dnodestats.hold_alloc_misses,
	  { PMDA_PMID(4, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_alloc_interior */
	{ &dnodestats.hold_alloc_interior,
	  { PMDA_PMID(4, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_alloc_lock_retry */
	{ &dnodestats.hold_alloc_lock_retry,
	  { PMDA_PMID(4, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_alloc_lock_misses */
	{ &dnodestats.hold_alloc_lock_misses,
	  { PMDA_PMID(4, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_alloc_type_none */
	{ &dnodestats.hold_alloc_type_none,
	  { PMDA_PMID(4, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_free_hits */
	{ &dnodestats.hold_free_hits,
	  { PMDA_PMID(4, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_free_misses */
	{ &dnodestats.hold_free_misses,
	  { PMDA_PMID(4, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_free_lock_misses */
	{ &dnodestats.hold_free_lock_misses,
	  { PMDA_PMID(4, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_free_lock_retry */
	{ &dnodestats.hold_free_lock_retry,
	  { PMDA_PMID(4, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_free_overflow */
	{ &dnodestats.hold_free_overflow,
	  { PMDA_PMID(4, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_hold_free_refcount */
	{ &dnodestats.hold_free_refcount,
	  { PMDA_PMID(4, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_free_interior_lock_retry */
	{ &dnodestats.free_interior_lock_retry,
	  { PMDA_PMID(4, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_allocate */
	{ &dnodestats.allocate,
	  { PMDA_PMID(4, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_reallocate */
	{ &dnodestats.reallocate,
	  { PMDA_PMID(4, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_buf_evict */
	{ &dnodestats.buf_evict,
	  { PMDA_PMID(4, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_alloc_next_chunk */
	{ &dnodestats.alloc_next_chunk,
	  { PMDA_PMID(4, 18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_alloc_race */
	{ &dnodestats.alloc_race,
	  { PMDA_PMID(4, 19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_alloc_next_block */
	{ &dnodestats.alloc_next_block,
	  { PMDA_PMID(4, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_move_invalid */
	{ &dnodestats.move_invalid,
	  { PMDA_PMID(4, 21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_move_recheck1 */
	{ &dnodestats.move_recheck1,
	  { PMDA_PMID(4, 22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_move_recheck2 */
	{ &dnodestats.move_recheck2,
	  { PMDA_PMID(4, 23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_move_special */
	{ &dnodestats.move_special,
	  { PMDA_PMID(4, 24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_move_handle */
	{ &dnodestats.move_handle,
	  { PMDA_PMID(4, 25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_move_rwlock */
	{ &dnodestats.move_rwlock,
	  { PMDA_PMID(4, 26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dnode_move_active */
	{ &dnodestats.move_active,
	  { PMDA_PMID(4, 27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  FMSTATS  */
/*---------------------------------------------------------------------------*/
/* erpt_dropped */
	{ &fmstats.erpt_dropped,
	  { PMDA_PMID(5, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* erpt_set_failed */
	{ &fmstats.erpt_set_failed,
	  { PMDA_PMID(5, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* fmri_set_failed */
	{ &fmstats.fmri_set_failed,
	  { PMDA_PMID(5, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* payload_set_failed */
	{ &fmstats.payload_set_failed,
	  { PMDA_PMID(5, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  VDEVSTATS  */
/*---------------------------------------------------------------------------*/
/* delegations */
	{ &vdev_cachestats.delegations,
	  { PMDA_PMID(6, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* hits */
	{ &vdev_cachestats.hits,
	  { PMDA_PMID(6, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* misses */
	{ &vdev_cachestats.misses,
	  { PMDA_PMID(6, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* rotating_linear */
	{ &vdev_mirrorstats.rotating_linear,
	  { PMDA_PMID(6, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* rotating_offset */
	{ &vdev_mirrorstats.rotating_offset,
	  { PMDA_PMID(6, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* rotating_seek */
	{ &vdev_mirrorstats.rotating_seek,
	  { PMDA_PMID(6, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* non_rotating_linear */
	{ &vdev_mirrorstats.non_rotating_linear,
	  { PMDA_PMID(6, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* non_rotating_seek */
	{ &vdev_mirrorstats.non_rotating_seek,
	  { PMDA_PMID(6, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* preferred_found */
	{ &vdev_mirrorstats.preferred_found,
	  { PMDA_PMID(6, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* preferred_not_found */
	{ &vdev_mirrorstats.preferred_not_found,
	  { PMDA_PMID(6, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  XUIOSTATS  */
/*---------------------------------------------------------------------------*/
/* onloan_read_buf */
	{ &xuiostats.onloan_read_buf,
	  { PMDA_PMID(7, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* onloan_write_buf */
	{ &xuiostats.onloan_write_buf,
	  { PMDA_PMID(7, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* read_buf_copied */
	{ &xuiostats.read_buf_copied,
	  { PMDA_PMID(7, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* read_buf_nocopy */
	{ &xuiostats.read_buf_nocopy,
	  { PMDA_PMID(7, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* write_buf_copied */
	{ &xuiostats.write_buf_copied,
	  { PMDA_PMID(7, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* write_buf_nocopy */
	{ &xuiostats.write_buf_nocopy,
	  { PMDA_PMID(7, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  ZFETCHSTATS  */
/*---------------------------------------------------------------------------*/
/* hits */
	{ &zfetchstats.hits,
	  { PMDA_PMID(8, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* misses */
	{ &zfetchstats.misses,
	  { PMDA_PMID(8, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* max_streams */
	{ &zfetchstats.max_streams,
	  { PMDA_PMID(8, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  ZILSTATS  */
/*---------------------------------------------------------------------------*/
/* commit_count */
	{ &zilstats.commit_count,
	  { PMDA_PMID(9, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* commit_writer_count */
	{ &zilstats.commit_writer_count,
	  { PMDA_PMID(9, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_count */
	{ &zilstats.itx_count,
	  { PMDA_PMID(9, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_indirect_count */
	{ &zilstats.itx_indirect_count,
	  { PMDA_PMID(9, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_indirect_bytes */
	{ &zilstats.itx_indirect_bytes,
	  { PMDA_PMID(9, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_copied_count */
	{ &zilstats.itx_copied_count,
	  { PMDA_PMID(9, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_copied_bytes */
	{ &zilstats.itx_copied_bytes,
	  { PMDA_PMID(9, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_needcopy_count */
	{ &zilstats.itx_needcopy_count,
	  { PMDA_PMID(9, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_needcopy_bytes */
	{ &zilstats.itx_needcopy_bytes,
	  { PMDA_PMID(9, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_metaslab_normal_count */
	{ &zilstats.itx_metaslab_normal_count,
	  { PMDA_PMID(9, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_metaslab_normal_bytes */
	{ &zilstats.itx_metaslab_normal_bytes,
	  { PMDA_PMID(9, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_metaslab_slog_count */
	{ &zilstats.itx_metaslab_slog_count,
	  { PMDA_PMID(9, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* itx_metaslab_slog_bytes */
	{ &zilstats.itx_metaslab_slog_bytes,
	  { PMDA_PMID(9, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/*---------------------------------------------------------------------------*/
/*  POOLSTATS  */
/*---------------------------------------------------------------------------*/
/* state */
	{ NULL,
	  { PMDA_PMID(10, ZFS_POOL_STATE), PM_TYPE_U32, ZFS_POOL_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* nread */
	{ NULL,
	  { PMDA_PMID(10, ZFS_POOL_NREAD), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* nwritten */
	{ NULL,
	  { PMDA_PMID(10, ZFS_POOL_NWRITTEN), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* reads */
	{ NULL,
	  { PMDA_PMID(10, ZFS_POOL_READS), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* writes */
	{ NULL,
	  { PMDA_PMID(10, ZFS_POOL_WRITES), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* wtime */
	{ NULL,
	  { PMDA_PMID(10, ZFS_POOL_WTIME), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* wlentime */
	{ NULL,
	  { PMDA_PMID(10, ZFS_POOL_WLENTIME), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* wupdate */
	{ NULL,
	  { PMDA_PMID(10, ZFS_POOL_WUPDATE), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* rtime */
	{ NULL,
	  { PMDA_PMID(10, ZFS_POOL_RTIME), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* rlentime */
	{ NULL,
	  { PMDA_PMID(10, ZFS_POOL_RLENTIME), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* rupdate */
	{ NULL,
	  { PMDA_PMID(10, ZFS_POOL_RUPDATE), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* wcnt */
	{ NULL,
	  { PMDA_PMID(10, ZFS_POOL_WCNT), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* rcnt */
	{ NULL,
	  { PMDA_PMID(10, ZFS_POOL_RCNT), PM_TYPE_U64, ZFS_POOL_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } }
};

static int
zfs_fetch(int numpmid, pmID *pmidlist, pmResult **resp, pmdaExt *pmda)
{
        regcomp(&rgx_row, "^([^ ]+)[ ]+[0-9][ ]+([0-9]+)", REG_EXTENDED);
	zfs_arcstats_refresh(&arcstats, &rgx_row);
	zfs_abdstats_refresh(&abdstats, &rgx_row);
	zfs_dbufstats_refresh(&dbufstats, &rgx_row);
	zfs_dmu_tx_refresh(&dmu_tx, &rgx_row);
	zfs_dnodestats_refresh(&dnodestats, &rgx_row);
	zfs_xuiostats_refresh(&xuiostats, &rgx_row);
	zfs_zfetchstats_refresh(&zfetchstats, &rgx_row);
	zfs_zilstats_refresh(&zilstats, &rgx_row);
	zfs_vdev_cachestats_refresh(&vdev_cachestats, &rgx_row);
	zfs_vdev_mirrorstats_refresh(&vdev_mirrorstats, &rgx_row);
	regfree(&rgx_row);
        zfs_poolstats_refresh(&poolstats, &pools, &indomtab[ZFS_POOL_INDOM]);
	return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
zfs_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
        __pmID_int *idp = (__pmID_int *)&(mdesc->m_desc.pmid);
        
        if (idp->cluster == 10) { // && mdesc->m_desc.indom == ZFS_POOL_INDOM) {
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
        } else {
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
        zfs_pools_init(&poolstats, &pools, &indomtab[ZFS_POOL_INDOM]);
        return pmdaInstance(indom, inst, name, result, pmda);
}

void
__PMDA_INIT_CALL
zfs_init(pmdaInterface *dp)
{
	char helppath[MAXPATHLEN];
	int sep = pmPathSeparator();

	if (_isDSO) {
	    pmsprintf(helppath, sizeof(helppath), "%s%c" "zfs" "%c" "help",
			    pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	    pmdaDSO(dp, PMDA_INTERFACE_7, "ZFS DSO", helppath);
	}

	if (dp->status != 0)
	        return;
	
        zfs_pools_init(&poolstats, &pools, &indomtab[ZFS_POOL_INDOM]);
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
    int			sep = pmPathSeparator();
    pmdaInterface	dispatch;
    char		helppath[MAXPATHLEN];

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
