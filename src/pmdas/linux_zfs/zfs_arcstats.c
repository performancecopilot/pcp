#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>

#include "zfs_arcstats.h"

void
zfs_arcstats_refresh(zfs_arcstats_t *arcstats, regex_t *rgx_row)
{
        int len_mn, len_mv, nmatch = 3;
        regmatch_t pmatch[3];
        char *line, *mname, *mval;
	char *fname = "/proc/spl/kstat/zfs/arcstats";
	FILE *fp;
        size_t len = 0;

        fp = fopen(fname, "r");
	if (fp != NULL) {
		while (getline(&line, &len, fp) != -1) {
                        if (regexec(rgx_row, line, nmatch, pmatch, 0) == 0) {
                                len_mn = pmatch[1].rm_eo - pmatch[1].rm_so + 1;
                                len_mv = pmatch[2].rm_eo - pmatch[2].rm_so + 1;
                                mname = (char *) malloc((size_t) (len_mn + 1) * sizeof(char));
                                mval  = (char *) malloc((size_t) (len_mv + 1) * sizeof(char));
                                strncpy(mname, line + pmatch[1].rm_so, len_mn);
                                strncpy(mval,  line + pmatch[2].rm_so, len_mv);
                                mname[len_mn] = '\0';
                                mval[len_mv] = '\0';

				if (strcmp(mname, "hits")) arcstats->hits = atoi(mval);
				else if (strcmp(mname, "misses")) arcstats->misses = atoi(mval);
				else if (strcmp(mname, "demand_data_hits")) arcstats->demand_data_hits = atoi(mval);
				else if (strcmp(mname, "demand_data_misses")) arcstats->demand_data_misses = atoi(mval);
				else if (strcmp(mname, "demand_metadata_hits")) arcstats->demand_metadata_hits = atoi(mval);
				else if (strcmp(mname, "demand_metadata_misses")) arcstats->demand_metadata_misses = atoi(mval);
				else if (strcmp(mname, "prefetch_data_hits")) arcstats->prefetch_data_hits = atoi(mval);
				else if (strcmp(mname, "prefetch_data_misses")) arcstats->prefetch_data_misses = atoi(mval);
				else if (strcmp(mname, "prefetch_metadata_hits")) arcstats->prefetch_metadata_hits = atoi(mval);
				else if (strcmp(mname, "prefetch_metadata_misses")) arcstats->prefetch_metadata_misses = atoi(mval);
				else if (strcmp(mname, "mru_hits")) arcstats->mru_hits = atoi(mval);
				else if (strcmp(mname, "mru_ghost_hits")) arcstats->mru_ghost_hits = atoi(mval);
				else if (strcmp(mname, "mfu_hits")) arcstats->mfu_hits = atoi(mval);
				else if (strcmp(mname, "mfu_ghost_hits")) arcstats->mfu_ghost_hits = atoi(mval);
				else if (strcmp(mname, "deleted")) arcstats->deleted = atoi(mval);
				else if (strcmp(mname, "mutex_miss")) arcstats->mutex_miss = atoi(mval);
				else if (strcmp(mname, "access_skip")) arcstats->access_skip = atoi(mval);
				else if (strcmp(mname, "evict_skip")) arcstats->evict_skip = atoi(mval);
				else if (strcmp(mname, "evict_not_enough")) arcstats->evict_not_enough = atoi(mval);
				else if (strcmp(mname, "evict_l2_cached")) arcstats->evict_l2_cached = atoi(mval);
				else if (strcmp(mname, "evict_l2_eligible")) arcstats->evict_l2_eligible = atoi(mval);
				else if (strcmp(mname, "evict_l2_ineligible")) arcstats->evict_l2_ineligible = atoi(mval);
				else if (strcmp(mname, "evict_l2_skip")) arcstats->evict_l2_skip = atoi(mval);
				else if (strcmp(mname, "hash_elements")) arcstats->hash_elements = atoi(mval);
				else if (strcmp(mname, "hash_elements_max")) arcstats->hash_elements_max = atoi(mval);
				else if (strcmp(mname, "hash_collisions")) arcstats->hash_collisions = atoi(mval);
				else if (strcmp(mname, "hash_chains")) arcstats->hash_chains = atoi(mval);
				else if (strcmp(mname, "hash_chain_max")) arcstats->hash_chain_max = atoi(mval);
				else if (strcmp(mname, "p")) arcstats->p = atoi(mval);
				else if (strcmp(mname, "c")) arcstats->c = atoi(mval);
				else if (strcmp(mname, "c_min")) arcstats->c_min = atoi(mval);
				else if (strcmp(mname, "c_max")) arcstats->c_max = atoi(mval);
				else if (strcmp(mname, "size")) arcstats->size = atoi(mval);
				else if (strcmp(mname, "compressed_size")) arcstats->compressed_size = atoi(mval);
				else if (strcmp(mname, "uncompressed_size")) arcstats->uncompressed_size = atoi(mval);
				else if (strcmp(mname, "overhead_size")) arcstats->overhead_size = atoi(mval);
				else if (strcmp(mname, "hdr_size")) arcstats->hdr_size = atoi(mval);
				else if (strcmp(mname, "data_size")) arcstats->data_size = atoi(mval);
				else if (strcmp(mname, "metadata_size")) arcstats->metadata_size = atoi(mval);
				else if (strcmp(mname, "dbuf_size")) arcstats->dbuf_size = atoi(mval);
				else if (strcmp(mname, "dnode_size")) arcstats->dnode_size = atoi(mval);
				else if (strcmp(mname, "bonus_size")) arcstats->bonus_size = atoi(mval);
				else if (strcmp(mname, "anon_size")) arcstats->anon_size = atoi(mval);
				else if (strcmp(mname, "anon_evictable_data")) arcstats->anon_evictable_data = atoi(mval);
				else if (strcmp(mname, "anon_evictable_metadata")) arcstats->anon_evictable_metadata = atoi(mval);
				else if (strcmp(mname, "mru_size")) arcstats->mru_size = atoi(mval);
				else if (strcmp(mname, "mru_evictable_data")) arcstats->mru_evictable_data = atoi(mval);
				else if (strcmp(mname, "mru_evictable_metadata")) arcstats->mru_evictable_metadata = atoi(mval);
				else if (strcmp(mname, "mru_ghost_size")) arcstats->mru_ghost_size = atoi(mval);
				else if (strcmp(mname, "mru_ghost_evictable_data")) arcstats->mru_ghost_evictable_data = atoi(mval);
				else if (strcmp(mname, "mru_ghost_evictable_metadata")) arcstats->mru_ghost_evictable_metadata = atoi(mval);
				else if (strcmp(mname, "mfu_size")) arcstats->mfu_size = atoi(mval);
				else if (strcmp(mname, "mfu_evictable_data")) arcstats->mfu_evictable_data = atoi(mval);
				else if (strcmp(mname, "mfu_evictable_metadata")) arcstats->mfu_evictable_metadata = atoi(mval);
				else if (strcmp(mname, "mfu_ghost_size")) arcstats->mfu_ghost_size = atoi(mval);
				else if (strcmp(mname, "mfu_ghost_evictable_data")) arcstats->mfu_ghost_evictable_data = atoi(mval);
				else if (strcmp(mname, "mfu_ghost_evictable_metadata")) arcstats->mfu_ghost_evictable_metadata = atoi(mval);
				else if (strcmp(mname, "l2_hits")) arcstats->l2_hits = atoi(mval);
				else if (strcmp(mname, "l2_misses")) arcstats->l2_misses = atoi(mval);
				else if (strcmp(mname, "l2_feeds")) arcstats->l2_feeds = atoi(mval);
				else if (strcmp(mname, "l2_rw_clash")) arcstats->l2_rw_clash = atoi(mval);
				else if (strcmp(mname, "l2_read_bytes")) arcstats->l2_read_bytes = atoi(mval);
				else if (strcmp(mname, "l2_write_bytes")) arcstats->l2_write_bytes = atoi(mval);
				else if (strcmp(mname, "l2_writes_sent")) arcstats->l2_writes_sent = atoi(mval);
				else if (strcmp(mname, "l2_writes_done")) arcstats->l2_writes_done = atoi(mval);
				else if (strcmp(mname, "l2_writes_error")) arcstats->l2_writes_error = atoi(mval);
				else if (strcmp(mname, "l2_writes_lock_retry")) arcstats->l2_writes_lock_retry = atoi(mval);
				else if (strcmp(mname, "l2_evict_lock_retry")) arcstats->l2_evict_lock_retry = atoi(mval);
				else if (strcmp(mname, "l2_evict_reading")) arcstats->l2_evict_reading = atoi(mval);
				else if (strcmp(mname, "l2_evict_l1cached")) arcstats->l2_evict_l1cached = atoi(mval);
				else if (strcmp(mname, "l2_free_on_write")) arcstats->l2_free_on_write = atoi(mval);
				else if (strcmp(mname, "l2_abort_lowmem")) arcstats->l2_abort_lowmem = atoi(mval);
				else if (strcmp(mname, "l2_cksum_bad")) arcstats->l2_cksum_bad = atoi(mval);
				else if (strcmp(mname, "l2_io_error")) arcstats->l2_io_error = atoi(mval);
				else if (strcmp(mname, "l2_size")) arcstats->l2_size = atoi(mval);
				else if (strcmp(mname, "l2_asize")) arcstats->l2_asize = atoi(mval);
				else if (strcmp(mname, "l2_hdr_size")) arcstats->l2_hdr_size = atoi(mval);
				else if (strcmp(mname, "memory_throttle_count")) arcstats->memory_throttle_count = atoi(mval);
				else if (strcmp(mname, "memory_direct_count")) arcstats->memory_direct_count = atoi(mval);
				else if (strcmp(mname, "memory_indirect_count")) arcstats->memory_indirect_count = atoi(mval);
				else if (strcmp(mname, "memory_all_bytes")) arcstats->memory_all_bytes = atoi(mval);
				else if (strcmp(mname, "memory_free_bytes")) arcstats->memory_free_bytes = atoi(mval);
				else if (strcmp(mname, "memory_available_bytes")) arcstats->memory_available_bytes = atoi(mval);
				else if (strcmp(mname, "arc_no_grow")) arcstats->arc_no_grow = atoi(mval);
				else if (strcmp(mname, "arc_tempreserve")) arcstats->arc_tempreserve = atoi(mval);
				else if (strcmp(mname, "arc_loaned_bytes")) arcstats->arc_loaned_bytes = atoi(mval);
				else if (strcmp(mname, "arc_prune")) arcstats->arc_prune = atoi(mval);
				else if (strcmp(mname, "arc_meta_used")) arcstats->arc_meta_used = atoi(mval);
				else if (strcmp(mname, "arc_meta_limit")) arcstats->arc_meta_limit = atoi(mval);
				else if (strcmp(mname, "arc_dnode_limit")) arcstats->arc_dnode_limit = atoi(mval);
				else if (strcmp(mname, "arc_meta_max")) arcstats->arc_meta_max = atoi(mval);
				else if (strcmp(mname, "arc_meta_min")) arcstats->arc_meta_min = atoi(mval);
				else if (strcmp(mname, "async_upgrade_sync")) arcstats->async_upgrade_sync = atoi(mval);
				else if (strcmp(mname, "demand_hit_predictive_prefetch")) arcstats->demand_hit_predictive_prefetch = atoi(mval);
				else if (strcmp(mname, "demand_hit_prescient_prefetch")) arcstats->demand_hit_prescient_prefetch = atoi(mval);
				else if (strcmp(mname, "arc_need_free")) arcstats->arc_need_free = atoi(mval);
				else if (strcmp(mname, "arc_sys_free")) arcstats->arc_sys_free = atoi(mval);
				else if (strcmp(mname, "arc_raw_size")) arcstats->arc_raw_size = atoi(mval);
                        }
                        free(mname);
                        free(mval);
                }
        }
        fclose(fp);
}
