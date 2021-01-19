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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "zfs_utils.h"
#include "zfs_arcstats.h"

void
zfs_arcstats_refresh(zfs_arcstats_t *arcstats)
{
    char *line = NULL, *mname, *mval;
    char delim[] = " ";
    char fname[MAXPATHLEN];
    FILE *fp;
    size_t len = 0;

    if (zfs_stats_file_check(fname, sizeof(fname), "arcstats") != 0)
        return;

    fp = fopen(fname, "r");
    if (fp != NULL) {
        while (getline(&line, &len, fp) != -1) {
            mname = strtok(line, delim);
            mval  = strtok(NULL, delim); // not used
            mval  = strtok(NULL, delim);
            if (strncmp(mname, "l2_", 3) == 0) {
                if (strncmp(mname, "l2_log_", 7) == 0) {
                    if (strcmp(mname, "l2_log_blk_asize") == 0) arcstats->l2_log_blk_asize = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_log_blk_avg_asize") == 0) arcstats->l2_log_blk_avg_asize = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_log_blk_count") == 0) arcstats->l2_log_blk_count = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_log_blk_writes") == 0) arcstats->l2_log_blk_writes = strtoul(mval, NULL, 0);
                }
                else if (strncmp(mname, "l2_write", 8) == 0) {
                    if (strcmp(mname, "l2_write_bytes") == 0) arcstats->l2_write_bytes = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_writes_done") == 0) arcstats->l2_writes_done = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_writes_error") == 0) arcstats->l2_writes_error = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_writes_lock_retry") == 0) arcstats->l2_writes_lock_retry = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_writes_sent") == 0) arcstats->l2_writes_sent = strtoul(mval, NULL, 0);
                }
                else if (strncmp(mname, "l2_evict_", 9) == 0) {
                    if (strcmp(mname, "l2_evict_l1cached") == 0) arcstats->l2_evict_l1cached = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_evict_lock_retry") == 0) arcstats->l2_evict_lock_retry = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_evict_reading") == 0) arcstats->l2_evict_reading = strtoul(mval, NULL, 0);
                }
                else if (strncmp(mname, "l2_rebuild_", 11) == 0) {
                    if (strcmp(mname, "l2_rebuild_asize") == 0) arcstats->l2_rebuild_asize = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_rebuild_bufs") == 0) arcstats->l2_rebuild_bufs = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_rebuild_bufs_precached") == 0) arcstats->l2_rebuild_bufs_precached = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_rebuild_cksum_lb_errors") == 0) arcstats->l2_rebuild_cksum_lb_errors = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_rebuild_dh_errors") == 0) arcstats->l2_rebuild_dh_errors = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_rebuild_io_errors") == 0) arcstats->l2_rebuild_io_errors = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_rebuild_log_blks") == 0) arcstats->l2_rebuild_log_blks = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_rebuild_lowmem") == 0) arcstats->l2_rebuild_lowmem = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_rebuild_size") == 0) arcstats->l2_rebuild_size = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_rebuild_success") == 0) arcstats->l2_rebuild_success = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "l2_rebuild_unsupported") == 0) arcstats->l2_rebuild_unsupported = strtoul(mval, NULL, 0);
                }
                else if (strcmp(mname, "l2_abort_lowmem") == 0) arcstats->l2_abort_lowmem = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "l2_asize") == 0) arcstats->l2_asize = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "l2_cksum_bad") == 0) arcstats->l2_cksum_bad = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "l2_data_to_meta_ratio") == 0) arcstats->l2_data_to_meta_ratio = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "l2_feeds") == 0) arcstats->l2_feeds = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "l2_free_on_write") == 0) arcstats->l2_free_on_write = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "l2_hdr_size") == 0) arcstats->l2_hdr_size = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "l2_hits") == 0) arcstats->l2_hits = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "l2_io_error") == 0) arcstats->l2_io_error = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "l2_misses") == 0) arcstats->l2_misses = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "l2_read_bytes") == 0) arcstats->l2_read_bytes = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "l2_rw_clash") == 0) arcstats->l2_rw_clash = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "l2_size") == 0) arcstats->l2_size = strtoul(mval, NULL, 0);
            }
            else if (strncmp(mname, "arc_", 4) == 0) {
                if (strcmp(mname, "arc_dnode_limit") == 0) arcstats->arc_dnode_limit = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "arc_loaned_bytes") == 0) arcstats->arc_loaned_bytes = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "arc_meta_limit") == 0) arcstats->arc_meta_limit = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "arc_meta_max") == 0) arcstats->arc_meta_max = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "arc_meta_min") == 0) arcstats->arc_meta_min = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "arc_meta_used") == 0) arcstats->arc_meta_used = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "arc_need_free") == 0) arcstats->arc_need_free = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "arc_no_grow") == 0) arcstats->arc_no_grow = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "arc_prune") == 0) arcstats->arc_prune = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "arc_raw_size") == 0) arcstats->arc_raw_size = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "arc_sys_free") == 0) arcstats->arc_sys_free = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "arc_tempreserve") == 0) arcstats->arc_tempreserve = strtoul(mval, NULL, 0);
            }
            else if (strncmp(mname, "mfu_", 4) == 0) {
                if (strcmp(mname, "mfu_evictable_data") == 0) arcstats->mfu_evictable_data = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "mfu_evictable_metadata") == 0) arcstats->mfu_evictable_metadata = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "mfu_ghost_evictable_data") == 0) arcstats->mfu_ghost_evictable_data = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "mfu_ghost_evictable_metadata") == 0) arcstats->mfu_ghost_evictable_metadata = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "mfu_ghost_hits") == 0) arcstats->mfu_ghost_hits = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "mfu_ghost_size") == 0) arcstats->mfu_ghost_size = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "mfu_hits") == 0) arcstats->mfu_hits = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "mfu_size") == 0) arcstats->mfu_size = strtoul(mval, NULL, 0);
            }
            else if (strncmp(mname, "mru_", 4) == 0) {
                if (strcmp(mname, "mru_evictable_data") == 0) arcstats->mru_evictable_data = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "mru_evictable_metadata") == 0) arcstats->mru_evictable_metadata = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "mru_ghost_evictable_data") == 0) arcstats->mru_ghost_evictable_data = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "mru_ghost_evictable_metadata") == 0) arcstats->mru_ghost_evictable_metadata = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "mru_ghost_hits") == 0) arcstats->mru_ghost_hits = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "mru_ghost_size") == 0) arcstats->mru_ghost_size = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "mru_hits") == 0) arcstats->mru_hits = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "mru_size") == 0) arcstats->mru_size = strtoul(mval, NULL, 0);
            }
            else if (strncmp(mname, "anon_", 5) == 0) {
                if (strcmp(mname, "anon_evictable_data") == 0) arcstats->anon_evictable_data = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "anon_evictable_metadata") == 0) arcstats->anon_evictable_metadata = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "anon_size") == 0) arcstats->anon_size = strtoul(mval, NULL, 0);
            }
            else if (strncmp(mname, "hash_", 5) == 0) {
                if (strcmp(mname, "hash_chain_max") == 0) arcstats->hash_chain_max = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "hash_chains") == 0) arcstats->hash_chains = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "hash_collisions") == 0) arcstats->hash_collisions = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "hash_elements") == 0) arcstats->hash_elements = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "hash_elements_max") == 0) arcstats->hash_elements_max = strtoul(mval, NULL, 0);
            }
            else if (strncmp(mname, "evict_", 6) == 0) {
                if (strcmp(mname, "evict_l2_cached") == 0) arcstats->evict_l2_cached = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "evict_l2_eligible") == 0) arcstats->evict_l2_eligible = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "evict_l2_ineligible") == 0) arcstats->evict_l2_ineligible = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "evict_l2_skip") == 0) arcstats->evict_l2_skip = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "evict_not_enough") == 0) arcstats->evict_not_enough = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "evict_skip") == 0) arcstats->evict_skip = strtoul(mval, NULL, 0);
            }
            else if (strncmp(mname, "demand_", 7) == 0) {
                if (strcmp(mname, "demand_data_hits") == 0) arcstats->demand_data_hits = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "demand_data_misses") == 0) arcstats->demand_data_misses = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "demand_hit_predictive_prefetch") == 0) arcstats->demand_hit_predictive_prefetch = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "demand_hit_prescient_prefetch") == 0) arcstats->demand_hit_prescient_prefetch = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "demand_metadata_hits") == 0) arcstats->demand_metadata_hits = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "demand_metadata_misses") == 0) arcstats->demand_metadata_misses = strtoul(mval, NULL, 0);
            }
            else if (strncmp(mname, "memory_", 7) == 0) {
                if (strcmp(mname, "memory_all_bytes") == 0) arcstats->memory_all_bytes = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "memory_available_bytes") == 0) arcstats->memory_available_bytes = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "memory_direct_count") == 0) arcstats->memory_direct_count = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "memory_free_bytes") == 0) arcstats->memory_free_bytes = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "memory_indirect_count") == 0) arcstats->memory_indirect_count = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "memory_throttle_count") == 0) arcstats->memory_throttle_count = strtoul(mval, NULL, 0);
            }
            else if (strncmp(mname, "prefetch_", 9) == 0) {
                if (strcmp(mname, "prefetch_data_hits") == 0) arcstats->prefetch_data_hits = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "prefetch_data_misses") == 0) arcstats->prefetch_data_misses = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "prefetch_metadata_hits") == 0) arcstats->prefetch_metadata_hits = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "prefetch_metadata_misses") == 0) arcstats->prefetch_metadata_misses = strtoul(mval, NULL, 0);
            }
            else if (strcmp(mname, "hits") == 0) arcstats->hits = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "misses") == 0) arcstats->misses = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "abd_chunk_waste_size") == 0) arcstats->abd_chunk_waste_size = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "access_skip") == 0) arcstats->access_skip = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "async_upgrade_sync") == 0) arcstats->async_upgrade_sync = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "bonus_size") == 0) arcstats->bonus_size = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "c") == 0) arcstats->c = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "c_max") == 0) arcstats->c_max = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "c_min") == 0) arcstats->c_min = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "cached_only_in_progress") == 0) arcstats->cached_only_in_progress = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "compressed_size") == 0) arcstats->compressed_size = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "data_size") == 0) arcstats->data_size = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dbuf_size") == 0) arcstats->dbuf_size = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "deleted") == 0) arcstats->deleted = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dnode_size") == 0) arcstats->dnode_size = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "hdr_size") == 0) arcstats->hdr_size = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "metadata_size") == 0) arcstats->metadata_size = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "mutex_miss") == 0) arcstats->mutex_miss = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "overhead_size") == 0) arcstats->overhead_size = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "p") == 0) arcstats->p = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "size") == 0) arcstats->size = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "uncompressed_size") == 0) arcstats->uncompressed_size = strtoul(mval, NULL, 0);
        }
        free(line);
    }
    fclose(fp);
}
