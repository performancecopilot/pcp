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

struct xfs_btree1 {
    unsigned int	lookup;
    unsigned int	compare;
    unsigned int	insrec;
    unsigned int	delrec;
};

struct xfs_btree2 {
    unsigned int	lookup;
    unsigned int	compare;
    unsigned int	insrec;	/* btree.alloc_blocks.insrec */
    unsigned int	delrec;	/* btree.alloc_blocks.delrec */
    unsigned int	newroot;	/* btree.alloc_blocks.newroot */
    unsigned int	killroot;	/* btree.alloc_blocks.killroot */
    unsigned int	increment;	/* btree.alloc_blocks.increment */
    unsigned int	decrement;	/* btree.alloc_blocks.decrement */
    unsigned int	lshift;	/* btree.alloc_blocks.lshift */
    unsigned int	rshift;	/* btree.alloc_blocks.rshift */
    unsigned int	split;	/* btree.alloc_blocks.split */
    unsigned int	join;		/* btree.alloc_blocks.join */
    unsigned int	alloc;	/* btree.alloc_blocks.alloc */
    unsigned int	free;		/* btree.alloc_blocks.free */
    unsigned int	moves;	/* btree.alloc_blocks.moves */
};

struct xfs_vnodes {
    unsigned int	vn_active;
    unsigned int	vn_alloc;
    unsigned int	vn_get;
    unsigned int	vn_hold;
    unsigned int	vn_rele;
    unsigned int	vn_reclaim;
    unsigned int	vn_remove;
    unsigned int	vn_free;
};

struct xfs_quota {
    unsigned int	dqreclaims;
    unsigned int	dqreclaim_misses;
    unsigned int	dquot_dups;
    unsigned int	dqcachemisses;
    unsigned int	dqcachehits;
    unsigned int	dqwants;
    unsigned int	dquots;
    unsigned int	dquots_unused;
};

struct xfs_xpc	{
    __uint64_t		write_bytes;
    __uint64_t		read_bytes;
    __uint64_t		xstrat_bytes;
};

typedef struct sysfs_xfs {
    int			errcode;	/* error from previous refresh */
    int			uptodate;	/* values up-to-date this fetch */
    unsigned int	xs_allocx;		/* allocs.alloc_extent */
    unsigned int	xs_allocb;		/* allocs.alloc_block */
    unsigned int	xs_freex;		/* allocs.free_extent */
    unsigned int	xs_freeb;		/* allocs.free_block */

    struct xfs_btree1	xs_abt;			/* alloc_btree.* */

    unsigned int	xs_blk_mapr;		/* block_map.read_ops */
    unsigned int	xs_blk_mapw;		/* block_map.write_ops */
    unsigned int	xs_blk_unmap;		/* block_map.unmap */
    unsigned int	xs_add_exlist;		/* block_map.add_exlist */
    unsigned int	xs_del_exlist;		/* block_map.del_exlist */
    unsigned int	xs_look_exlist;		/* block_map.look_exlist */
    unsigned int	xs_cmp_exlist;		/* block_map.cmp_exlist */

    struct xfs_btree1	xs_bmbt;		/* bmap_btree.* */

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

    struct xfs_quota	xs_qm;			/* quota.* */

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

    struct xfs_btree2	xs_abtb_2;		/* btree.alloc_blocks.* */
    struct xfs_btree2	xs_abtc_2;		/* btree.alloc_contig.* */
    struct xfs_btree2	xs_bmbt_2;		/* btree.block_map.* */
    struct xfs_btree2	xs_ibt_2;		/* btree.inode.* */
    struct xfs_btree2	xs_fibt_2;		/* btree.free_inode.* */
    struct xfs_btree2	xs_rmapbt;		/* btree.reverse_map.* */
    struct xfs_btree2	xs_refcntbt;		/* btree.refcount.* */

    struct xfs_vnodes	vnodes;

    struct xfs_xpc	xpc;
} sysfs_xfs_t;

extern FILE *xfs_statsfile(const char *, const char *);
extern char *xfs_statspath;

extern int refresh_devices(pmInDom);
extern sysfs_xfs_t *refresh_device(pmInDom, int);
extern int refresh_sysfs_xfs(sysfs_xfs_t *);
