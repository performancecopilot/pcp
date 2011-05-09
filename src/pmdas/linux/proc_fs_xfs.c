/*
 * Linux /proc/fs/xfs metrics cluster
 *
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
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

#include "pmapi.h"
#include "proc_fs_xfs.h"

int
refresh_proc_fs_xfs(proc_fs_xfs_t *proc_fs_xfs)
{
    char buf[4096];
    FILE *fp;

    memset(proc_fs_xfs, 0, sizeof(proc_fs_xfs_t));

    if ((fp = fopen("/proc/fs/xfs/stat", "r")) == (FILE *)NULL) {
    	proc_fs_xfs->errcode = -oserror();
    }
    else {
    	proc_fs_xfs->errcode = 0;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
	    if (strncmp(buf, "extent_alloc ", 13) == 0)
		sscanf(buf, "extent_alloc %u %u %u %u",
		    &proc_fs_xfs->xs_allocx,
		    &proc_fs_xfs->xs_allocb,
		    &proc_fs_xfs->xs_freex,
		    &proc_fs_xfs->xs_freeb);
	    else
	    if (strncmp(buf, "abt ", 4) == 0)
		sscanf(buf, "abt %u %u %u %u",
		    &proc_fs_xfs->xs_abt_lookup,
		    &proc_fs_xfs->xs_abt_compare,
		    &proc_fs_xfs->xs_abt_insrec,
		    &proc_fs_xfs->xs_abt_delrec);
	    else
	    if (strncmp(buf, "blk_map ", 8) == 0)
		sscanf(buf, "blk_map %u %u %u %u %u %u %u", 
		    &proc_fs_xfs->xs_blk_mapr,
		    &proc_fs_xfs->xs_blk_mapw,
		    &proc_fs_xfs->xs_blk_unmap,
		    &proc_fs_xfs->xs_add_exlist,
		    &proc_fs_xfs->xs_del_exlist,
		    &proc_fs_xfs->xs_look_exlist,
		    &proc_fs_xfs->xs_cmp_exlist);
	    else
	    if (strncmp(buf, "bmbt ", 5) == 0)
		sscanf(buf, "bmbt %u %u %u %u",
		    &proc_fs_xfs->xs_bmbt_lookup,
		    &proc_fs_xfs->xs_bmbt_compare,
		    &proc_fs_xfs->xs_bmbt_insrec,
		    &proc_fs_xfs->xs_bmbt_delrec);
	    else
	    if (strncmp(buf, "dir ", 4) == 0)
		sscanf(buf, "dir %u %u %u %u",
		    &proc_fs_xfs->xs_dir_lookup,
		    &proc_fs_xfs->xs_dir_create,
		    &proc_fs_xfs->xs_dir_remove,
		    &proc_fs_xfs->xs_dir_getdents);
	    else
	    if (strncmp(buf, "trans ", 6) == 0)
		sscanf(buf, "trans %u %u %u",
		    &proc_fs_xfs->xs_trans_sync,
		    &proc_fs_xfs->xs_trans_async,
		    &proc_fs_xfs->xs_trans_empty);
	    else
	    if (strncmp(buf, "ig ", 3) == 0)
		sscanf(buf, "ig %u %u %u %u %u %u %u",
		    &proc_fs_xfs->xs_ig_attempts,
		    &proc_fs_xfs->xs_ig_found,
		    &proc_fs_xfs->xs_ig_frecycle,
		    &proc_fs_xfs->xs_ig_missed,
		    &proc_fs_xfs->xs_ig_dup,
		    &proc_fs_xfs->xs_ig_reclaims,
		    &proc_fs_xfs->xs_ig_attrchg);
	    else
	    if (strncmp(buf, "log ", 4) == 0) {
		sscanf(buf, "log %u %u %u %u %u",
		    &proc_fs_xfs->xs_log_writes,
		    &proc_fs_xfs->xs_log_blocks,
		    &proc_fs_xfs->xs_log_noiclogs,
		    &proc_fs_xfs->xs_log_force,
		    &proc_fs_xfs->xs_log_force_sleep);
	    }
	    else
	    if (strncmp(buf, "push_ail ", 9) == 0)
		sscanf(buf, "push_ail %u %u %u %u %u %u %u %u %u %u", 
		    &proc_fs_xfs->xs_try_logspace,
		    &proc_fs_xfs->xs_sleep_logspace,
		    &proc_fs_xfs->xs_push_ail,
		    &proc_fs_xfs->xs_push_ail_success,
		    &proc_fs_xfs->xs_push_ail_pushbuf,
		    &proc_fs_xfs->xs_push_ail_pinned,
		    &proc_fs_xfs->xs_push_ail_locked,
		    &proc_fs_xfs->xs_push_ail_flushing,
		    &proc_fs_xfs->xs_push_ail_restarts,
		    &proc_fs_xfs->xs_push_ail_flush);
	    else
	    if (strncmp(buf, "xstrat ", 7) == 0)
		sscanf(buf, "xstrat %u %u", 
		    &proc_fs_xfs->xs_xstrat_quick,
		    &proc_fs_xfs->xs_xstrat_split);
	    else
	    if (strncmp(buf, "rw ", 3) == 0)
		sscanf(buf, "rw %u %u",
		    &proc_fs_xfs->xs_write_calls,
		    &proc_fs_xfs->xs_read_calls);
	    else
	    if (strncmp(buf, "attr ", 5) == 0)
		sscanf(buf, "attr %u %u %u %u",
		    &proc_fs_xfs->xs_attr_get,
		    &proc_fs_xfs->xs_attr_set,
		    &proc_fs_xfs->xs_attr_remove,
		    &proc_fs_xfs->xs_attr_list);
	    else
	    if (strncmp(buf, "qm ", 3) == 0)
		sscanf(buf, "qm %u %u %u %u %u %u %u %u",
		    &proc_fs_xfs->xs_qm_dqreclaims,
		    &proc_fs_xfs->xs_qm_dqreclaim_misses,
		    &proc_fs_xfs->xs_qm_dquot_dups,
		    &proc_fs_xfs->xs_qm_dqcachemisses,
		    &proc_fs_xfs->xs_qm_dqcachehits,
		    &proc_fs_xfs->xs_qm_dqwants,
		    &proc_fs_xfs->xs_qm_dqshake_reclaims,
		    &proc_fs_xfs->xs_qm_dqinact_reclaims);
	    else
	    if (strncmp(buf, "icluster ", 9) == 0)
		sscanf(buf, "icluster %u %u %u",
		    &proc_fs_xfs->xs_iflush_count,
		    &proc_fs_xfs->xs_icluster_flushcnt,
		    &proc_fs_xfs->xs_icluster_flushinode);
	    else
	    if (strncmp(buf, "buf ", 4) == 0) {
		sscanf(buf, "buf %u %u %u %u %u %u %u %u %u",
		    &proc_fs_xfs->xs_buf_get,
		    &proc_fs_xfs->xs_buf_create,
		    &proc_fs_xfs->xs_buf_get_locked,
		    &proc_fs_xfs->xs_buf_get_locked_waited,
		    &proc_fs_xfs->xs_buf_busy_locked,
		    &proc_fs_xfs->xs_buf_miss_locked,
		    &proc_fs_xfs->xs_buf_page_retries,
		    &proc_fs_xfs->xs_buf_page_found,
		    &proc_fs_xfs->xs_buf_get_read);		    
	    } else
	    if (strncmp(buf, "vnodes ", 7) == 0) {
		sscanf(buf, "vnodes %u %u %u %u %u %u %u %u",
		    &proc_fs_xfs->vnodes.vn_active,
		    &proc_fs_xfs->vnodes.vn_alloc,
		    &proc_fs_xfs->vnodes.vn_get,
		    &proc_fs_xfs->vnodes.vn_hold,
		    &proc_fs_xfs->vnodes.vn_rele,
		    &proc_fs_xfs->vnodes.vn_reclaim,
		    &proc_fs_xfs->vnodes.vn_remove,
		    &proc_fs_xfs->vnodes.vn_free);
	    } else
	    if (strncmp(buf, "abtb2 ", 6) == 0) {
		sscanf(buf, "abtb2 %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
		    &proc_fs_xfs->xs_abtb_2_lookup,
		    &proc_fs_xfs->xs_abtb_2_compare,
		    &proc_fs_xfs->xs_abtb_2_insrec,
		    &proc_fs_xfs->xs_abtb_2_delrec,
		    &proc_fs_xfs->xs_abtb_2_newroot,
		    &proc_fs_xfs->xs_abtb_2_killroot,
		    &proc_fs_xfs->xs_abtb_2_increment,
		    &proc_fs_xfs->xs_abtb_2_decrement,
		    &proc_fs_xfs->xs_abtb_2_lshift,
		    &proc_fs_xfs->xs_abtb_2_rshift,
		    &proc_fs_xfs->xs_abtb_2_split,
		    &proc_fs_xfs->xs_abtb_2_join,
		    &proc_fs_xfs->xs_abtb_2_alloc,
		    &proc_fs_xfs->xs_abtb_2_free,
		    &proc_fs_xfs->xs_abtb_2_moves);
	    } else
	    if (strncmp(buf, "abtc2 ", 6) == 0) {
		sscanf(buf, "abtc2 %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
		    &proc_fs_xfs->xs_abtc_2_lookup,
		    &proc_fs_xfs->xs_abtc_2_compare,
		    &proc_fs_xfs->xs_abtc_2_insrec,
		    &proc_fs_xfs->xs_abtc_2_delrec,
		    &proc_fs_xfs->xs_abtc_2_newroot,
		    &proc_fs_xfs->xs_abtc_2_killroot,
		    &proc_fs_xfs->xs_abtc_2_increment,
		    &proc_fs_xfs->xs_abtc_2_decrement,
		    &proc_fs_xfs->xs_abtc_2_lshift,
		    &proc_fs_xfs->xs_abtc_2_rshift,
		    &proc_fs_xfs->xs_abtc_2_split,
		    &proc_fs_xfs->xs_abtc_2_join,
		    &proc_fs_xfs->xs_abtc_2_alloc,
		    &proc_fs_xfs->xs_abtc_2_free,
		    &proc_fs_xfs->xs_abtc_2_moves);
	    } else
	    if (strncmp(buf, "bmbt2 ", 6) == 0) {
		sscanf(buf, "bmbt2 %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
		    &proc_fs_xfs->xs_bmbt_2_lookup,
		    &proc_fs_xfs->xs_bmbt_2_compare,
		    &proc_fs_xfs->xs_bmbt_2_insrec,
		    &proc_fs_xfs->xs_bmbt_2_delrec,
		    &proc_fs_xfs->xs_bmbt_2_newroot,
		    &proc_fs_xfs->xs_bmbt_2_killroot,
		    &proc_fs_xfs->xs_bmbt_2_increment,
		    &proc_fs_xfs->xs_bmbt_2_decrement,
		    &proc_fs_xfs->xs_bmbt_2_lshift,
		    &proc_fs_xfs->xs_bmbt_2_rshift,
		    &proc_fs_xfs->xs_bmbt_2_split,
		    &proc_fs_xfs->xs_bmbt_2_join,
		    &proc_fs_xfs->xs_bmbt_2_alloc,
		    &proc_fs_xfs->xs_bmbt_2_free,
		    &proc_fs_xfs->xs_bmbt_2_moves);
	    } else
	    if (strncmp(buf, "ibt2 ", 5) == 0) {
		sscanf(buf, "ibt2 %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
		    &proc_fs_xfs->xs_ibt_2_lookup,
		    &proc_fs_xfs->xs_ibt_2_compare,
		    &proc_fs_xfs->xs_ibt_2_insrec,
		    &proc_fs_xfs->xs_ibt_2_delrec,
		    &proc_fs_xfs->xs_ibt_2_newroot,
		    &proc_fs_xfs->xs_ibt_2_killroot,
		    &proc_fs_xfs->xs_ibt_2_increment,
		    &proc_fs_xfs->xs_ibt_2_decrement,
		    &proc_fs_xfs->xs_ibt_2_lshift,
		    &proc_fs_xfs->xs_ibt_2_rshift,
		    &proc_fs_xfs->xs_ibt_2_split,
		    &proc_fs_xfs->xs_ibt_2_join,
		    &proc_fs_xfs->xs_ibt_2_alloc,
		    &proc_fs_xfs->xs_ibt_2_free,
		    &proc_fs_xfs->xs_ibt_2_moves);
	    } else
	    if (strncmp(buf, "xpc", 3) == 0)
		sscanf(buf, "xpc %llu %llu %llu",
		    (unsigned long long *)&proc_fs_xfs->xpc.xs_xstrat_bytes,
		    (unsigned long long *)&proc_fs_xfs->xpc.xs_write_bytes,
		    (unsigned long long *)&proc_fs_xfs->xpc.xs_read_bytes);
	}
	fclose(fp);

	if (proc_fs_xfs->xs_log_writes)
	    proc_fs_xfs->xs_log_write_ratio = 
		proc_fs_xfs->xs_log_blocks / proc_fs_xfs->xs_log_writes;
	/*
	 * Bug #824382. xs_log_blocks is counted in units
	 * of 512 bytes/block, but PCP exports it as Kbytes.
	 */
	proc_fs_xfs->xs_log_blocks >>= 1;

	fp = fopen("/proc/fs/xfs/xqmstat", "r");
	if (fp != (FILE *)NULL) {
	    if (fgets(buf, sizeof(buf), fp) != NULL) {
		if (strncmp(buf, "qm", 2) == 0)
		    sscanf(buf, "qm %u %u %u %u %u %u %u %u",
			&proc_fs_xfs->xs_qm_dqreclaims,
			&proc_fs_xfs->xs_qm_dqreclaim_misses,
			&proc_fs_xfs->xs_qm_dquot_dups,
			&proc_fs_xfs->xs_qm_dqcachemisses,
			&proc_fs_xfs->xs_qm_dqcachehits,
			&proc_fs_xfs->xs_qm_dqwants,
			&proc_fs_xfs->xs_qm_dqshake_reclaims,
			&proc_fs_xfs->xs_qm_dqinact_reclaims);
	    }
	    fclose(fp);
	}
    }

    if (proc_fs_xfs->errcode == 0)
	return 0;
    return -1;
}
