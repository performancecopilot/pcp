#include <regex.h>
#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "zfs_arcstats.h"
#include "zfs_abdstats.h"

regex_t rgx_row;
static zfs_arcstats_t arcstats;
static zfs_abdstats_t abdstats;

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
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* demand_data_hits */
	{ &arcstats.demand_data_hits,
	  { PMDA_PMID(0, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* demand_data_misses */
	{ &arcstats.demand_data_misses,
	  { PMDA_PMID(0, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* demand_metadata_hits */
	{ &arcstats.demand_metadata_hits,
	  { PMDA_PMID(0, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* demand_metadata_misses */
	{ &arcstats.demand_metadata_misses,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* prefetch_data_hits */
	{ &arcstats.prefetch_data_hits,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* prefetch_data_misses */
	{ &arcstats.prefetch_data_misses,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* prefetch_metadata_hits */
	{ &arcstats.prefetch_metadata_hits,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* prefetch_metadata_misses */
	{ &arcstats.prefetch_metadata_misses,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_hits */
	{ &arcstats.mru_hits,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_ghost_hits */
	{ &arcstats.mru_ghost_hits,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_hits */
	{ &arcstats.mfu_hits,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_ghost_hits */
	{ &arcstats.mfu_ghost_hits,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* deleted */
	{ &arcstats.deleted,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mutex_miss */
	{ &arcstats.mutex_miss,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* access_skip */
	{ &arcstats.access_skip,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* evict_skip */
	{ &arcstats.evict_skip,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* evict_not_enough */
	{ &arcstats.evict_not_enough,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* evict_l2_cached */
	{ &arcstats.evict_l2_cached,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* evict_l2_eligible */
	{ &arcstats.evict_l2_eligible,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* evict_l2_ineligible */
	{ &arcstats.evict_l2_ineligible,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* evict_l2_skip */
	{ &arcstats.evict_l2_skip,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_elements */
	{ &arcstats.hash_elements,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_elements_max */
	{ &arcstats.hash_elements_max,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_collisions */
	{ &arcstats.hash_collisions,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_chains */
	{ &arcstats.hash_chains,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hash_chain_max */
	{ &arcstats.hash_chain_max,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* p */
	{ &arcstats.p,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* c */
	{ &arcstats.c,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* c_min */
	{ &arcstats.c_min,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* c_max */
	{ &arcstats.c_max,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* size */
	{ &arcstats.size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* compressed_size */
	{ &arcstats.compressed_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* uncompressed_size */
	{ &arcstats.uncompressed_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* overhead_size */
	{ &arcstats.overhead_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* hdr_size */
	{ &arcstats.hdr_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* data_size */
	{ &arcstats.data_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* metadata_size */
	{ &arcstats.metadata_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dbuf_size */
	{ &arcstats.dbuf_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* dnode_size */
	{ &arcstats.dnode_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* bonus_size */
	{ &arcstats.bonus_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* anon_size */
	{ &arcstats.anon_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* anon_evictable_data */
	{ &arcstats.anon_evictable_data,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* anon_evictable_metadata */
	{ &arcstats.anon_evictable_metadata,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_size */
	{ &arcstats.mru_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_evictable_data */
	{ &arcstats.mru_evictable_data,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_evictable_metadata */
	{ &arcstats.mru_evictable_metadata,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_ghost_size */
	{ &arcstats.mru_ghost_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_ghost_evictable_data */
	{ &arcstats.mru_ghost_evictable_data,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mru_ghost_evictable_metadata */
	{ &arcstats.mru_ghost_evictable_metadata,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_size */
	{ &arcstats.mfu_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_evictable_data */
	{ &arcstats.mfu_evictable_data,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_evictable_metadata */
	{ &arcstats.mfu_evictable_metadata,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_ghost_size */
	{ &arcstats.mfu_ghost_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_ghost_evictable_data */
	{ &arcstats.mfu_ghost_evictable_data,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mfu_ghost_evictable_metadata */
	{ &arcstats.mfu_ghost_evictable_metadata,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_hits */
	{ &arcstats.l2_hits,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_misses */
	{ &arcstats.l2_misses,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_feeds */
	{ &arcstats.l2_feeds,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_rw_clash */
	{ &arcstats.l2_rw_clash,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_read_bytes */
	{ &arcstats.l2_read_bytes,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_write_bytes */
	{ &arcstats.l2_write_bytes,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_writes_sent */
	{ &arcstats.l2_writes_sent,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_writes_done */
	{ &arcstats.l2_writes_done,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_writes_error */
	{ &arcstats.l2_writes_error,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_writes_lock_retry */
	{ &arcstats.l2_writes_lock_retry,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_evict_lock_retry */
	{ &arcstats.l2_evict_lock_retry,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_evict_reading */
	{ &arcstats.l2_evict_reading,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_evict_l1cached */
	{ &arcstats.l2_evict_l1cached,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_free_on_write */
	{ &arcstats.l2_free_on_write,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_abort_lowmem */
	{ &arcstats.l2_abort_lowmem,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_cksum_bad */
	{ &arcstats.l2_cksum_bad,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_io_error */
	{ &arcstats.l2_io_error,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_size */
	{ &arcstats.l2_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_asize */
	{ &arcstats.l2_asize,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* l2_hdr_size */
	{ &arcstats.l2_hdr_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* memory_throttle_count */
	{ &arcstats.memory_throttle_count,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* memory_direct_count */
	{ &arcstats.memory_direct_count,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* memory_indirect_count */
	{ &arcstats.memory_indirect_count,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* memory_all_bytes */
	{ &arcstats.memory_all_bytes,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* memory_free_bytes */
	{ &arcstats.memory_free_bytes,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* memory_available_bytes */
	{ &arcstats.memory_available_bytes,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_no_grow */
	{ &arcstats.arc_no_grow,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_tempreserve */
	{ &arcstats.arc_tempreserve,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_loaned_bytes */
	{ &arcstats.arc_loaned_bytes,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_prune */
	{ &arcstats.arc_prune,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_meta_used */
	{ &arcstats.arc_meta_used,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_meta_limit */
	{ &arcstats.arc_meta_limit,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_dnode_limit */
	{ &arcstats.arc_dnode_limit,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_meta_max */
	{ &arcstats.arc_meta_max,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_meta_min */
	{ &arcstats.arc_meta_min,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* async_upgrade_sync */
	{ &arcstats.async_upgrade_sync,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* demand_hit_predictive_prefetch */
	{ &arcstats.demand_hit_predictive_prefetch,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* demand_hit_prescient_prefetch */
	{ &arcstats.demand_hit_prescient_prefetch,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_need_free */
	{ &arcstats.arc_need_free,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_sys_free */
	{ &arcstats.arc_sys_free,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* arc_raw_size */
	{ &arcstats.arc_raw_size,
	  { PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/*---------------------------------------------------------------------------*/
/*  ABDSTATS  */
/*---------------------------------------------------------------------------*/
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
	//zfs_zil_fetch(pmda);
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
