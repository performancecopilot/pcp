/* 
 * Linux /sys/fs/xfs metrics cluster
 *
 * Copyright (c) 2014,2016 Red Hat.
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

typedef struct sysfs_xfs {
    int			errcode;	/* error from previous refresh */
    int			uptodate;	/* values up-to-date this fetch */
    unsigned int	xs_allocx;		/* allocs.alloc_extent */
    unsigned int	xs_allocb;		/* allocs.alloc_block */
    unsigned int	xs_freex;		/* allocs.free_extent */
    unsigned int	xs_freeb;		/* allocs.free_block */

    unsigned int	xs_abt_lookup;		/* alloc_btree.lookup */
    unsigned int	xs_abt_compare;		/* alloc_btree.compare */
    unsigned int	xs_abt_insrec;		/* alloc_btree.insrec */
    unsigned int	xs_abt_delrec;		/* alloc_btree.delrec */
    unsigned int	xs_blk_mapr;		/* block_map.read_ops */
    unsigned int	xs_blk_mapw;		/* block_map.write_ops */
    unsigned int	xs_blk_unmap;		/* block_map.unmap */
    unsigned int	xs_add_exlist;		/* block_map.add_exlist */
    unsigned int	xs_del_exlist;		/* block_map.del_exlist */
    unsigned int	xs_look_exlist;		/* block_map.look_exlist */
    unsigned int	xs_cmp_exlist;		/* block_map.cmp_exlist */
    unsigned int	xs_bmbt_lookup;		/* bmap_btree.lookup */
    unsigned int	xs_bmbt_compare;	/* bmap_btree.compare */
    unsigned int	xs_bmbt_insrec;		/* bmap_btree.insrec */
    unsigned int	xs_bmbt_delrec;		/* bmap_btree.delrec */

    unsigned int	xs_dir_lookup;		/* dir_ops.lookup */
    unsigned int	xs_dir_create;		/* dir_ops.create */
    unsigned int	xs_dir_remove;		/* dir_ops.remove */
    unsigned int	xs_dir_getdents;	/* dir_ops.getdents */

    unsigned int	xs_trans_sync;		/* transactions.sync */
    unsigned int	xs_trans_async;		/* transactions.async */
    unsigned int	xs_trans_empty;		/* transactions.empty */

    unsigned int	xs_ig_attempts;		/* inode_ops.ig_attempts */
    unsigned int	xs_ig_found;		/* inode_ops.ig_found */
    unsigned int	xs_ig_frecycle;		/* inode_ops.ig_frecycle */
    unsigned int	xs_ig_missed;		/* inode_ops.ig_missed */
    unsigned int	xs_ig_dup;		/* inode_ops.ig_dup */
    unsigned int	xs_ig_reclaims;		/* inode_ops.ig_reclaims */
    unsigned int	xs_ig_attrchg;		/* inode_ops.ig_attrchg */

    unsigned int	xs_log_writes;		/* log.writes */
    unsigned int	xs_log_blocks;		/* log.blocks */
    float		xs_log_write_ratio;	/* log.write_ratio */
    unsigned int	xs_log_noiclogs;	/* log.noiclogs */

    unsigned int	xs_xstrat_quick;	/* xstrat.quick */
    unsigned int	xs_xstrat_split;	/* xstrat.split */
    unsigned int	xs_write_calls;		/* write */
    unsigned int	xs_read_calls;		/* read */

    unsigned int	xs_attr_get;		/* attr.get */
    unsigned int	xs_attr_set;		/* attr.set */
    unsigned int	xs_attr_remove;		/* attr.remove */
    unsigned int	xs_attr_list;		/* attr.list */

    unsigned int	xs_log_force;		/* log.force */
    unsigned int	xs_log_force_sleep;	/* log.force_sleep */
    unsigned int	xs_try_logspace;	/* log_tail.try_logspace */
    unsigned int	xs_sleep_logspace;	/* log_tail.sleep_logspace */
    unsigned int	xs_push_ail;		/* log_tail.push_ail.pushes */
    unsigned int	xs_push_ail_success;	/* log_tail.push_ail.success */
    unsigned int	xs_push_ail_pushbuf;	/* log_tail.push_ail.pushbuf */
    unsigned int	xs_push_ail_pinned;	/* log_tail.push_ail.pinned */
    unsigned int	xs_push_ail_locked;	/* log_tail.push_ail.locked */
    unsigned int	xs_push_ail_flushing;	/* log_tail.push_ail.flushing */
    unsigned int	xs_push_ail_restarts;	/* log_tail.push_ail.restarts */
    unsigned int	xs_push_ail_flush;	/* log_tail.push_ail.flush */

    unsigned int	xs_qm_dqreclaims;	/* quota.reclaims */
    unsigned int	xs_qm_dqreclaim_misses;	/* quota.reclaim_misses */
    unsigned int	xs_qm_dquot_dups;	/* quota.dquot_dups */
    unsigned int	xs_qm_dqcachemisses;	/* quota.cachemisses */
    unsigned int	xs_qm_dqcachehits;	/* quota.cachehits */
    unsigned int	xs_qm_dqwants;		/* quota.wants */
    unsigned int	xs_qm_dqshake_reclaims;	/* quota.shake_reclaims */
    unsigned int	xs_qm_dqinact_reclaims;	/* quota.inact_reclaims */

    unsigned int	xs_iflush_count;	/* iflush_count */
    unsigned int	xs_icluster_flushcnt;	/* icluster_flushcnt */
    unsigned int	xs_icluster_flushinode;	/* icluster_flushinode */

    unsigned int	xs_buf_get;		/* buffer.get */
    unsigned int	xs_buf_create;		/* buffer.create */
    unsigned int	xs_buf_get_locked;	/* buffer.get_locked */
    unsigned int	xs_buf_get_locked_waited; /* buffer.get_locked_waited */
    unsigned int	xs_buf_busy_locked;	/* buffer.busy_locked */
    unsigned int	xs_buf_miss_locked;	/* buffer.miss_locked */
    unsigned int	xs_buf_page_retries;	/* buffer.page_retries */
    unsigned int	xs_buf_page_found;	/* buffer.page_found */
    unsigned int	xs_buf_get_read;	/* buffer.get_read */

    unsigned int	xs_abtb_2_lookup;	/* btree.alloc_blocks.lookup */
    unsigned int	xs_abtb_2_compare;	/* btree.alloc_blocks.compare */
    unsigned int	xs_abtb_2_insrec;	/* btree.alloc_blocks.insrec */
    unsigned int	xs_abtb_2_delrec;	/* btree.alloc_blocks.delrec */
    unsigned int	xs_abtb_2_newroot;	/* btree.alloc_blocks.newroot */
    unsigned int	xs_abtb_2_killroot;	/* btree.alloc_blocks.killroot */
    unsigned int	xs_abtb_2_increment;	/* btree.alloc_blocks.increment */
    unsigned int	xs_abtb_2_decrement;	/* btree.alloc_blocks.decrement */
    unsigned int	xs_abtb_2_lshift;	/* btree.alloc_blocks.lshift */
    unsigned int	xs_abtb_2_rshift;	/* btree.alloc_blocks.rshift */
    unsigned int	xs_abtb_2_split;	/* btree.alloc_blocks.split */
    unsigned int	xs_abtb_2_join;		/* btree.alloc_blocks.join */
    unsigned int	xs_abtb_2_alloc;	/* btree.alloc_blocks.alloc */
    unsigned int	xs_abtb_2_free;		/* btree.alloc_blocks.free */
    unsigned int	xs_abtb_2_moves;	/* btree.alloc_blocks.moves */
    unsigned int	xs_abtc_2_lookup;	/* btree.alloc_contig.lookup */
    unsigned int	xs_abtc_2_compare;	/* btree.alloc_contig.compare */
    unsigned int	xs_abtc_2_insrec;	/* btree.alloc_contig.insrec */
    unsigned int	xs_abtc_2_delrec;	/* btree.alloc_contig.delrec */
    unsigned int	xs_abtc_2_newroot;	/* btree.alloc_contig.newroot */
    unsigned int	xs_abtc_2_killroot;	/* btree.alloc_contig.killroot */
    unsigned int	xs_abtc_2_increment;	/* btree.alloc_contig.increment */
    unsigned int	xs_abtc_2_decrement;	/* btree.alloc_contig.decrement */
    unsigned int	xs_abtc_2_lshift;	/* btree.alloc_contig.lshift */
    unsigned int	xs_abtc_2_rshift;	/* btree.alloc_contig.rshift */
    unsigned int	xs_abtc_2_split;	/* btree.alloc_contig.split */
    unsigned int	xs_abtc_2_join;		/* btree.alloc_contig.join */
    unsigned int	xs_abtc_2_alloc;	/* btree.alloc_contig.alloc */
    unsigned int	xs_abtc_2_free;		/* btree.alloc_contig.free */
    unsigned int	xs_abtc_2_moves;	/* btree.alloc_contig.moves */
    unsigned int	xs_bmbt_2_lookup;	/* btree.block_map.lookup */
    unsigned int	xs_bmbt_2_compare;	/* btree.block_map.compare */
    unsigned int	xs_bmbt_2_insrec;	/* btree.block_map.insrec */
    unsigned int	xs_bmbt_2_delrec;	/* btree.block_map.delrec */
    unsigned int	xs_bmbt_2_newroot;	/* btree.block_map.newroot */
    unsigned int	xs_bmbt_2_killroot;	/* btree.block_map.killroot */
    unsigned int	xs_bmbt_2_increment;	/* btree.block_map.increment */
    unsigned int	xs_bmbt_2_decrement;	/* btree.block_map.decrement */
    unsigned int	xs_bmbt_2_lshift;	/* btree.block_map.lshift */
    unsigned int	xs_bmbt_2_rshift;	/* btree.block_map.rshift */
    unsigned int	xs_bmbt_2_split;	/* btree.block_map.split */
    unsigned int	xs_bmbt_2_join;		/* btree.block_map.join */
    unsigned int	xs_bmbt_2_alloc;	/* btree.block_map.alloc */
    unsigned int	xs_bmbt_2_free;		/* btree.block_map.free */
    unsigned int	xs_bmbt_2_moves;	/* btree.block_map.moves */
    unsigned int	xs_ibt_2_lookup;	/* btree.inode.lookup */
    unsigned int	xs_ibt_2_compare;	/* btree.inode.compare */
    unsigned int	xs_ibt_2_insrec;	/* btree.inode.insrec */
    unsigned int	xs_ibt_2_delrec;	/* btree.inode.delrec */
    unsigned int	xs_ibt_2_newroot;	/* btree.inode.newroot */
    unsigned int	xs_ibt_2_killroot;	/* btree.inode.killroot */
    unsigned int	xs_ibt_2_increment;	/* btree.inode.increment */
    unsigned int	xs_ibt_2_decrement;	/* btree.inode.decrement */
    unsigned int	xs_ibt_2_lshift;	/* btree.inode.lshift */
    unsigned int	xs_ibt_2_rshift;	/* btree.inode.rshift */
    unsigned int	xs_ibt_2_split;		/* btree.inode.split */
    unsigned int	xs_ibt_2_join;		/* btree.inode.join */
    unsigned int	xs_ibt_2_alloc;		/* btree.inode.alloc */
    unsigned int	xs_ibt_2_free;		/* btree.inode.free */
    unsigned int	xs_ibt_2_moves;		/* btree.inode.moves */

    struct vnodes {
	unsigned int	vn_active;		/* vnodes.active */
	unsigned int	vn_alloc;		/* vnodes.alloc */
	unsigned int	vn_get;			/* vnodes.get */
	unsigned int	vn_hold;		/* vnodes.hold */
	unsigned int	vn_rele;		/* vnodes.rele */
	unsigned int	vn_reclaim;		/* vnodes.reclaim */
	unsigned int	vn_remove;		/* vnodes.remove */
	unsigned int	vn_free;		/* vnodes.free */
    } vnodes;
    struct xpc {
	__uint64_t	xs_write_bytes;		/* write_bytes */
	__uint64_t	xs_read_bytes;		/* read_bytes */
	__uint64_t	xs_xstrat_bytes;	/* xstrat_bytes */
    } xpc;
} sysfs_xfs_t;

extern FILE *xfs_statsfile(const char *, const char *);
extern char *xfs_statspath;

extern int refresh_devices(pmInDom);
extern sysfs_xfs_t *refresh_device(pmInDom, int);
extern int refresh_sysfs_xfs(sysfs_xfs_t *);
