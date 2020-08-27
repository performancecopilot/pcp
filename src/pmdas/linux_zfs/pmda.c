#include <regex.h>
#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "zfs_arcstats.h"
#include "zfs_abdstats.h"
#include "zfs_dbufstats.h"
#include "zfs_dmu_tx.h"
#include "zfs_dnodestats.h"
#include "zfs_fmstats.h"
#include "zfs_xuiostats.h"
#include "zfs_zfetchstats.h"

regex_t rgx_row;
static zfs_arcstats_t arcstats;
static zfs_abdstats_t abdstats;
static zfs_dbufstats_t dbufstats;
static zfs_dmu_tx_t dmu_tx;
static zfs_dnodestats_t dnodestats;
static zfs_fmstats_t fmstats;
static zfs_xuiostats_t xuiostats;
static zfs_zfetchstats_t zfetchstats;

static pmdaMetric metrictab[] = {
/*---------------------------------------------------------------------------*/
/*  ARCSTATS  */
/*---------------------------------------------------------------------------*/
/* hits */
	{ &arcstats.hits,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* misses */
	{ &arcstats.misses,
	  { PMDA_PMID(0, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* demand_data_hits */
	{ &arcstats.demand_data_hits,
	  { PMDA_PMID(0, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* demand_data_misses */
	{ &arcstats.demand_data_misses,
	  { PMDA_PMID(0, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* demand_metadata_hits */
	{ &arcstats.demand_metadata_hits,
	  { PMDA_PMID(0, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* demand_metadata_misses */
	{ &arcstats.demand_metadata_misses,
	  { PMDA_PMID(0, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* prefetch_data_hits */
	{ &arcstats.prefetch_data_hits,
	  { PMDA_PMID(0, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* prefetch_data_misses */
	{ &arcstats.prefetch_data_misses,
	  { PMDA_PMID(0, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* prefetch_metadata_hits */
	{ &arcstats.prefetch_metadata_hits,
	  { PMDA_PMID(0, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* prefetch_metadata_misses */
	{ &arcstats.prefetch_metadata_misses,
	  { PMDA_PMID(0, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_hits */
	{ &arcstats.mru_hits,
	  { PMDA_PMID(0, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_ghost_hits */
	{ &arcstats.mru_ghost_hits,
	  { PMDA_PMID(0, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_hits */
	{ &arcstats.mfu_hits,
	  { PMDA_PMID(0, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_ghost_hits */
	{ &arcstats.mfu_ghost_hits,
	  { PMDA_PMID(0, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* deleted */
	{ &arcstats.deleted,
	  { PMDA_PMID(0, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mutex_miss */
	{ &arcstats.mutex_miss,
	  { PMDA_PMID(0, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* access_skip */
	{ &arcstats.access_skip,
	  { PMDA_PMID(0, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* evict_skip */
	{ &arcstats.evict_skip,
	  { PMDA_PMID(0, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* evict_not_enough */
	{ &arcstats.evict_not_enough,
	  { PMDA_PMID(0, 18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* evict_l2_cached */
	{ &arcstats.evict_l2_cached,
	  { PMDA_PMID(0, 19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* evict_l2_eligible */
	{ &arcstats.evict_l2_eligible,
	  { PMDA_PMID(0, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* evict_l2_ineligible */
	{ &arcstats.evict_l2_ineligible,
	  { PMDA_PMID(0, 21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* evict_l2_skip */
	{ &arcstats.evict_l2_skip,
	  { PMDA_PMID(0, 22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_elements */
	{ &arcstats.hash_elements,
	  { PMDA_PMID(0, 23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_elements_max */
	{ &arcstats.hash_elements_max,
	  { PMDA_PMID(0, 24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_collisions */
	{ &arcstats.hash_collisions,
	  { PMDA_PMID(0, 25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_chains */
	{ &arcstats.hash_chains,
	  { PMDA_PMID(0, 26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_chain_max */
	{ &arcstats.hash_chain_max,
	  { PMDA_PMID(0, 27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* p */
	{ &arcstats.p,
	  { PMDA_PMID(0, 28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* c */
	{ &arcstats.c,
	  { PMDA_PMID(0, 29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* c_min */
	{ &arcstats.c_min,
	  { PMDA_PMID(0, 30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* c_max */
	{ &arcstats.c_max,
	  { PMDA_PMID(0, 31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* size */
	{ &arcstats.size,
	  { PMDA_PMID(0, 32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* compressed_size */
	{ &arcstats.compressed_size,
	  { PMDA_PMID(0, 33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* uncompressed_size */
	{ &arcstats.uncompressed_size,
	  { PMDA_PMID(0, 34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* overhead_size */
	{ &arcstats.overhead_size,
	  { PMDA_PMID(0, 35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hdr_size */
	{ &arcstats.hdr_size,
	  { PMDA_PMID(0, 36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* data_size */
	{ &arcstats.data_size,
	  { PMDA_PMID(0, 37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* metadata_size */
	{ &arcstats.metadata_size,
	  { PMDA_PMID(0, 38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dbuf_size */
	{ &arcstats.dbuf_size,
	  { PMDA_PMID(0, 39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_size */
	{ &arcstats.dnode_size,
	  { PMDA_PMID(0, 40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* bonus_size */
	{ &arcstats.bonus_size,
	  { PMDA_PMID(0, 41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* anon_size */
	{ &arcstats.anon_size,
	  { PMDA_PMID(0, 42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* anon_evictable_data */
	{ &arcstats.anon_evictable_data,
	  { PMDA_PMID(0, 43), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* anon_evictable_metadata */
	{ &arcstats.anon_evictable_metadata,
	  { PMDA_PMID(0, 44), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_size */
	{ &arcstats.mru_size,
	  { PMDA_PMID(0, 45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_evictable_data */
	{ &arcstats.mru_evictable_data,
	  { PMDA_PMID(0, 46), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_evictable_metadata */
	{ &arcstats.mru_evictable_metadata,
	  { PMDA_PMID(0, 47), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_ghost_size */
	{ &arcstats.mru_ghost_size,
	  { PMDA_PMID(0, 48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_ghost_evictable_data */
	{ &arcstats.mru_ghost_evictable_data,
	  { PMDA_PMID(0, 49), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_ghost_evictable_metadata */
	{ &arcstats.mru_ghost_evictable_metadata,
	  { PMDA_PMID(0, 50), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_size */
	{ &arcstats.mfu_size,
	  { PMDA_PMID(0, 51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_evictable_data */
	{ &arcstats.mfu_evictable_data,
	  { PMDA_PMID(0, 52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_evictable_metadata */
	{ &arcstats.mfu_evictable_metadata,
	  { PMDA_PMID(0, 53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_ghost_size */
	{ &arcstats.mfu_ghost_size,
	  { PMDA_PMID(0, 54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_ghost_evictable_data */
	{ &arcstats.mfu_ghost_evictable_data,
	  { PMDA_PMID(0, 55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_ghost_evictable_metadata */
	{ &arcstats.mfu_ghost_evictable_metadata,
	  { PMDA_PMID(0, 56), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_hits */
	{ &arcstats.l2_hits,
	  { PMDA_PMID(0, 57), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_misses */
	{ &arcstats.l2_misses,
	  { PMDA_PMID(0, 58), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_feeds */
	{ &arcstats.l2_feeds,
	  { PMDA_PMID(0, 59), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_rw_clash */
	{ &arcstats.l2_rw_clash,
	  { PMDA_PMID(0, 60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_read_bytes */
	{ &arcstats.l2_read_bytes,
	  { PMDA_PMID(0, 61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_write_bytes */
	{ &arcstats.l2_write_bytes,
	  { PMDA_PMID(0, 62), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_writes_sent */
	{ &arcstats.l2_writes_sent,
	  { PMDA_PMID(0, 63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_writes_done */
	{ &arcstats.l2_writes_done,
	  { PMDA_PMID(0, 64), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_writes_error */
	{ &arcstats.l2_writes_error,
	  { PMDA_PMID(0, 65), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_writes_lock_retry */
	{ &arcstats.l2_writes_lock_retry,
	  { PMDA_PMID(0, 66), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_evict_lock_retry */
	{ &arcstats.l2_evict_lock_retry,
	  { PMDA_PMID(0, 67), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_evict_reading */
	{ &arcstats.l2_evict_reading,
	  { PMDA_PMID(0, 68), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_evict_l1cached */
	{ &arcstats.l2_evict_l1cached,
	  { PMDA_PMID(0, 69), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_free_on_write */
	{ &arcstats.l2_free_on_write,
	  { PMDA_PMID(0, 70), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_abort_lowmem */
	{ &arcstats.l2_abort_lowmem,
	  { PMDA_PMID(0, 71), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_cksum_bad */
	{ &arcstats.l2_cksum_bad,
	  { PMDA_PMID(0, 72), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_io_error */
	{ &arcstats.l2_io_error,
	  { PMDA_PMID(0, 73), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_size */
	{ &arcstats.l2_size,
	  { PMDA_PMID(0, 74), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_asize */
	{ &arcstats.l2_asize,
	  { PMDA_PMID(0, 75), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_hdr_size */
	{ &arcstats.l2_hdr_size,
	  { PMDA_PMID(0, 76), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* memory_throttle_count */
	{ &arcstats.memory_throttle_count,
	  { PMDA_PMID(0, 77), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* memory_direct_count */
	{ &arcstats.memory_direct_count,
	  { PMDA_PMID(0, 78), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* memory_indirect_count */
	{ &arcstats.memory_indirect_count,
	  { PMDA_PMID(0, 79), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* memory_all_bytes */
	{ &arcstats.memory_all_bytes,
	  { PMDA_PMID(0, 80), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* memory_free_bytes */
	{ &arcstats.memory_free_bytes,
	  { PMDA_PMID(0, 81), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* memory_available_bytes */
	{ &arcstats.memory_available_bytes,
	  { PMDA_PMID(0, 82), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_no_grow */
	{ &arcstats.arc_no_grow,
	  { PMDA_PMID(0, 83), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_tempreserve */
	{ &arcstats.arc_tempreserve,
	  { PMDA_PMID(0, 84), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_loaned_bytes */
	{ &arcstats.arc_loaned_bytes,
	  { PMDA_PMID(0, 85), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_prune */
	{ &arcstats.arc_prune,
	  { PMDA_PMID(0, 86), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_meta_used */
	{ &arcstats.arc_meta_used,
	  { PMDA_PMID(0, 87), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_meta_limit */
	{ &arcstats.arc_meta_limit,
	  { PMDA_PMID(0, 88), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_dnode_limit */
	{ &arcstats.arc_dnode_limit,
	  { PMDA_PMID(0, 89), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_meta_max */
	{ &arcstats.arc_meta_max,
	  { PMDA_PMID(0, 90), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_meta_min */
	{ &arcstats.arc_meta_min,
	  { PMDA_PMID(0, 91), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* async_upgrade_sync */
	{ &arcstats.async_upgrade_sync,
	  { PMDA_PMID(0, 92), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* demand_hit_predictive_prefetch */
	{ &arcstats.demand_hit_predictive_prefetch,
	  { PMDA_PMID(0, 93), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* demand_hit_prescient_prefetch */
	{ &arcstats.demand_hit_prescient_prefetch,
	  { PMDA_PMID(0, 94), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_need_free */
	{ &arcstats.arc_need_free,
	  { PMDA_PMID(0, 95), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_sys_free */
	{ &arcstats.arc_sys_free,
	  { PMDA_PMID(0, 96), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_raw_size */
	{ &arcstats.arc_raw_size,
	  { PMDA_PMID(0, 97), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/*---------------------------------------------------------------------------*/
/*  ABDSTATS  */
/*---------------------------------------------------------------------------*/
/* struct_size */
	{ &abdstats.struct_size,
	  { PMDA_PMID(1, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* linear_cnt */
	{ &abdstats.linear_cnt,
	  { PMDA_PMID(1, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* linear_data_size */
	{ &abdstats.linear_data_size,
	  { PMDA_PMID(1, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_cnt */
	{ &abdstats.scatter_cnt,
	  { PMDA_PMID(1, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_data_size */
	{ &abdstats.scatter_data_size,
	  { PMDA_PMID(1, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_chunk_waste */
	{ &abdstats.scatter_chunk_waste,
	  { PMDA_PMID(1, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_order_0 */
	{ &abdstats.scatter_order_0,
	  { PMDA_PMID(1, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_order_1 */
	{ &abdstats.scatter_order_1,
	  { PMDA_PMID(1, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_order_2 */
	{ &abdstats.scatter_order_2,
	  { PMDA_PMID(1, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_order_3 */
	{ &abdstats.scatter_order_3,
	  { PMDA_PMID(1, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_order_4 */
	{ &abdstats.scatter_order_4,
	  { PMDA_PMID(1, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_order_5 */
	{ &abdstats.scatter_order_5,
	  { PMDA_PMID(1, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_order_6 */
	{ &abdstats.scatter_order_6,
	  { PMDA_PMID(1, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_order_7 */
	{ &abdstats.scatter_order_7,
	  { PMDA_PMID(1, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_order_8 */
	{ &abdstats.scatter_order_8,
	  { PMDA_PMID(1, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_order_9 */
	{ &abdstats.scatter_order_9,
	  { PMDA_PMID(1, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_order_10 */
	{ &abdstats.scatter_order_10,
	  { PMDA_PMID(1, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_page_multi_chunk */
	{ &abdstats.scatter_page_multi_chunk,
	  { PMDA_PMID(1, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_page_multi_zone */
	{ &abdstats.scatter_page_multi_zone,
	  { PMDA_PMID(1, 18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_page_alloc_retry */
	{ &abdstats.scatter_page_alloc_retry,
	  { PMDA_PMID(1, 19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* scatter_sg_table_retry */
	{ &abdstats.scatter_sg_table_retry,
	  { PMDA_PMID(1, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/*---------------------------------------------------------------------------*/
/*  DBUFSTATS  */
/*---------------------------------------------------------------------------*/
/* cache_count */
	{ &dbufstats.cache_count,
	  { PMDA_PMID(2, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_size_bytes */
	{ &dbufstats.cache_size_bytes,
	  { PMDA_PMID(2, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_size_bytes_max */
	{ &dbufstats.cache_size_bytes_max,
	  { PMDA_PMID(2, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_target_bytes */
	{ &dbufstats.cache_target_bytes,
	  { PMDA_PMID(2, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_lowater_bytes */
	{ &dbufstats.cache_lowater_bytes,
	  { PMDA_PMID(2, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_hiwater_bytes */
	{ &dbufstats.cache_hiwater_bytes,
	  { PMDA_PMID(2, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_total_evicts */
	{ &dbufstats.cache_total_evicts,
	  { PMDA_PMID(2, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_0 */
	{ &dbufstats.cache_level_0,
	  { PMDA_PMID(2, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_1 */
	{ &dbufstats.cache_level_1,
	  { PMDA_PMID(2, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_2 */
	{ &dbufstats.cache_level_2,
	  { PMDA_PMID(2, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_3 */
	{ &dbufstats.cache_level_3,
	  { PMDA_PMID(2, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_4 */
	{ &dbufstats.cache_level_4,
	  { PMDA_PMID(2, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_5 */
	{ &dbufstats.cache_level_5,
	  { PMDA_PMID(2, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_6 */
	{ &dbufstats.cache_level_6,
	  { PMDA_PMID(2, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_7 */
	{ &dbufstats.cache_level_7,
	  { PMDA_PMID(2, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_8 */
	{ &dbufstats.cache_level_8,
	  { PMDA_PMID(2, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_9 */
	{ &dbufstats.cache_level_9,
	  { PMDA_PMID(2, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_10 */
	{ &dbufstats.cache_level_10,
	  { PMDA_PMID(2, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_11 */
	{ &dbufstats.cache_level_11,
	  { PMDA_PMID(2, 18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_0_bytes */
	{ &dbufstats.cache_level_0_bytes,
	  { PMDA_PMID(2, 19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_1_bytes */
	{ &dbufstats.cache_level_1_bytes,
	  { PMDA_PMID(2, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_2_bytes */
	{ &dbufstats.cache_level_2_bytes,
	  { PMDA_PMID(2, 21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_3_bytes */
	{ &dbufstats.cache_level_3_bytes,
	  { PMDA_PMID(2, 22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_4_bytes */
	{ &dbufstats.cache_level_4_bytes,
	  { PMDA_PMID(2, 23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_5_bytes */
	{ &dbufstats.cache_level_5_bytes,
	  { PMDA_PMID(2, 24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_6_bytes */
	{ &dbufstats.cache_level_6_bytes,
	  { PMDA_PMID(2, 25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_7_bytes */
	{ &dbufstats.cache_level_7_bytes,
	  { PMDA_PMID(2, 26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_8_bytes */
	{ &dbufstats.cache_level_8_bytes,
	  { PMDA_PMID(2, 27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_9_bytes */
	{ &dbufstats.cache_level_9_bytes,
	  { PMDA_PMID(2, 28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_10_bytes */
	{ &dbufstats.cache_level_10_bytes,
	  { PMDA_PMID(2, 29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* cache_level_11_bytes */
	{ &dbufstats.cache_level_11_bytes,
	  { PMDA_PMID(2, 30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_hits */
	{ &dbufstats.hash_hits,
	  { PMDA_PMID(2, 31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_misses */
	{ &dbufstats.hash_misses,
	  { PMDA_PMID(2, 32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_collisions */
	{ &dbufstats.hash_collisions,
	  { PMDA_PMID(2, 33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_elements */
	{ &dbufstats.hash_elements,
	  { PMDA_PMID(2, 34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_elements_max */
	{ &dbufstats.hash_elements_max,
	  { PMDA_PMID(2, 35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_chains */
	{ &dbufstats.hash_chains,
	  { PMDA_PMID(2, 36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_chain_max */
	{ &dbufstats.hash_chain_max,
	  { PMDA_PMID(2, 37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_insert_race */
	{ &dbufstats.hash_insert_race,
	  { PMDA_PMID(2, 38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* metadata_cache_count */
	{ &dbufstats.metadata_cache_count,
	  { PMDA_PMID(2, 39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* metadata_cache_size_bytes */
	{ &dbufstats.metadata_cache_size_bytes,
	  { PMDA_PMID(2, 40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* metadata_cache_size_bytes_max */
	{ &dbufstats.metadata_cache_size_bytes_max,
	  { PMDA_PMID(2, 41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* metadata_cache_overflow */
	{ &dbufstats.metadata_cache_overflow,
	  { PMDA_PMID(2, 42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/*---------------------------------------------------------------------------*/
/*  DMU_TX  */
/*---------------------------------------------------------------------------*/
/* dmu_tx_assigned */
	{ &dmu_tx.assigned,
	  { PMDA_PMID(3, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dmu_tx_delay */
	{ &dmu_tx.delay,
	  { PMDA_PMID(3, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dmu_tx_error */
	{ &dmu_tx.error,
	  { PMDA_PMID(3, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dmu_tx_suspended */
	{ &dmu_tx.suspended,
	  { PMDA_PMID(3, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dmu_tx_group */
	{ &dmu_tx.group,
	  { PMDA_PMID(3, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dmu_tx_memory_reserve */
	{ &dmu_tx.memory_reserve,
	  { PMDA_PMID(3, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dmu_tx_memory_reclaim */
	{ &dmu_tx.memory_reclaim,
	  { PMDA_PMID(3, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dmu_tx_dirty_throttle */
	{ &dmu_tx.dirty_throttle,
	  { PMDA_PMID(3, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dmu_tx_dirty_delay */
	{ &dmu_tx.dirty_delay,
	  { PMDA_PMID(3, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dmu_tx_dirty_over_max */
	{ &dmu_tx.dirty_over_max,
	  { PMDA_PMID(3, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dmu_tx_dirty_frees_delay */
	{ &dmu_tx.dirty_frees_delay,
	  { PMDA_PMID(3, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dmu_tx_quota */
	{ &dmu_tx.quota,
	  { PMDA_PMID(3, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/*---------------------------------------------------------------------------*/
/*  DNODESTATS */
/*---------------------------------------------------------------------------*/
/* dnode_hold_dbuf_hold */
	{ &dnodestats.hold_dbuf_hold,
	  { PMDA_PMID(4, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_hold_dbuf_read */
	{ &dnodestats.hold_dbuf_read,
	  { PMDA_PMID(4, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_hold_alloc_hits */
	{ &dnodestats.hold_alloc_hits,
	  { PMDA_PMID(4, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_hold_alloc_misses */
	{ &dnodestats.hold_alloc_misses,
	  { PMDA_PMID(4, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_hold_alloc_interior */
	{ &dnodestats.hold_alloc_interior,
	  { PMDA_PMID(4, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_hold_alloc_lock_retry */
	{ &dnodestats.hold_alloc_lock_retry,
	  { PMDA_PMID(4, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_hold_alloc_lock_misses */
	{ &dnodestats.hold_alloc_lock_misses,
	  { PMDA_PMID(4, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_hold_alloc_type_none */
	{ &dnodestats.hold_alloc_type_none,
	  { PMDA_PMID(4, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_hold_free_hits */
	{ &dnodestats.hold_free_hits,
	  { PMDA_PMID(4, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_hold_free_misses */
	{ &dnodestats.hold_free_misses,
	  { PMDA_PMID(4, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_hold_free_lock_misses */
	{ &dnodestats.hold_free_lock_misses,
	  { PMDA_PMID(4, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_hold_free_lock_retry */
	{ &dnodestats.hold_free_lock_retry,
	  { PMDA_PMID(4, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_hold_free_overflow */
	{ &dnodestats.hold_free_overflow,
	  { PMDA_PMID(4, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_hold_free_refcount */
	{ &dnodestats.hold_free_refcount,
	  { PMDA_PMID(4, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_free_interior_lock_retry */
	{ &dnodestats.free_interior_lock_retry,
	  { PMDA_PMID(4, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_allocate */
	{ &dnodestats.allocate,
	  { PMDA_PMID(4, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_reallocate */
	{ &dnodestats.reallocate,
	  { PMDA_PMID(4, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_buf_evict */
	{ &dnodestats.buf_evict,
	  { PMDA_PMID(4, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_alloc_next_chunk */
	{ &dnodestats.alloc_next_chunk,
	  { PMDA_PMID(4, 18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_alloc_race */
	{ &dnodestats.alloc_race,
	  { PMDA_PMID(4, 19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_alloc_next_block */
	{ &dnodestats.alloc_next_block,
	  { PMDA_PMID(4, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_move_invalid */
	{ &dnodestats.move_invalid,
	  { PMDA_PMID(4, 21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_move_recheck1 */
	{ &dnodestats.move_recheck1,
	  { PMDA_PMID(4, 22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_move_recheck2 */
	{ &dnodestats.move_recheck2,
	  { PMDA_PMID(4, 23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_move_special */
	{ &dnodestats.move_special,
	  { PMDA_PMID(4, 24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_move_handle */
	{ &dnodestats.move_handle,
	  { PMDA_PMID(4, 25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_move_rwlock */
	{ &dnodestats.move_rwlock,
	  { PMDA_PMID(4, 26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_move_active */
	{ &dnodestats.move_active,
	  { PMDA_PMID(4, 27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/*---------------------------------------------------------------------------*/
/*  FMSTATS  */
/*---------------------------------------------------------------------------*/
/* erpt_dropped */
	{ &fmstats.erpt_dropped,
	  { PMDA_PMID(5, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* erpt_set_failed */
	{ &fmstats.erpt_set_failed,
	  { PMDA_PMID(5, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* fmri_set_failed */
	{ &fmstats.fmri_set_failed,
	  { PMDA_PMID(5, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* payload_set_failed */
	{ &fmstats.payload_set_failed,
	  { PMDA_PMID(5, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/*---------------------------------------------------------------------------*/
/*  XUIOSTATS  */
/*---------------------------------------------------------------------------*/
/* onloan_read_buf */
	{ &xuiostats.onloan_read_buf,
	  { PMDA_PMID(7, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* onloan_write_buf */
	{ &xuiostats.onloan_write_buf,
	  { PMDA_PMID(7, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* read_buf_copied */
	{ &xuiostats.read_buf_copied,
	  { PMDA_PMID(7, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* read_buf_nocopy */
	{ &xuiostats.read_buf_nocopy,
	  { PMDA_PMID(7, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* write_buf_copied */
	{ &xuiostats.write_buf_copied,
	  { PMDA_PMID(7, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* write_buf_nocopy */
	{ &xuiostats.write_buf_nocopy,
	  { PMDA_PMID(7, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/*---------------------------------------------------------------------------*/
/*  ZFETCHSTATS  */
/*---------------------------------------------------------------------------*/
/* hits */
	{ &zfetchstats.hits,
	  { PMDA_PMID(8, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* misses */
	{ &zfetchstats.misses,
	  { PMDA_PMID(8, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* max_streams */
	{ &zfetchstats.max_streams,
	  { PMDA_PMID(8, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
};

static int
zfs_store(pmResult *result, pmdaExt *pmda)
{
	int i;
	int sts = 0;
	pmAtomValue av;
	pmValueSet *vsp = NULL;
	__pmID_int *pmidp = NULL;

	/*
	for (i = 0; i < result->numpid; i++) {
		vsp = result->vset[i];
		pmidp = (__pmID_int *)&vsp->pmid;
		if (pmidp->cluster == CLUSTER_ZFS_ARCSTATS)
	*/
	return sts;
}

static int
zfs_fetch(int numpid, pmID *pmidlist, pmResult **resp, pmdaExt *pmda)
{
	zfs_arcstats_fetch(&arcstats, &rgx_row);
	zfs_abdstats_fetch(&abdstats, &rgx_row);
	zfs_dbufstats_fetch(&dbufstats, &rgx_row);
	zfs_dmu_tx_fetch(&dmu_tx, &rgx_row);
	zfs_dnodestats_fetch(&dnodestats, &rgx_row);
	zfs_xuiostats_fetch(&xuiostats, &rgx_row);
	zfs_zfetchstats_fetch(&zfetchstats, &rgx_row);
	return pmdaFetch(numpid, pmidlist, resp, pmda);
}

static int
zfs_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
	unsigned int	cluster = pmID_cluster(mdesc->m_desc.pmid);
	unsigned int	item = pmID_item(mdesc->m_desc.pmid);
	int		sts;

	switch (mdesc->m_desc.type) {
	case PM_TYPE_U32:
		atom->ul = *(__uint32_t *)mdesc->m_user;
		break;
	case PM_TYPE_U64:
		atom->ull = *(__uint64_t *)mdesc->m_user;
		break;
	default:
		return PM_ERR_TYPE;
	}
	return 1;
}

void
__PMDA_INIT_CALL
zfs_init(pmdaInterface *dp)
{
	char helppath[MAXPATHLEN];
	int sep = pmPathSeparator();

	//if (dp->status != 0)
	//	return;
	
	pmsprintf(helppath, sizeof(helppath), "%s%c" "zfs" "%c" "help",
			pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_3, "ZFS DSO", helppath);
	//dp->version.any.instance = zfs_instance;
	dp->version.any.fetch = zfs_fetch;
	dp->version.any.store = zfs_store;
	pmdaSetFetchCallBack(dp, zfs_fetchCallBack);
	pmdaInit(dp, 
			//zfs_indomtab, sizeof(zfs_indomtab)/sizeof(zfs_indomtab[0]),
                        NULL, 0,
			metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
}

int
main(int argc, char **argv)
{
	pmdaInterface dispatch;

	pmSetProgname(argv[0]);
	zfs_init(&dispatch);
	pmdaOpenLog(&dispatch);
	pmdaConnect(&dispatch);
	pmdaMain(&dispatch);
	regfree(&rgx_row);
	exit(0);
}

/*
 * Fetching:
 * zfs_fetch -> pmdaFetch -> zfs_fetchCallback
 *
 * zfs_fetch takes the content of the files from /proc and saves then in 
 * data structures, then calls pmdaFetch.
 *
 * pmdaFetch works through the metric domain and extracts the metric values from
 * the data structures created by zfs_fetch.
 *
 */
