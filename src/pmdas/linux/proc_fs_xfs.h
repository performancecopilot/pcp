/* 
 * Linux /proc/fs/xfs metrics cluster
 *
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: proc_fs_xfs.h,v 1.4 2004/06/24 06:15:36 kenmcd Exp $"

typedef struct {
    int			errcode;	/* error from previous refresh */
    unsigned int	xs_allocx;		/* xfs.allocs.alloc_extent */
    unsigned int	xs_allocb;		/* xfs.allocs.alloc_block */
    unsigned int	xs_freex;		/* xfs.allocs.free_extent */
    unsigned int	xs_freeb;		/* xfs.allocs.free_block */
    unsigned int	xs_abt_lookup;		/* xfs.alloc_btree.lookup */
    unsigned int	xs_abt_compare;		/* xfs.alloc_btree.compare */
    unsigned int	xs_abt_insrec;		/* xfs.alloc_btree.insrec */
    unsigned int	xs_abt_delrec;		/* xfs.alloc_btree.delrec */
    unsigned int	xs_blk_mapr;		/* xfs.block_map.read_ops */
    unsigned int	xs_blk_mapw;		/* xfs.block_map.write_ops */
    unsigned int	xs_blk_unmap;		/* xfs.block_map.unmap */
    unsigned int	xs_add_exlist;		/* xfs.block_map.add_exlist */
    unsigned int	xs_del_exlist;		/* xfs.block_map.del_exlist */
    unsigned int	xs_look_exlist;		/* xfs.block_map.look_exlist */
    unsigned int	xs_cmp_exlist;		/* xfs.block_map.cmp_exlist */
    unsigned int	xs_bmbt_lookup;		/* xfs.bmap_btree.lookup */
    unsigned int	xs_bmbt_compare;	/* xfs.bmap_btree.compare */
    unsigned int	xs_bmbt_insrec;		/* xfs.bmap_btree.insrec */
    unsigned int	xs_bmbt_delrec;		/* xfs.bmap_btree.delrec */
    unsigned int	xs_dir_lookup;		/* xfs.dir_ops.lookup */
    unsigned int	xs_dir_create;		/* xfs.dir_ops.create */
    unsigned int	xs_dir_remove;		/* xfs.dir_ops.remove */
    unsigned int	xs_dir_getdents;	/* xfs.dir_ops.getdents */
    unsigned int	xs_trans_sync;		/* xfs.transactions.sync */
    unsigned int	xs_trans_async;		/* xfs.transactions.async */
    unsigned int	xs_trans_empty;		/* xfs.transactions.empty */
    unsigned int	xs_ig_attempts;		/* xfs.inode_ops.ig_attempts */
    unsigned int	xs_ig_found;		/* xfs.inode_ops.ig_found */
    unsigned int	xs_ig_frecycle;		/* xfs.inode_ops.ig_frecycle */
    unsigned int	xs_ig_missed;		/* xfs.inode_ops.ig_missed */
    unsigned int	xs_ig_dup;		/* xfs.inode_ops.ig_dup */
    unsigned int	xs_ig_reclaims;		/* xfs.inode_ops.ig_reclaims */
    unsigned int	xs_ig_attrchg;		/* xfs.inode_ops.ig_attrchg */
    unsigned int	xs_log_writes;		/* xfs.log.writes */
    unsigned int	xs_log_blocks;		/* xfs.log.blocks */
    float		xs_log_write_ratio;	/* xfs.log.write_ratio */
    unsigned int	xs_log_noiclogs;	/* xfs.log.noiclogs */
    unsigned int	xs_xstrat_quick;	/* xfs.xstrat.quick */
    unsigned int	xs_xstrat_split;	/* xfs.xstrat.split */
    unsigned int	xs_write_calls;		/* xfs.write */
    unsigned int	xs_read_calls;		/* xfs.read */
    unsigned int	xs_attr_get;		/* xfs.attr.get */
    unsigned int	xs_attr_set;		/* xfs.attr.set */
    unsigned int	xs_attr_remove;		/* xfs.attr.remove */
    unsigned int	xs_attr_list;		/* xfs.attr.list */
    unsigned int	xs_log_force;		/* xfs.log.force */
    unsigned int	xs_log_force_sleep;	/* xfs.log.force_sleep */
    unsigned int	xs_try_logspace;	/* xfs.log_tail.try_logspace */
    unsigned int	xs_sleep_logspace;	/* xfs.log_tail.sleep_logspace */
    unsigned int	xs_push_ail;		/* xfs.log_tail.push_ail.pushes */
    unsigned int	xs_push_ail_success;	/* xfs.log_tail.push_ail.success */
    unsigned int	xs_push_ail_pushbuf;	/* xfs.log_tail.push_ail.pushbuf */
    unsigned int	xs_push_ail_pinned;	/* xfs.log_tail.push_ail.pinned */
    unsigned int	xs_push_ail_locked;	/* xfs.log_tail.push_ail.locked */
    unsigned int	xs_push_ail_flushing;	/* xfs.log_tail.push_ail.flushing */
    unsigned int	xs_push_ail_restarts;	/* xfs.log_tail.push_ail.restarts */
    unsigned int	xs_push_ail_flush;	/* xfs.log_tail.push_ail.flush */
    unsigned int	xs_qm_dqreclaims;	/* xfs.quota.reclaims */
    unsigned int	xs_qm_dqreclaim_misses;	/* xfs.quota.reclaim_misses */
    unsigned int	xs_qm_dquot_dups;	/* xfs.quota.dquot_dups */
    unsigned int	xs_qm_dqcachemisses;	/* xfs.quota.cachemisses */
    unsigned int	xs_qm_dqcachehits;	/* xfs.quota.cachehits */
    unsigned int	xs_qm_dqwants;		/* xfs.quota.wants */
    unsigned int	xs_qm_dqshake_reclaims;	/* xfs.quota.shake_reclaims */
    unsigned int	xs_qm_dqinact_reclaims;	/* xfs.quota.inact_reclaims */
    unsigned int	xs_iflush_count;	/* xfs.iflush_count */
    unsigned int	xs_icluster_flushcnt;	/* xfs.icluster_flushcnt */
    unsigned int	xs_icluster_flushinode;	/* xfs.icluster_flushinode */
    unsigned int	xs_buf_get;		/* xfs.buffer.get */
    unsigned int	xs_buf_create;		/* xfs.buffer.create */
    unsigned int	xs_buf_get_locked;	/* xfs.buffer.get_locked */
    unsigned int	xs_buf_get_locked_waited; /* xfs.buffer.get_locked_waited */
    unsigned int	xs_buf_busy_locked;	/* xfs.buffer.busy_locked */
    unsigned int	xs_buf_miss_locked;	/* xfs.buffer.miss_locked */
    unsigned int	xs_buf_page_retries;	/* xfs.buffer.page_retries */
    unsigned int	xs_buf_page_found;	/* xfs.buffer.page_found */
    unsigned int	xs_buf_get_read;	/* xfs.buffer.get_read */
    struct vnodes {
	unsigned int	vn_active;		/* xfs.vnodes.active */
	unsigned int	vn_alloc;		/* xfs.vnodes.alloc */
	unsigned int	vn_get;			/* xfs.vnodes.get */
	unsigned int	vn_hold;		/* xfs.vnodes.hold */
	unsigned int	vn_rele;		/* xfs.vnodes.rele */
	unsigned int	vn_reclaim;		/* xfs.vnodes.reclaim */
	unsigned int	vn_remove;		/* xfs.vnodes.remove */
	unsigned int	vn_free;		/* xfs.vnodes.free */
    } vnodes;
    struct xpc {
	__uint64_t	xs_write_bytes;		/* xfs.write_bytes */
	__uint64_t	xs_read_bytes;		/* xfs.read_bytes */
	__uint64_t	xs_xstrat_bytes;	/* xfs.xstrat_bytes */
    } xpc;
} proc_fs_xfs_t;

extern int refresh_proc_fs_xfs(proc_fs_xfs_t *);
