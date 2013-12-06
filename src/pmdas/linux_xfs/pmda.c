/*
 * XFS PMDA
 *
 * Copyright (c) 2012-2013 Red Hat.
 * Copyright (c) 2000,2004,2007-2008 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "impl.h"
#include "pmda.h"
#include "indom.h"
#include "domain.h"
#include "clusters.h"
#include "filesys.h"
#include "proc_fs_xfs.h"

static proc_fs_xfs_t	proc_fs_xfs;

/*
 * The XFS instance domain table is direct lookup and sparse.
 * It is initialized in xfs_init(), see below.
 */
static pmdaIndom xfs_indomtab[NUM_INDOMS];
#define INDOM(x) (xfs_indomtab[x].it_indom)

static pmdaMetric xfs_metrictab[] = {

/* xfs.allocs.alloc_extent */
    { &proc_fs_xfs.xs_allocx,
      { PMDA_PMID(CLUSTER_XFS,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.allocs.alloc_block */
    { &proc_fs_xfs.xs_allocb,
      { PMDA_PMID(CLUSTER_XFS,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.allocs.free_extent*/
    { &proc_fs_xfs.xs_freex,
      { PMDA_PMID(CLUSTER_XFS,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.allocs.free_block */
    { &proc_fs_xfs.xs_freeb,
      { PMDA_PMID(CLUSTER_XFS,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.alloc_btree.lookup */
    { &proc_fs_xfs.xs_abt_lookup,
      { PMDA_PMID(CLUSTER_XFS,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.alloc_btree.compare */
    { &proc_fs_xfs.xs_abt_compare,
      { PMDA_PMID(CLUSTER_XFS,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.alloc_btree.insrec */
    { &proc_fs_xfs.xs_abt_insrec,
      { PMDA_PMID(CLUSTER_XFS,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.alloc_btree.delrec */
    { &proc_fs_xfs.xs_abt_delrec,
      { PMDA_PMID(CLUSTER_XFS,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.block_map.read_ops */
    { &proc_fs_xfs.xs_blk_mapr,
      { PMDA_PMID(CLUSTER_XFS,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.write_ops */
    { &proc_fs_xfs.xs_blk_mapw,
      { PMDA_PMID(CLUSTER_XFS,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.unmap */
    { &proc_fs_xfs.xs_blk_unmap,
      { PMDA_PMID(CLUSTER_XFS,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.add_exlist */
    { &proc_fs_xfs.xs_add_exlist,
      { PMDA_PMID(CLUSTER_XFS,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.del_exlist */
    { &proc_fs_xfs.xs_del_exlist,
      { PMDA_PMID(CLUSTER_XFS,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.look_exlist */
    { &proc_fs_xfs.xs_look_exlist,
      { PMDA_PMID(CLUSTER_XFS,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.cmp_exlist */
    { &proc_fs_xfs.xs_cmp_exlist,
      { PMDA_PMID(CLUSTER_XFS,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.bmap_btree.lookup */
    { &proc_fs_xfs.xs_bmbt_lookup,
      { PMDA_PMID(CLUSTER_XFS,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.bmap_btree.compare */
    { &proc_fs_xfs.xs_bmbt_compare,
      { PMDA_PMID(CLUSTER_XFS,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.bmap_btree.insrec */
    { &proc_fs_xfs.xs_bmbt_insrec,
      { PMDA_PMID(CLUSTER_XFS,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.bmap_btree.delrec */
    { &proc_fs_xfs.xs_bmbt_delrec,
      { PMDA_PMID(CLUSTER_XFS,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.dir_ops.lookup */
    { &proc_fs_xfs.xs_dir_lookup,
      { PMDA_PMID(CLUSTER_XFS,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.dir_ops.create */
    { &proc_fs_xfs.xs_dir_create,
      { PMDA_PMID(CLUSTER_XFS,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.dir_ops.remove */
    { &proc_fs_xfs.xs_dir_remove,
      { PMDA_PMID(CLUSTER_XFS,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.dir_ops.getdents */
    { &proc_fs_xfs.xs_dir_getdents,
      { PMDA_PMID(CLUSTER_XFS,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.transactions.sync */
    { &proc_fs_xfs.xs_trans_sync,
      { PMDA_PMID(CLUSTER_XFS,23), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.transactions.async */
    { &proc_fs_xfs.xs_trans_async,
      { PMDA_PMID(CLUSTER_XFS,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.transactions.empty */
    { &proc_fs_xfs.xs_trans_empty,
      { PMDA_PMID(CLUSTER_XFS,25), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.inode_ops.ig_attempts */
    { &proc_fs_xfs.xs_ig_attempts,
      { PMDA_PMID(CLUSTER_XFS,26), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_found */
    { &proc_fs_xfs.xs_ig_found,
      { PMDA_PMID(CLUSTER_XFS,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_frecycle */
    { &proc_fs_xfs.xs_ig_frecycle,
      { PMDA_PMID(CLUSTER_XFS,28), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_missed */
    { &proc_fs_xfs.xs_ig_missed,
      { PMDA_PMID(CLUSTER_XFS,29), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_dup */
    { &proc_fs_xfs.xs_ig_dup,
      { PMDA_PMID(CLUSTER_XFS,30), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_reclaims */
    { &proc_fs_xfs.xs_ig_reclaims,
      { PMDA_PMID(CLUSTER_XFS,31), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_attrchg */
    { &proc_fs_xfs.xs_ig_attrchg,
      { PMDA_PMID(CLUSTER_XFS,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.log.writes */
    { &proc_fs_xfs.xs_log_writes,
      { PMDA_PMID(CLUSTER_XFS,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log.blocks */
    { &proc_fs_xfs.xs_log_blocks,
      { PMDA_PMID(CLUSTER_XFS,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* xfs.log.noiclogs */
    { &proc_fs_xfs.xs_log_noiclogs,
      { PMDA_PMID(CLUSTER_XFS,35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log.force */
    { &proc_fs_xfs.xs_log_force,
      { PMDA_PMID(CLUSTER_XFS,36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log.force_sleep */
    { &proc_fs_xfs.xs_log_force_sleep,
      { PMDA_PMID(CLUSTER_XFS,37), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.log_tail.try_logspace */
    { &proc_fs_xfs.xs_try_logspace,
      { PMDA_PMID(CLUSTER_XFS,38), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.sleep_logspace */
    { &proc_fs_xfs.xs_sleep_logspace,
      { PMDA_PMID(CLUSTER_XFS,39), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.pushes */
    { &proc_fs_xfs.xs_push_ail,
      { PMDA_PMID(CLUSTER_XFS,40), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.success */
    { &proc_fs_xfs.xs_push_ail_success,
      { PMDA_PMID(CLUSTER_XFS,41), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.pushbuf */
    { &proc_fs_xfs.xs_push_ail_pushbuf,
      { PMDA_PMID(CLUSTER_XFS,42), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.pinned */
    { &proc_fs_xfs.xs_push_ail_pinned,
      { PMDA_PMID(CLUSTER_XFS,43), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.locked */
    { &proc_fs_xfs.xs_push_ail_locked,
      { PMDA_PMID(CLUSTER_XFS,44), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.flushing */
    { &proc_fs_xfs.xs_push_ail_flushing,
      { PMDA_PMID(CLUSTER_XFS,45), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.restarts */
    { &proc_fs_xfs.xs_push_ail_restarts,
      { PMDA_PMID(CLUSTER_XFS,46), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.flush */
    { &proc_fs_xfs.xs_push_ail_flush,
      { PMDA_PMID(CLUSTER_XFS,47), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.xstrat.bytes */
    { &proc_fs_xfs.xpc.xs_xstrat_bytes,
      { PMDA_PMID(CLUSTER_XFS,48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* xfs.xstrat.quick */
    { &proc_fs_xfs.xs_xstrat_quick,
      { PMDA_PMID(CLUSTER_XFS,49), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.xstrat.split */
    { &proc_fs_xfs.xs_xstrat_split,
      { PMDA_PMID(CLUSTER_XFS,50), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.write */
    { &proc_fs_xfs.xs_write_calls,
      { PMDA_PMID(CLUSTER_XFS,51), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.write_bytes */
    { &proc_fs_xfs.xpc.xs_write_bytes,
      { PMDA_PMID(CLUSTER_XFS,52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* xfs.read */
    { &proc_fs_xfs.xs_read_calls,
      { PMDA_PMID(CLUSTER_XFS,53), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.read_bytes */
    { &proc_fs_xfs.xpc.xs_read_bytes,
      { PMDA_PMID(CLUSTER_XFS,54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* xfs.attr.get */
    { &proc_fs_xfs.xs_attr_get,
      { PMDA_PMID(CLUSTER_XFS,55), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.attr.set */
    { &proc_fs_xfs.xs_attr_set,
      { PMDA_PMID(CLUSTER_XFS,56), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.attr.remove */
    { &proc_fs_xfs.xs_attr_remove,
      { PMDA_PMID(CLUSTER_XFS,57), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.attr.list */
    { &proc_fs_xfs.xs_attr_list,
      { PMDA_PMID(CLUSTER_XFS,58), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.quota.reclaims */
    { &proc_fs_xfs.xs_qm_dqreclaims,
      { PMDA_PMID(CLUSTER_XFS,59), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.reclaim_misses */
    { &proc_fs_xfs.xs_qm_dqreclaim_misses,
      { PMDA_PMID(CLUSTER_XFS,60), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.dquot_dups */
    { &proc_fs_xfs.xs_qm_dquot_dups,
      { PMDA_PMID(CLUSTER_XFS,61), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.cachemisses */
    { &proc_fs_xfs.xs_qm_dqcachemisses,
      { PMDA_PMID(CLUSTER_XFS,62), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.cachehits */
    { &proc_fs_xfs.xs_qm_dqcachehits,
      { PMDA_PMID(CLUSTER_XFS,63), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.wants */
    { &proc_fs_xfs.xs_qm_dqwants,
      { PMDA_PMID(CLUSTER_XFS,64), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.shake_reclaims */
    { &proc_fs_xfs.xs_qm_dqshake_reclaims,
      { PMDA_PMID(CLUSTER_XFS,65), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.inact_reclaims */
    { &proc_fs_xfs.xs_qm_dqinact_reclaims,
      { PMDA_PMID(CLUSTER_XFS,66), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.iflush_count */
    { &proc_fs_xfs.xs_iflush_count,
      { PMDA_PMID(CLUSTER_XFS,67), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.icluster_flushcnt */
    { &proc_fs_xfs.xs_icluster_flushcnt,
      { PMDA_PMID(CLUSTER_XFS,68), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.icluster_flushinode */
    { &proc_fs_xfs.xs_icluster_flushinode,
      { PMDA_PMID(CLUSTER_XFS,69), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.buffer.get */
    { &proc_fs_xfs.xs_buf_get,
      { PMDA_PMID(CLUSTER_XFSBUF,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.create */
    { &proc_fs_xfs.xs_buf_create,
      { PMDA_PMID(CLUSTER_XFSBUF,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.get_locked */
    { &proc_fs_xfs.xs_buf_get_locked,
      { PMDA_PMID(CLUSTER_XFSBUF,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.get_locked_waited */
    { &proc_fs_xfs.xs_buf_get_locked_waited,
      { PMDA_PMID(CLUSTER_XFSBUF,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.busy_locked */
    { &proc_fs_xfs.xs_buf_busy_locked,
      { PMDA_PMID(CLUSTER_XFSBUF,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.miss_locked */
    { &proc_fs_xfs.xs_buf_miss_locked,
      { PMDA_PMID(CLUSTER_XFSBUF,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.page_retries */
    { &proc_fs_xfs.xs_buf_page_retries,
      { PMDA_PMID(CLUSTER_XFSBUF,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.page_found */         
    { &proc_fs_xfs.xs_buf_page_found,
      { PMDA_PMID(CLUSTER_XFSBUF,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.get_read */         
    { &proc_fs_xfs.xs_buf_get_read,
      { PMDA_PMID(CLUSTER_XFSBUF,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.vnodes.active */
    { &proc_fs_xfs.vnodes.vn_active,
      { PMDA_PMID(CLUSTER_XFS,70), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* xfs.vnodes.alloc */
    { &proc_fs_xfs.vnodes.vn_alloc,
      { PMDA_PMID(CLUSTER_XFS,71), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.get */
    { &proc_fs_xfs.vnodes.vn_get,
      { PMDA_PMID(CLUSTER_XFS,72), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.hold */
    { &proc_fs_xfs.vnodes.vn_hold,
      { PMDA_PMID(CLUSTER_XFS,73), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.rele */
    { &proc_fs_xfs.vnodes.vn_rele,
      { PMDA_PMID(CLUSTER_XFS,74), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.reclaim */
    { &proc_fs_xfs.vnodes.vn_reclaim,
      { PMDA_PMID(CLUSTER_XFS,75), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.remove */
    { &proc_fs_xfs.vnodes.vn_remove,
      { PMDA_PMID(CLUSTER_XFS,76), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.free */
    { &proc_fs_xfs.vnodes.vn_free,
      { PMDA_PMID(CLUSTER_XFS,77), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.log.write_ratio */
    { &proc_fs_xfs.xs_log_write_ratio,
      { PMDA_PMID(CLUSTER_XFS,78), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* xfs.control.reset */
    { NULL,
      { PMDA_PMID(CLUSTER_XFS,79), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* xfs.btree.alloc_blocks.lookup */
    { &proc_fs_xfs.xs_abtb_2_lookup,
      { PMDA_PMID(CLUSTER_XFS,80), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.compare */
    { &proc_fs_xfs.xs_abtb_2_compare,
      { PMDA_PMID(CLUSTER_XFS,81), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.insrec */
    { &proc_fs_xfs.xs_abtb_2_insrec,
      { PMDA_PMID(CLUSTER_XFS,82), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.delrec */
    { &proc_fs_xfs.xs_abtb_2_delrec,
      { PMDA_PMID(CLUSTER_XFS,83), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.newroot */
    { &proc_fs_xfs.xs_abtb_2_newroot,
      { PMDA_PMID(CLUSTER_XFS,84), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.killroot */
    { &proc_fs_xfs.xs_abtb_2_killroot,
      { PMDA_PMID(CLUSTER_XFS,85), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.increment */
    { &proc_fs_xfs.xs_abtb_2_increment,
      { PMDA_PMID(CLUSTER_XFS,86), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.decrement */
    { &proc_fs_xfs.xs_abtb_2_decrement,
      { PMDA_PMID(CLUSTER_XFS,87), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.lshift */
    { &proc_fs_xfs.xs_abtb_2_lshift,
      { PMDA_PMID(CLUSTER_XFS,88), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.rshift */
    { &proc_fs_xfs.xs_abtb_2_rshift,
      { PMDA_PMID(CLUSTER_XFS,89), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.split */
    { &proc_fs_xfs.xs_abtb_2_split,
      { PMDA_PMID(CLUSTER_XFS,90), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.join */
    { &proc_fs_xfs.xs_abtb_2_join,
      { PMDA_PMID(CLUSTER_XFS,91), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.alloc */
    { &proc_fs_xfs.xs_abtb_2_alloc,
      { PMDA_PMID(CLUSTER_XFS,92), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.free */
    { &proc_fs_xfs.xs_abtb_2_free,
      { PMDA_PMID(CLUSTER_XFS,93), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.moves */
    { &proc_fs_xfs.xs_abtb_2_moves,
      { PMDA_PMID(CLUSTER_XFS,94), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.btree.alloc_contig.lookup */
    { &proc_fs_xfs.xs_abtc_2_lookup,
      { PMDA_PMID(CLUSTER_XFS,95), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.compare */
    { &proc_fs_xfs.xs_abtc_2_compare,
      { PMDA_PMID(CLUSTER_XFS,96), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.insrec */
    { &proc_fs_xfs.xs_abtc_2_insrec,
      { PMDA_PMID(CLUSTER_XFS,97), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.delrec */
    { &proc_fs_xfs.xs_abtc_2_delrec,
      { PMDA_PMID(CLUSTER_XFS,98), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.newroot */
    { &proc_fs_xfs.xs_abtc_2_newroot,
      { PMDA_PMID(CLUSTER_XFS,99), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.killroot */
    { &proc_fs_xfs.xs_abtc_2_killroot,
      { PMDA_PMID(CLUSTER_XFS,100), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.increment */
    { &proc_fs_xfs.xs_abtc_2_increment,
      { PMDA_PMID(CLUSTER_XFS,101), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.decrement */
    { &proc_fs_xfs.xs_abtc_2_decrement,
      { PMDA_PMID(CLUSTER_XFS,102), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.lshift */
    { &proc_fs_xfs.xs_abtc_2_lshift,
      { PMDA_PMID(CLUSTER_XFS,103), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.rshift */
    { &proc_fs_xfs.xs_abtc_2_rshift,
      { PMDA_PMID(CLUSTER_XFS,104), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.split */
    { &proc_fs_xfs.xs_abtc_2_split,
      { PMDA_PMID(CLUSTER_XFS,105), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.join */
    { &proc_fs_xfs.xs_abtc_2_join,
      { PMDA_PMID(CLUSTER_XFS,106), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.alloc */
    { &proc_fs_xfs.xs_abtc_2_alloc,
      { PMDA_PMID(CLUSTER_XFS,107), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.free */
    { &proc_fs_xfs.xs_abtc_2_free,
      { PMDA_PMID(CLUSTER_XFS,108), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.moves */
    { &proc_fs_xfs.xs_abtc_2_moves,
      { PMDA_PMID(CLUSTER_XFS,109), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.btree.block_map.lookup */
    { &proc_fs_xfs.xs_bmbt_2_lookup,
      { PMDA_PMID(CLUSTER_XFS,110), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.compare */
    { &proc_fs_xfs.xs_bmbt_2_compare,
      { PMDA_PMID(CLUSTER_XFS,111), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.insrec */
    { &proc_fs_xfs.xs_bmbt_2_insrec,
      { PMDA_PMID(CLUSTER_XFS,112), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.delrec */
    { &proc_fs_xfs.xs_bmbt_2_delrec,
      { PMDA_PMID(CLUSTER_XFS,113), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.newroot */
    { &proc_fs_xfs.xs_bmbt_2_newroot,
      { PMDA_PMID(CLUSTER_XFS,114), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.killroot */
    { &proc_fs_xfs.xs_bmbt_2_killroot,
      { PMDA_PMID(CLUSTER_XFS,115), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.increment */
    { &proc_fs_xfs.xs_bmbt_2_increment,
      { PMDA_PMID(CLUSTER_XFS,116), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.decrement */
    { &proc_fs_xfs.xs_bmbt_2_decrement,
      { PMDA_PMID(CLUSTER_XFS,117), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.lshift */
    { &proc_fs_xfs.xs_bmbt_2_lshift,
      { PMDA_PMID(CLUSTER_XFS,118), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.rshift */
    { &proc_fs_xfs.xs_bmbt_2_rshift,
      { PMDA_PMID(CLUSTER_XFS,119), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.split */
    { &proc_fs_xfs.xs_bmbt_2_split,
      { PMDA_PMID(CLUSTER_XFS,120), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.join */
    { &proc_fs_xfs.xs_bmbt_2_join,
      { PMDA_PMID(CLUSTER_XFS,121), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.alloc */
    { &proc_fs_xfs.xs_bmbt_2_alloc,
      { PMDA_PMID(CLUSTER_XFS,122), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.free */
    { &proc_fs_xfs.xs_bmbt_2_free,
      { PMDA_PMID(CLUSTER_XFS,123), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.moves */
    { &proc_fs_xfs.xs_bmbt_2_moves,
      { PMDA_PMID(CLUSTER_XFS,124), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.btree.inode.lookup */
    { &proc_fs_xfs.xs_ibt_2_compare,
      { PMDA_PMID(CLUSTER_XFS,125), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.compare */
    { &proc_fs_xfs.xs_ibt_2_lookup,
      { PMDA_PMID(CLUSTER_XFS,126), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.insrec */
    { &proc_fs_xfs.xs_ibt_2_insrec,
      { PMDA_PMID(CLUSTER_XFS,127), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.delrec */
    { &proc_fs_xfs.xs_ibt_2_delrec,
      { PMDA_PMID(CLUSTER_XFS,128), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.newroot */
    { &proc_fs_xfs.xs_ibt_2_newroot,
      { PMDA_PMID(CLUSTER_XFS,129), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.killroot */
    { &proc_fs_xfs.xs_ibt_2_killroot,
      { PMDA_PMID(CLUSTER_XFS,130), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.increment */
    { &proc_fs_xfs.xs_ibt_2_increment,
      { PMDA_PMID(CLUSTER_XFS,131), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.decrement */
    { &proc_fs_xfs.xs_ibt_2_decrement,
      { PMDA_PMID(CLUSTER_XFS,132), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.lshift */
    { &proc_fs_xfs.xs_ibt_2_lshift,
      { PMDA_PMID(CLUSTER_XFS,133), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.rshift */
    { &proc_fs_xfs.xs_ibt_2_rshift,
      { PMDA_PMID(CLUSTER_XFS,134), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.split */
    { &proc_fs_xfs.xs_ibt_2_split,
      { PMDA_PMID(CLUSTER_XFS,135), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.join */
    { &proc_fs_xfs.xs_ibt_2_join,
      { PMDA_PMID(CLUSTER_XFS,136), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.alloc */
    { &proc_fs_xfs.xs_ibt_2_alloc,
      { PMDA_PMID(CLUSTER_XFS,137), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.free */
    { &proc_fs_xfs.xs_ibt_2_free,
      { PMDA_PMID(CLUSTER_XFS,138), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.moves */
    { &proc_fs_xfs.xs_ibt_2_moves,
      { PMDA_PMID(CLUSTER_XFS,139), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* quota.state.project.accounting */
    { NULL,
      { PMDA_PMID(CLUSTER_QUOTA,0), PM_TYPE_U32, FILESYS_INDOM, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* quota.state.project.enforcement */
    { NULL,
      { PMDA_PMID(CLUSTER_QUOTA,1), PM_TYPE_U32, FILESYS_INDOM, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* quota.project.space.hard */
    { NULL, 
      { PMDA_PMID(CLUSTER_QUOTA,6), PM_TYPE_U64, QUOTA_PRJ_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* quota.project.space.soft */
    { NULL, 
      { PMDA_PMID(CLUSTER_QUOTA,7), PM_TYPE_U64, QUOTA_PRJ_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* quota.project.space.used */
    { NULL, 
      { PMDA_PMID(CLUSTER_QUOTA,8), PM_TYPE_U64, QUOTA_PRJ_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* quota.project.space.time_left */
    { NULL, 
      { PMDA_PMID(CLUSTER_QUOTA,9), PM_TYPE_32, QUOTA_PRJ_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) }, },
/* quota.project.files.hard */
    { NULL, 
      { PMDA_PMID(CLUSTER_QUOTA,10), PM_TYPE_U64, QUOTA_PRJ_INDOM, PM_SEM_DISCRETE, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* quota.project.files.soft */
    { NULL, 
      { PMDA_PMID(CLUSTER_QUOTA,11), PM_TYPE_U64, QUOTA_PRJ_INDOM, PM_SEM_DISCRETE, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* quota.project.files.used */
    { NULL, 
      { PMDA_PMID(CLUSTER_QUOTA,12), PM_TYPE_U64, QUOTA_PRJ_INDOM, PM_SEM_DISCRETE, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* quota.project.files.time_left */
    { NULL, 
      { PMDA_PMID(CLUSTER_QUOTA,13), PM_TYPE_32, QUOTA_PRJ_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) }, },
};

static void
xfs_refresh(pmdaExt *pmda, int *need_refresh)
{
    if (need_refresh[CLUSTER_QUOTA])
	refresh_filesys(INDOM(FILESYS_INDOM), INDOM(QUOTA_PRJ_INDOM));
    if (need_refresh[CLUSTER_XFS] || need_refresh[CLUSTER_XFSBUF])
    	refresh_proc_fs_xfs(&proc_fs_xfs);
}

static int
xfs_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    __pmInDom_int	*indomp = (__pmInDom_int *)&indom;
    int			need_refresh[NUM_CLUSTERS] = { 0 };

    if (indomp->serial == FILESYS_INDOM || indomp->serial == QUOTA_PRJ_INDOM)
    	need_refresh[CLUSTER_QUOTA]++;
    xfs_refresh(pmda, need_refresh);
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
xfs_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    struct filesys	*fs;
    int			sts;

    if (mdesc->m_user != NULL) {
	if ((idp->cluster == CLUSTER_XFS || idp->cluster == CLUSTER_XFSBUF) &&
	    proc_fs_xfs.errcode != 0) {
	    /* no values available for XFS metrics */
	    return 0;
	}

	switch (mdesc->m_desc.type) {
	case PM_TYPE_32:
	    atom->l = *(__int32_t *)mdesc->m_user;
	    break;
	case PM_TYPE_U32:
	    atom->ul = *(__uint32_t *)mdesc->m_user;
	    break;
	case PM_TYPE_64:
	    atom->ll = *(__int64_t *)mdesc->m_user;
	    break;
	case PM_TYPE_U64:
	    atom->ull = *(__uint64_t *)mdesc->m_user;
	    break;
	case PM_TYPE_FLOAT:
	    atom->f = *(float *)mdesc->m_user;
	    break;
	case PM_TYPE_DOUBLE:
	    atom->d = *(double *)mdesc->m_user;
	    break;
	case PM_TYPE_STRING:
	    atom->cp = (char *)mdesc->m_user;
	    break;
	default:
	    return 0;
	}
    }
    else
    switch (idp->cluster) {

    case CLUSTER_XFS:
	switch (idp->item) {
	case 79: /* xfs.control.reset */
	    atom->ul = 0;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_QUOTA:
	if (idp->item <= 5) {
	    sts = pmdaCacheLookup(INDOM(FILESYS_INDOM), inst, NULL,
					(void **)&fs);
	    if (sts < 0)
		return sts;
	    if (sts != PMDA_CACHE_ACTIVE)
		return PM_ERR_INST;
	    switch (idp->item) {
	    case 0:	/* quota.state.project.accounting */
		atom->ul = !!(fs->flags & FSF_QUOT_PROJ_ACC);
		break;
	    case 1:	/* quota.state.project.enforcement */
		atom->ul = !!(fs->flags & FSF_QUOT_PROJ_ENF);
		break;
	    default:
		return PM_ERR_PMID;
	    }
	}
	else if (idp->item <= 13) {
	    struct project *pp;
	    sts = pmdaCacheLookup(INDOM(QUOTA_PRJ_INDOM), inst, NULL,
					(void **)&pp);
	    if (sts < 0)
		return sts;
	    if (sts != PMDA_CACHE_ACTIVE)
		return PM_ERR_INST;
	    switch (idp->item) {
	    case 6:	/* quota.project.space.hard */
		atom->ull = pp->space_hard >> 1; /* BBs to KB */
		break;
	    case 7:	/* quota.project.space.soft */
		atom->ull = pp->space_soft >> 1; /* BBs to KB */
		break;
	    case 8:	/* quota.project.space.used */
		atom->ull = pp->space_used >> 1; /* BBs to KB */
		break;
	    case 9:	/* quota.project.space.time_left */
		atom->l = pp->space_time_left;
		break;
	    case 10:	/* quota.project.files.hard */
		atom->ull = pp->files_hard;
		break;
	    case 11:	/* quota.project.files.soft */
		atom->ull = pp->files_soft;
		break;
	    case 12:	/* quota.project.files.used */
		atom->ull = pp->files_used;
		break;
	    case 13:	/* quota.project.files.time_left */
		atom->l = pp->files_time_left;
		break;
	    default:
		return PM_ERR_PMID;
	    }
	}
	else
	    return PM_ERR_PMID;
	break;
    }

    return 1;
}

static int
xfs_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int		i, need_refresh[NUM_CLUSTERS] = { 0 };

    for (i = 0; i < numpmid; i++) {
	__pmID_int *idp = (__pmID_int *)&(pmidlist[i]);
	if (idp->cluster >= MIN_CLUSTER && idp->cluster < NUM_CLUSTERS)
	    need_refresh[idp->cluster]++;
    }

    xfs_refresh(pmda, need_refresh);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
procfs_zero(const char *filename, pmValueSet *vsp)
{
    FILE	*fp;
    int		value;
    int		sts = 0;

    value = vsp->vlist[0].value.lval;
    if (value < 0)
	return PM_ERR_SIGN;

    fp = fopen(filename, "w");
    if (!fp) {
	sts = PM_ERR_PERMISSION;
    } else {
	fprintf(fp, "%d\n", value);
	fclose(fp);
    }
    return sts;
}

static int
xfs_store(pmResult *result, pmdaExt *pmda)
{
    int		i;
    int		sts = 0;
    pmValueSet	*vsp;
    __pmID_int	*pmidp;

    for (i = 0; i < result->numpmid && !sts; i++) {
	vsp = result->vset[i];
	pmidp = (__pmID_int *)&vsp->pmid;

	if (pmidp->cluster == CLUSTER_XFS && pmidp->item == 79) {
	    if ((sts = procfs_zero("/proc/sys/fs/xfs/stats_clear", vsp)) < 0)
		break;
	} else {
	    sts = PM_ERR_PERMISSION;
	    break;
	}
    }
    return sts;
}

void 
__PMDA_INIT_CALL
xfs_init(pmdaInterface *dp)
{
    if (dp->status != 0)
	return;

    dp->version.any.fetch = xfs_fetch;
    dp->version.any.store = xfs_store;
    dp->version.any.instance = xfs_instance;
    pmdaSetFetchCallBack(dp, xfs_fetchCallBack);

    xfs_indomtab[FILESYS_INDOM].it_indom = FILESYS_INDOM;
    xfs_indomtab[QUOTA_PRJ_INDOM].it_indom = QUOTA_PRJ_INDOM;

    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, xfs_indomtab, sizeof(xfs_indomtab)/sizeof(xfs_indomtab[0]),
		xfs_metrictab, sizeof(xfs_metrictab)/sizeof(xfs_metrictab[0]));
    pmdaCacheOp(INDOM(FILESYS_INDOM), PMDA_CACHE_CULL);
    pmdaCacheOp(INDOM(QUOTA_PRJ_INDOM), PMDA_CACHE_CULL);
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain   use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile  write log into logfile rather than using default log name\n",
	  stderr);		
    exit(1);
}

int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    int			err = 0;
    pmdaInterface	dispatch;
    char		helppath[MAXPATHLEN];

    __pmSetProgname(argv[0]);

    snprintf(helppath, sizeof(helppath), "%s%c" "xfs" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_3, pmProgname, XFS, "xfs.log", helppath);

    while (pmdaGetOpt(argc, argv, "D:d:l:?", &dispatch, &err) != EOF)
	err++;
    if (err)
	usage();

    pmdaOpenLog(&dispatch);
    xfs_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
