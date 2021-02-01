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
    uint64_t value;

    if (zfs_stats_file_check(fname, sizeof(fname), "arcstats") != 0)
        return;

    fp = fopen(fname, "r");
    if (fp != NULL) {
        while (getline(&line, &len, fp) != -1) {
            mname = strtok(line, delim);
            mval  = strtok(NULL, delim); // not used
            mval  = strtok(NULL, delim);

            if ((strcmp(mname, "name") == 0) || strtok(NULL, delim) != NULL)
                continue;

            value = strtoull(mval, NULL, 0);

            if (strncmp(mname, "l2_", 3) == 0) {
                mname += 3;
                if (strncmp(mname, "log_blk_", 8) == 0) {
                    mname += 8;
                    if (strcmp(mname, "asize") == 0) arcstats->l2_log_blk_asize = value;
                    else if (strcmp(mname, "avg_asize") == 0) arcstats->l2_log_blk_avg_asize = value;
                    else if (strcmp(mname, "count") == 0) arcstats->l2_log_blk_count = value;
                    else if (strcmp(mname, "writes") == 0) arcstats->l2_log_blk_writes = value;
                }
                else if (strcmp(mname, "write_bytes") == 0) arcstats->l2_write_bytes = value;
                else if (strncmp(mname, "writes_", 7) == 0) {
                    mname += 7;
                    if (strcmp(mname, "done") == 0) arcstats->l2_writes_done = value;
                    else if (strcmp(mname, "error") == 0) arcstats->l2_writes_error = value;
                    else if (strcmp(mname, "lock_retry") == 0) arcstats->l2_writes_lock_retry = value;
                    else if (strcmp(mname, "sent") == 0) arcstats->l2_writes_sent = value;
                }
                else if (strncmp(mname, "evict_", 6) == 0) {
                    mname += 6;
                    if (strcmp(mname, "l1cached") == 0) arcstats->l2_evict_l1cached = value;
                    else if (strcmp(mname, "lock_retry") == 0) arcstats->l2_evict_lock_retry = value;
                    else if (strcmp(mname, "reading") == 0) arcstats->l2_evict_reading = value;
                }
                else if (strncmp(mname, "rebuild_", 8) == 0) {
                    mname += 8;
                    if (strcmp(mname, "asize") == 0) arcstats->l2_rebuild_asize = value;
                    else if (strcmp(mname, "bufs") == 0) arcstats->l2_rebuild_bufs = value;
                    else if (strcmp(mname, "bufs_precached") == 0) arcstats->l2_rebuild_bufs_precached = value;
                    else if (strcmp(mname, "cksum_lb_errors") == 0) arcstats->l2_rebuild_cksum_lb_errors = value;
                    else if (strcmp(mname, "dh_errors") == 0) arcstats->l2_rebuild_dh_errors = value;
                    else if (strcmp(mname, "io_errors") == 0) arcstats->l2_rebuild_io_errors = value;
                    else if (strcmp(mname, "log_blks") == 0) arcstats->l2_rebuild_log_blks = value;
                    else if (strcmp(mname, "lowmem") == 0) arcstats->l2_rebuild_lowmem = value;
                    else if (strcmp(mname, "size") == 0) arcstats->l2_rebuild_size = value;
                    else if (strcmp(mname, "success") == 0) arcstats->l2_rebuild_success = value;
                    else if (strcmp(mname, "unsupported") == 0) arcstats->l2_rebuild_unsupported = value;
                }
                else if (strcmp(mname, "abort_lowmem") == 0) arcstats->l2_abort_lowmem = value;
                else if (strcmp(mname, "asize") == 0) arcstats->l2_asize = value;
                else if (strcmp(mname, "cksum_bad") == 0) arcstats->l2_cksum_bad = value;
                else if (strcmp(mname, "data_to_meta_ratio") == 0) arcstats->l2_data_to_meta_ratio = value;
                else if (strcmp(mname, "feeds") == 0) arcstats->l2_feeds = value;
                else if (strcmp(mname, "free_on_write") == 0) arcstats->l2_free_on_write = value;
                else if (strcmp(mname, "hdr_size") == 0) arcstats->l2_hdr_size = value;
                else if (strcmp(mname, "hits") == 0) arcstats->l2_hits = value;
                else if (strcmp(mname, "io_error") == 0) arcstats->l2_io_error = value;
                else if (strcmp(mname, "misses") == 0) arcstats->l2_misses = value;
                else if (strcmp(mname, "read_bytes") == 0) arcstats->l2_read_bytes = value;
                else if (strcmp(mname, "rw_clash") == 0) arcstats->l2_rw_clash = value;
                else if (strcmp(mname, "size") == 0) arcstats->l2_size = value;
            }
            else if (strncmp(mname, "arc_", 4) == 0) {
                mname += 4;
                if (strcmp(mname, "dnode_limit") == 0) arcstats->arc_dnode_limit = value;
                else if (strcmp(mname, "loaned_bytes") == 0) arcstats->arc_loaned_bytes = value;
                else if (strcmp(mname, "meta_limit") == 0) arcstats->arc_meta_limit = value;
                else if (strcmp(mname, "meta_max") == 0) arcstats->arc_meta_max = value;
                else if (strcmp(mname, "meta_min") == 0) arcstats->arc_meta_min = value;
                else if (strcmp(mname, "meta_used") == 0) arcstats->arc_meta_used = value;
                else if (strcmp(mname, "need_free") == 0) arcstats->arc_need_free = value;
                else if (strcmp(mname, "no_grow") == 0) arcstats->arc_no_grow = value;
                else if (strcmp(mname, "prune") == 0) arcstats->arc_prune = value;
                else if (strcmp(mname, "raw_size") == 0) arcstats->arc_raw_size = value;
                else if (strcmp(mname, "sys_free") == 0) arcstats->arc_sys_free = value;
                else if (strcmp(mname, "tempreserve") == 0) arcstats->arc_tempreserve = value;
            }
            else if (strncmp(mname, "mfu_", 4) == 0) {
                mname += 4;
                if (strcmp(mname, "evictable_data") == 0) arcstats->mfu_evictable_data = value;
                else if (strcmp(mname, "evictable_metadata") == 0) arcstats->mfu_evictable_metadata = value;
                else if (strcmp(mname, "ghost_evictable_data") == 0) arcstats->mfu_ghost_evictable_data = value;
                else if (strcmp(mname, "ghost_evictable_metadata") == 0) arcstats->mfu_ghost_evictable_metadata = value;
                else if (strcmp(mname, "ghost_hits") == 0) arcstats->mfu_ghost_hits = value;
                else if (strcmp(mname, "ghost_size") == 0) arcstats->mfu_ghost_size = value;
                else if (strcmp(mname, "hits") == 0) arcstats->mfu_hits = value;
                else if (strcmp(mname, "size") == 0) arcstats->mfu_size = value;
            }
            else if (strncmp(mname, "mru_", 4) == 0) {
                mname += 4;
                if (strcmp(mname, "evictable_data") == 0) arcstats->mru_evictable_data = value;
                else if (strcmp(mname, "evictable_metadata") == 0) arcstats->mru_evictable_metadata = value;
                else if (strcmp(mname, "ghost_evictable_data") == 0) arcstats->mru_ghost_evictable_data = value;
                else if (strcmp(mname, "ghost_evictable_metadata") == 0) arcstats->mru_ghost_evictable_metadata = value;
                else if (strcmp(mname, "ghost_hits") == 0) arcstats->mru_ghost_hits = value;
                else if (strcmp(mname, "ghost_size") == 0) arcstats->mru_ghost_size = value;
                else if (strcmp(mname, "hits") == 0) arcstats->mru_hits = value;
                else if (strcmp(mname, "size") == 0) arcstats->mru_size = value;
            }
            else if (strncmp(mname, "anon_", 5) == 0) {
                mname += 5;
                if (strcmp(mname, "evictable_data") == 0) arcstats->anon_evictable_data = value;
                else if (strcmp(mname, "evictable_metadata") == 0) arcstats->anon_evictable_metadata = value;
                else if (strcmp(mname, "size") == 0) arcstats->anon_size = value;
            }
            else if (strncmp(mname, "hash_", 5) == 0) {
                mname += 5;
                if (strcmp(mname, "chain_max") == 0) arcstats->hash_chain_max = value;
                else if (strcmp(mname, "chains") == 0) arcstats->hash_chains = value;
                else if (strcmp(mname, "collisions") == 0) arcstats->hash_collisions = value;
                else if (strcmp(mname, "elements") == 0) arcstats->hash_elements = value;
                else if (strcmp(mname, "elements_max") == 0) arcstats->hash_elements_max = value;
            }
            else if (strncmp(mname, "evict_", 6) == 0) {
                mname += 6;
                if (strcmp(mname, "l2_cached") == 0) arcstats->evict_l2_cached = value;
                else if (strcmp(mname, "l2_eligible") == 0) arcstats->evict_l2_eligible = value;
                else if (strcmp(mname, "l2_ineligible") == 0) arcstats->evict_l2_ineligible = value;
                else if (strcmp(mname, "l2_skip") == 0) arcstats->evict_l2_skip = value;
                else if (strcmp(mname, "not_enough") == 0) arcstats->evict_not_enough = value;
                else if (strcmp(mname, "skip") == 0) arcstats->evict_skip = value;
            }
            else if (strncmp(mname, "demand_", 7) == 0) {
                mname += 7;
                if (strcmp(mname, "data_hits") == 0) arcstats->demand_data_hits = value;
                else if (strcmp(mname, "data_misses") == 0) arcstats->demand_data_misses = value;
                else if (strcmp(mname, "hit_predictive_prefetch") == 0) arcstats->demand_hit_predictive_prefetch = value;
                else if (strcmp(mname, "hit_prescient_prefetch") == 0) arcstats->demand_hit_prescient_prefetch = value;
                else if (strcmp(mname, "metadata_hits") == 0) arcstats->demand_metadata_hits = value;
                else if (strcmp(mname, "metadata_misses") == 0) arcstats->demand_metadata_misses = value;
            }
            else if (strncmp(mname, "memory_", 7) == 0) {
                mname += 7;
                if (strcmp(mname, "all_bytes") == 0) arcstats->memory_all_bytes = value;
                else if (strcmp(mname, "available_bytes") == 0) arcstats->memory_available_bytes = value;
                else if (strcmp(mname, "direct_count") == 0) arcstats->memory_direct_count = value;
                else if (strcmp(mname, "free_bytes") == 0) arcstats->memory_free_bytes = value;
                else if (strcmp(mname, "indirect_count") == 0) arcstats->memory_indirect_count = value;
                else if (strcmp(mname, "throttle_count") == 0) arcstats->memory_throttle_count = value;
            }
            else if (strncmp(mname, "prefetch_", 9) == 0) {
                mname += 9;
                if (strcmp(mname, "data_hits") == 0) arcstats->prefetch_data_hits = value;
                else if (strcmp(mname, "data_misses") == 0) arcstats->prefetch_data_misses = value;
                else if (strcmp(mname, "metadata_hits") == 0) arcstats->prefetch_metadata_hits = value;
                else if (strcmp(mname, "metadata_misses") == 0) arcstats->prefetch_metadata_misses = value;
            }
            else if (strcmp(mname, "hits") == 0) arcstats->hits = value;
            else if (strcmp(mname, "misses") == 0) arcstats->misses = value;
            else if (strcmp(mname, "abd_chunk_waste_size") == 0) arcstats->abd_chunk_waste_size = value;
            else if (strcmp(mname, "access_skip") == 0) arcstats->access_skip = value;
            else if (strcmp(mname, "async_upgrade_sync") == 0) arcstats->async_upgrade_sync = value;
            else if (strcmp(mname, "bonus_size") == 0) arcstats->bonus_size = value;
            else if (strcmp(mname, "c") == 0) arcstats->c = value;
            else if (strcmp(mname, "c_max") == 0) arcstats->c_max = value;
            else if (strcmp(mname, "c_min") == 0) arcstats->c_min = value;
            else if (strcmp(mname, "cached_only_in_progress") == 0) arcstats->cached_only_in_progress = value;
            else if (strcmp(mname, "compressed_size") == 0) arcstats->compressed_size = value;
            else if (strcmp(mname, "data_size") == 0) arcstats->data_size = value;
            else if (strcmp(mname, "dbuf_size") == 0) arcstats->dbuf_size = value;
            else if (strcmp(mname, "deleted") == 0) arcstats->deleted = value;
            else if (strcmp(mname, "dnode_size") == 0) arcstats->dnode_size = value;
            else if (strcmp(mname, "hdr_size") == 0) arcstats->hdr_size = value;
            else if (strcmp(mname, "metadata_size") == 0) arcstats->metadata_size = value;
            else if (strcmp(mname, "mutex_miss") == 0) arcstats->mutex_miss = value;
            else if (strcmp(mname, "overhead_size") == 0) arcstats->overhead_size = value;
            else if (strcmp(mname, "p") == 0) arcstats->p = value;
            else if (strcmp(mname, "size") == 0) arcstats->size = value;
            else if (strcmp(mname, "uncompressed_size") == 0) arcstats->uncompressed_size = value;
        }
        free(line);
        fclose(fp);
    }
}
