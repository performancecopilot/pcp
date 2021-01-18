/*
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

typedef struct zfs_arcstats {
    uint64_t hits;
    uint64_t misses;
    uint64_t demand_data_hits;
    uint64_t demand_data_misses;
    uint64_t demand_metadata_hits;
    uint64_t demand_metadata_misses;
    uint64_t prefetch_data_hits;
    uint64_t prefetch_data_misses;
    uint64_t prefetch_metadata_hits;
    uint64_t prefetch_metadata_misses;
    uint64_t mru_hits;
    uint64_t mru_ghost_hits;
    uint64_t mfu_hits;
    uint64_t mfu_ghost_hits;
    uint64_t deleted;
    uint64_t mutex_miss;
    uint64_t access_skip;
    uint64_t evict_skip;
    uint64_t evict_not_enough;
    uint64_t evict_l2_cached;
    uint64_t evict_l2_eligible;
    uint64_t evict_l2_ineligible;
    uint64_t evict_l2_skip;
    uint64_t hash_elements;
    uint64_t hash_elements_max;
    uint64_t hash_collisions;
    uint64_t hash_chains;
    uint64_t hash_chain_max;
    uint64_t p;
    uint64_t c;
    uint64_t c_min;
    uint64_t c_max;
    uint64_t size;
    uint64_t compressed_size;
    uint64_t uncompressed_size;
    uint64_t overhead_size;
    uint64_t hdr_size;
    uint64_t data_size;
    uint64_t metadata_size;
    uint64_t dbuf_size;
    uint64_t dnode_size;
    uint64_t bonus_size;
    uint64_t anon_size;
    uint64_t anon_evictable_data;
    uint64_t anon_evictable_metadata;
    uint64_t mru_size;
    uint64_t mru_evictable_data;
    uint64_t mru_evictable_metadata;
    uint64_t mru_ghost_size;
    uint64_t mru_ghost_evictable_data;
    uint64_t mru_ghost_evictable_metadata;
    uint64_t mfu_size;
    uint64_t mfu_evictable_data;
    uint64_t mfu_evictable_metadata;
    uint64_t mfu_ghost_size;
    uint64_t mfu_ghost_evictable_data;
    uint64_t mfu_ghost_evictable_metadata;
    uint64_t l2_hits;
    uint64_t l2_misses;
    uint64_t l2_feeds;
    uint64_t l2_rw_clash;
    uint64_t l2_read_bytes;
    uint64_t l2_write_bytes;
    uint64_t l2_writes_sent;
    uint64_t l2_writes_done;
    uint64_t l2_writes_error;
    uint64_t l2_writes_lock_retry;
    uint64_t l2_evict_lock_retry;
    uint64_t l2_evict_reading;
    uint64_t l2_evict_l1cached;
    uint64_t l2_free_on_write;
    uint64_t l2_abort_lowmem;
    uint64_t l2_cksum_bad;
    uint64_t l2_io_error;
    uint64_t l2_size;
    uint64_t l2_asize;
    uint64_t l2_hdr_size;
    uint64_t memory_throttle_count;
    uint64_t memory_direct_count;
    uint64_t memory_indirect_count;
    uint64_t memory_all_bytes;
    uint64_t memory_free_bytes;
    uint64_t memory_available_bytes;
    uint64_t arc_no_grow;
    uint64_t arc_tempreserve;
    uint64_t arc_loaned_bytes;
    uint64_t arc_prune;
    uint64_t arc_meta_used;
    uint64_t arc_meta_limit;
    uint64_t arc_dnode_limit;
    uint64_t arc_meta_max;
    uint64_t arc_meta_min;
    uint64_t async_upgrade_sync;
    uint64_t demand_hit_predictive_prefetch;
    uint64_t demand_hit_prescient_prefetch;
    uint64_t arc_need_free;
    uint64_t arc_sys_free;
    uint64_t arc_raw_size;
    /*--
      Metrics introduced in OpenZFS v. 2
    --*/
    uint64_t cached_only_in_progress;
    uint64_t abd_chunk_waste_size;
    uint64_t l2_log_blk_writes;
    uint64_t l2_log_blk_avg_asize;
    uint64_t l2_log_blk_asize;
    uint64_t l2_log_blk_count;
    uint64_t l2_data_to_meta_ratio;
    uint64_t l2_rebuild_success;
    uint64_t l2_rebuild_unsupported;
    uint64_t l2_rebuild_io_errors;
    uint64_t l2_rebuild_dh_errors;
    uint64_t l2_rebuild_cksum_lb_errors;
    uint64_t l2_rebuild_lowmem;
    uint64_t l2_rebuild_size;
    uint64_t l2_rebuild_asize;
    uint64_t l2_rebuild_bufs;
    uint64_t l2_rebuild_bufs_precached;
    uint64_t l2_rebuild_log_blks;
} zfs_arcstats_t;

void zfs_arcstats_refresh(zfs_arcstats_t *arcstats);
