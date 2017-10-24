/*
 * XFS PMDA
 *
 * Copyright (c) 2012-2014,2016 Red Hat.
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
#include "sysfs_xfs.h"

static int		_isDSO = 1; /* for local contexts */
static sysfs_xfs_t	sysfs_xfs; /* global xfs statistics */

/*
 * The XFS instance domain table is direct lookup and sparse.
 * It is initialized in xfs_init(), see below.
 */
static pmdaIndom xfs_indomtab[NUM_INDOMS];
#define INDOM(x) (xfs_indomtab[x].it_indom)

static pmdaMetric xfs_metrictab[] = {

/* xfs.allocs.alloc_extent */
    { &sysfs_xfs.xs_allocx,
      { PMDA_PMID(CLUSTER_XFS,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.allocs.alloc_block */
    { &sysfs_xfs.xs_allocb,
      { PMDA_PMID(CLUSTER_XFS,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.allocs.free_extent*/
    { &sysfs_xfs.xs_freex,
      { PMDA_PMID(CLUSTER_XFS,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.allocs.free_block */
    { &sysfs_xfs.xs_freeb,
      { PMDA_PMID(CLUSTER_XFS,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.alloc_btree.lookup */
    { &sysfs_xfs.xs_abt_lookup,
      { PMDA_PMID(CLUSTER_XFS,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.alloc_btree.compare */
    { &sysfs_xfs.xs_abt_compare,
      { PMDA_PMID(CLUSTER_XFS,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.alloc_btree.insrec */
    { &sysfs_xfs.xs_abt_insrec,
      { PMDA_PMID(CLUSTER_XFS,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.alloc_btree.delrec */
    { &sysfs_xfs.xs_abt_delrec,
      { PMDA_PMID(CLUSTER_XFS,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.block_map.read_ops */
    { &sysfs_xfs.xs_blk_mapr,
      { PMDA_PMID(CLUSTER_XFS,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.write_ops */
    { &sysfs_xfs.xs_blk_mapw,
      { PMDA_PMID(CLUSTER_XFS,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.unmap */
    { &sysfs_xfs.xs_blk_unmap,
      { PMDA_PMID(CLUSTER_XFS,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.add_exlist */
    { &sysfs_xfs.xs_add_exlist,
      { PMDA_PMID(CLUSTER_XFS,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.del_exlist */
    { &sysfs_xfs.xs_del_exlist,
      { PMDA_PMID(CLUSTER_XFS,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.look_exlist */
    { &sysfs_xfs.xs_look_exlist,
      { PMDA_PMID(CLUSTER_XFS,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.cmp_exlist */
    { &sysfs_xfs.xs_cmp_exlist,
      { PMDA_PMID(CLUSTER_XFS,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.bmap_btree.lookup */
    { &sysfs_xfs.xs_bmbt_lookup,
      { PMDA_PMID(CLUSTER_XFS,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.bmap_btree.compare */
    { &sysfs_xfs.xs_bmbt_compare,
      { PMDA_PMID(CLUSTER_XFS,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.bmap_btree.insrec */
    { &sysfs_xfs.xs_bmbt_insrec,
      { PMDA_PMID(CLUSTER_XFS,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.bmap_btree.delrec */
    { &sysfs_xfs.xs_bmbt_delrec,
      { PMDA_PMID(CLUSTER_XFS,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.dir_ops.lookup */
    { &sysfs_xfs.xs_dir_lookup,
      { PMDA_PMID(CLUSTER_XFS,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.dir_ops.create */
    { &sysfs_xfs.xs_dir_create,
      { PMDA_PMID(CLUSTER_XFS,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.dir_ops.remove */
    { &sysfs_xfs.xs_dir_remove,
      { PMDA_PMID(CLUSTER_XFS,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.dir_ops.getdents */
    { &sysfs_xfs.xs_dir_getdents,
      { PMDA_PMID(CLUSTER_XFS,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.transactions.sync */
    { &sysfs_xfs.xs_trans_sync,
      { PMDA_PMID(CLUSTER_XFS,23), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.transactions.async */
    { &sysfs_xfs.xs_trans_async,
      { PMDA_PMID(CLUSTER_XFS,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.transactions.empty */
    { &sysfs_xfs.xs_trans_empty,
      { PMDA_PMID(CLUSTER_XFS,25), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.inode_ops.ig_attempts */
    { &sysfs_xfs.xs_ig_attempts,
      { PMDA_PMID(CLUSTER_XFS,26), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_found */
    { &sysfs_xfs.xs_ig_found,
      { PMDA_PMID(CLUSTER_XFS,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_frecycle */
    { &sysfs_xfs.xs_ig_frecycle,
      { PMDA_PMID(CLUSTER_XFS,28), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_missed */
    { &sysfs_xfs.xs_ig_missed,
      { PMDA_PMID(CLUSTER_XFS,29), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_dup */
    { &sysfs_xfs.xs_ig_dup,
      { PMDA_PMID(CLUSTER_XFS,30), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_reclaims */
    { &sysfs_xfs.xs_ig_reclaims,
      { PMDA_PMID(CLUSTER_XFS,31), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_attrchg */
    { &sysfs_xfs.xs_ig_attrchg,
      { PMDA_PMID(CLUSTER_XFS,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.log.writes */
    { &sysfs_xfs.xs_log_writes,
      { PMDA_PMID(CLUSTER_XFS,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log.blocks */
    { &sysfs_xfs.xs_log_blocks,
      { PMDA_PMID(CLUSTER_XFS,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* xfs.log.noiclogs */
    { &sysfs_xfs.xs_log_noiclogs,
      { PMDA_PMID(CLUSTER_XFS,35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log.force */
    { &sysfs_xfs.xs_log_force,
      { PMDA_PMID(CLUSTER_XFS,36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log.force_sleep */
    { &sysfs_xfs.xs_log_force_sleep,
      { PMDA_PMID(CLUSTER_XFS,37), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.log_tail.try_logspace */
    { &sysfs_xfs.xs_try_logspace,
      { PMDA_PMID(CLUSTER_XFS,38), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.sleep_logspace */
    { &sysfs_xfs.xs_sleep_logspace,
      { PMDA_PMID(CLUSTER_XFS,39), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.pushes */
    { &sysfs_xfs.xs_push_ail,
      { PMDA_PMID(CLUSTER_XFS,40), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.success */
    { &sysfs_xfs.xs_push_ail_success,
      { PMDA_PMID(CLUSTER_XFS,41), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.pushbuf */
    { &sysfs_xfs.xs_push_ail_pushbuf,
      { PMDA_PMID(CLUSTER_XFS,42), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.pinned */
    { &sysfs_xfs.xs_push_ail_pinned,
      { PMDA_PMID(CLUSTER_XFS,43), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.locked */
    { &sysfs_xfs.xs_push_ail_locked,
      { PMDA_PMID(CLUSTER_XFS,44), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.flushing */
    { &sysfs_xfs.xs_push_ail_flushing,
      { PMDA_PMID(CLUSTER_XFS,45), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.restarts */
    { &sysfs_xfs.xs_push_ail_restarts,
      { PMDA_PMID(CLUSTER_XFS,46), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.flush */
    { &sysfs_xfs.xs_push_ail_flush,
      { PMDA_PMID(CLUSTER_XFS,47), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.xstrat.bytes */
    { &sysfs_xfs.xpc.xs_xstrat_bytes,
      { PMDA_PMID(CLUSTER_XFS,48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* xfs.xstrat.quick */
    { &sysfs_xfs.xs_xstrat_quick,
      { PMDA_PMID(CLUSTER_XFS,49), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.xstrat.split */
    { &sysfs_xfs.xs_xstrat_split,
      { PMDA_PMID(CLUSTER_XFS,50), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.write */
    { &sysfs_xfs.xs_write_calls,
      { PMDA_PMID(CLUSTER_XFS,51), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.write_bytes */
    { &sysfs_xfs.xpc.xs_write_bytes,
      { PMDA_PMID(CLUSTER_XFS,52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* xfs.read */
    { &sysfs_xfs.xs_read_calls,
      { PMDA_PMID(CLUSTER_XFS,53), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.read_bytes */
    { &sysfs_xfs.xpc.xs_read_bytes,
      { PMDA_PMID(CLUSTER_XFS,54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* xfs.attr.get */
    { &sysfs_xfs.xs_attr_get,
      { PMDA_PMID(CLUSTER_XFS,55), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.attr.set */
    { &sysfs_xfs.xs_attr_set,
      { PMDA_PMID(CLUSTER_XFS,56), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.attr.remove */
    { &sysfs_xfs.xs_attr_remove,
      { PMDA_PMID(CLUSTER_XFS,57), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.attr.list */
    { &sysfs_xfs.xs_attr_list,
      { PMDA_PMID(CLUSTER_XFS,58), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.quota.reclaims */
    { &sysfs_xfs.xs_qm_dqreclaims,
      { PMDA_PMID(CLUSTER_XFS,59), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.reclaim_misses */
    { &sysfs_xfs.xs_qm_dqreclaim_misses,
      { PMDA_PMID(CLUSTER_XFS,60), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.dquot_dups */
    { &sysfs_xfs.xs_qm_dquot_dups,
      { PMDA_PMID(CLUSTER_XFS,61), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.cachemisses */
    { &sysfs_xfs.xs_qm_dqcachemisses,
      { PMDA_PMID(CLUSTER_XFS,62), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.cachehits */
    { &sysfs_xfs.xs_qm_dqcachehits,
      { PMDA_PMID(CLUSTER_XFS,63), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.wants */
    { &sysfs_xfs.xs_qm_dqwants,
      { PMDA_PMID(CLUSTER_XFS,64), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.shake_reclaims */
    { &sysfs_xfs.xs_qm_dqshake_reclaims,
      { PMDA_PMID(CLUSTER_XFS,65), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.inact_reclaims */
    { &sysfs_xfs.xs_qm_dqinact_reclaims,
      { PMDA_PMID(CLUSTER_XFS,66), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.iflush_count */
    { &sysfs_xfs.xs_iflush_count,
      { PMDA_PMID(CLUSTER_XFS,67), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.icluster_flushcnt */
    { &sysfs_xfs.xs_icluster_flushcnt,
      { PMDA_PMID(CLUSTER_XFS,68), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.icluster_flushinode */
    { &sysfs_xfs.xs_icluster_flushinode,
      { PMDA_PMID(CLUSTER_XFS,69), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.buffer.get */
    { &sysfs_xfs.xs_buf_get,
      { PMDA_PMID(CLUSTER_XFSBUF,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.create */
    { &sysfs_xfs.xs_buf_create,
      { PMDA_PMID(CLUSTER_XFSBUF,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.get_locked */
    { &sysfs_xfs.xs_buf_get_locked,
      { PMDA_PMID(CLUSTER_XFSBUF,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.get_locked_waited */
    { &sysfs_xfs.xs_buf_get_locked_waited,
      { PMDA_PMID(CLUSTER_XFSBUF,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.busy_locked */
    { &sysfs_xfs.xs_buf_busy_locked,
      { PMDA_PMID(CLUSTER_XFSBUF,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.miss_locked */
    { &sysfs_xfs.xs_buf_miss_locked,
      { PMDA_PMID(CLUSTER_XFSBUF,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.page_retries */
    { &sysfs_xfs.xs_buf_page_retries,
      { PMDA_PMID(CLUSTER_XFSBUF,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.page_found */         
    { &sysfs_xfs.xs_buf_page_found,
      { PMDA_PMID(CLUSTER_XFSBUF,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.get_read */         
    { &sysfs_xfs.xs_buf_get_read,
      { PMDA_PMID(CLUSTER_XFSBUF,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.vnodes.active */
    { &sysfs_xfs.vnodes.vn_active,
      { PMDA_PMID(CLUSTER_XFS,70), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* xfs.vnodes.alloc */
    { &sysfs_xfs.vnodes.vn_alloc,
      { PMDA_PMID(CLUSTER_XFS,71), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.get */
    { &sysfs_xfs.vnodes.vn_get,
      { PMDA_PMID(CLUSTER_XFS,72), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.hold */
    { &sysfs_xfs.vnodes.vn_hold,
      { PMDA_PMID(CLUSTER_XFS,73), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.rele */
    { &sysfs_xfs.vnodes.vn_rele,
      { PMDA_PMID(CLUSTER_XFS,74), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.reclaim */
    { &sysfs_xfs.vnodes.vn_reclaim,
      { PMDA_PMID(CLUSTER_XFS,75), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.remove */
    { &sysfs_xfs.vnodes.vn_remove,
      { PMDA_PMID(CLUSTER_XFS,76), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.free */
    { &sysfs_xfs.vnodes.vn_free,
      { PMDA_PMID(CLUSTER_XFS,77), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.log.write_ratio */
    { &sysfs_xfs.xs_log_write_ratio,
      { PMDA_PMID(CLUSTER_XFS,78), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* xfs.control.reset */
    { NULL,
      { PMDA_PMID(CLUSTER_XFS,79), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* xfs.btree.alloc_blocks.lookup */
    { &sysfs_xfs.xs_abtb_2_lookup,
      { PMDA_PMID(CLUSTER_XFS,80), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.compare */
    { &sysfs_xfs.xs_abtb_2_compare,
      { PMDA_PMID(CLUSTER_XFS,81), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.insrec */
    { &sysfs_xfs.xs_abtb_2_insrec,
      { PMDA_PMID(CLUSTER_XFS,82), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.delrec */
    { &sysfs_xfs.xs_abtb_2_delrec,
      { PMDA_PMID(CLUSTER_XFS,83), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.newroot */
    { &sysfs_xfs.xs_abtb_2_newroot,
      { PMDA_PMID(CLUSTER_XFS,84), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.killroot */
    { &sysfs_xfs.xs_abtb_2_killroot,
      { PMDA_PMID(CLUSTER_XFS,85), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.increment */
    { &sysfs_xfs.xs_abtb_2_increment,
      { PMDA_PMID(CLUSTER_XFS,86), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.decrement */
    { &sysfs_xfs.xs_abtb_2_decrement,
      { PMDA_PMID(CLUSTER_XFS,87), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.lshift */
    { &sysfs_xfs.xs_abtb_2_lshift,
      { PMDA_PMID(CLUSTER_XFS,88), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.rshift */
    { &sysfs_xfs.xs_abtb_2_rshift,
      { PMDA_PMID(CLUSTER_XFS,89), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.split */
    { &sysfs_xfs.xs_abtb_2_split,
      { PMDA_PMID(CLUSTER_XFS,90), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.join */
    { &sysfs_xfs.xs_abtb_2_join,
      { PMDA_PMID(CLUSTER_XFS,91), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.alloc */
    { &sysfs_xfs.xs_abtb_2_alloc,
      { PMDA_PMID(CLUSTER_XFS,92), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.free */
    { &sysfs_xfs.xs_abtb_2_free,
      { PMDA_PMID(CLUSTER_XFS,93), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_blocks.moves */
    { &sysfs_xfs.xs_abtb_2_moves,
      { PMDA_PMID(CLUSTER_XFS,94), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.btree.alloc_contig.lookup */
    { &sysfs_xfs.xs_abtc_2_lookup,
      { PMDA_PMID(CLUSTER_XFS,95), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.compare */
    { &sysfs_xfs.xs_abtc_2_compare,
      { PMDA_PMID(CLUSTER_XFS,96), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.insrec */
    { &sysfs_xfs.xs_abtc_2_insrec,
      { PMDA_PMID(CLUSTER_XFS,97), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.delrec */
    { &sysfs_xfs.xs_abtc_2_delrec,
      { PMDA_PMID(CLUSTER_XFS,98), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.newroot */
    { &sysfs_xfs.xs_abtc_2_newroot,
      { PMDA_PMID(CLUSTER_XFS,99), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.killroot */
    { &sysfs_xfs.xs_abtc_2_killroot,
      { PMDA_PMID(CLUSTER_XFS,100), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.increment */
    { &sysfs_xfs.xs_abtc_2_increment,
      { PMDA_PMID(CLUSTER_XFS,101), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.decrement */
    { &sysfs_xfs.xs_abtc_2_decrement,
      { PMDA_PMID(CLUSTER_XFS,102), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.lshift */
    { &sysfs_xfs.xs_abtc_2_lshift,
      { PMDA_PMID(CLUSTER_XFS,103), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.rshift */
    { &sysfs_xfs.xs_abtc_2_rshift,
      { PMDA_PMID(CLUSTER_XFS,104), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.split */
    { &sysfs_xfs.xs_abtc_2_split,
      { PMDA_PMID(CLUSTER_XFS,105), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.join */
    { &sysfs_xfs.xs_abtc_2_join,
      { PMDA_PMID(CLUSTER_XFS,106), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.alloc */
    { &sysfs_xfs.xs_abtc_2_alloc,
      { PMDA_PMID(CLUSTER_XFS,107), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.free */
    { &sysfs_xfs.xs_abtc_2_free,
      { PMDA_PMID(CLUSTER_XFS,108), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.alloc_contig.moves */
    { &sysfs_xfs.xs_abtc_2_moves,
      { PMDA_PMID(CLUSTER_XFS,109), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.btree.block_map.lookup */
    { &sysfs_xfs.xs_bmbt_2_lookup,
      { PMDA_PMID(CLUSTER_XFS,110), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.compare */
    { &sysfs_xfs.xs_bmbt_2_compare,
      { PMDA_PMID(CLUSTER_XFS,111), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.insrec */
    { &sysfs_xfs.xs_bmbt_2_insrec,
      { PMDA_PMID(CLUSTER_XFS,112), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.delrec */
    { &sysfs_xfs.xs_bmbt_2_delrec,
      { PMDA_PMID(CLUSTER_XFS,113), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.newroot */
    { &sysfs_xfs.xs_bmbt_2_newroot,
      { PMDA_PMID(CLUSTER_XFS,114), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.killroot */
    { &sysfs_xfs.xs_bmbt_2_killroot,
      { PMDA_PMID(CLUSTER_XFS,115), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.increment */
    { &sysfs_xfs.xs_bmbt_2_increment,
      { PMDA_PMID(CLUSTER_XFS,116), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.decrement */
    { &sysfs_xfs.xs_bmbt_2_decrement,
      { PMDA_PMID(CLUSTER_XFS,117), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.lshift */
    { &sysfs_xfs.xs_bmbt_2_lshift,
      { PMDA_PMID(CLUSTER_XFS,118), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.rshift */
    { &sysfs_xfs.xs_bmbt_2_rshift,
      { PMDA_PMID(CLUSTER_XFS,119), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.split */
    { &sysfs_xfs.xs_bmbt_2_split,
      { PMDA_PMID(CLUSTER_XFS,120), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.join */
    { &sysfs_xfs.xs_bmbt_2_join,
      { PMDA_PMID(CLUSTER_XFS,121), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.alloc */
    { &sysfs_xfs.xs_bmbt_2_alloc,
      { PMDA_PMID(CLUSTER_XFS,122), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.free */
    { &sysfs_xfs.xs_bmbt_2_free,
      { PMDA_PMID(CLUSTER_XFS,123), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.block_map.moves */
    { &sysfs_xfs.xs_bmbt_2_moves,
      { PMDA_PMID(CLUSTER_XFS,124), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.btree.inode.lookup */
    { &sysfs_xfs.xs_ibt_2_compare,
      { PMDA_PMID(CLUSTER_XFS,125), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.compare */
    { &sysfs_xfs.xs_ibt_2_lookup,
      { PMDA_PMID(CLUSTER_XFS,126), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.insrec */
    { &sysfs_xfs.xs_ibt_2_insrec,
      { PMDA_PMID(CLUSTER_XFS,127), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.delrec */
    { &sysfs_xfs.xs_ibt_2_delrec,
      { PMDA_PMID(CLUSTER_XFS,128), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.newroot */
    { &sysfs_xfs.xs_ibt_2_newroot,
      { PMDA_PMID(CLUSTER_XFS,129), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.killroot */
    { &sysfs_xfs.xs_ibt_2_killroot,
      { PMDA_PMID(CLUSTER_XFS,130), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.increment */
    { &sysfs_xfs.xs_ibt_2_increment,
      { PMDA_PMID(CLUSTER_XFS,131), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.decrement */
    { &sysfs_xfs.xs_ibt_2_decrement,
      { PMDA_PMID(CLUSTER_XFS,132), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.lshift */
    { &sysfs_xfs.xs_ibt_2_lshift,
      { PMDA_PMID(CLUSTER_XFS,133), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.rshift */
    { &sysfs_xfs.xs_ibt_2_rshift,
      { PMDA_PMID(CLUSTER_XFS,134), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.split */
    { &sysfs_xfs.xs_ibt_2_split,
      { PMDA_PMID(CLUSTER_XFS,135), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.join */
    { &sysfs_xfs.xs_ibt_2_join,
      { PMDA_PMID(CLUSTER_XFS,136), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.alloc */
    { &sysfs_xfs.xs_ibt_2_alloc,
      { PMDA_PMID(CLUSTER_XFS,137), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.free */
    { &sysfs_xfs.xs_ibt_2_free,
      { PMDA_PMID(CLUSTER_XFS,138), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.btree.inode.moves */
    { &sysfs_xfs.xs_ibt_2_moves,
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

/* xfs.perdev.allocs.alloc_extent */
    { &sysfs_xfs.xs_allocx,
      { PMDA_PMID(CLUSTER_PERDEV,0), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.allocs.alloc_block */
    { &sysfs_xfs.xs_allocb,
      { PMDA_PMID(CLUSTER_PERDEV,1), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.allocs.free_extent*/
    { &sysfs_xfs.xs_freex,
      { PMDA_PMID(CLUSTER_PERDEV,2), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.allocs.free_block */
    { &sysfs_xfs.xs_freeb,
      { PMDA_PMID(CLUSTER_PERDEV,3), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.alloc_btree.lookup */
    { &sysfs_xfs.xs_abt_lookup,
      { PMDA_PMID(CLUSTER_PERDEV,4), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.alloc_btree.compare */
    { &sysfs_xfs.xs_abt_compare,
      { PMDA_PMID(CLUSTER_PERDEV,5), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.alloc_btree.insrec */
    { &sysfs_xfs.xs_abt_insrec,
      { PMDA_PMID(CLUSTER_PERDEV,6), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.alloc_btree.delrec */
    { &sysfs_xfs.xs_abt_delrec,
      { PMDA_PMID(CLUSTER_PERDEV,7), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.block_map.read_ops */
    { &sysfs_xfs.xs_blk_mapr,
      { PMDA_PMID(CLUSTER_PERDEV,8), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.block_map.write_ops */
    { &sysfs_xfs.xs_blk_mapw,
      { PMDA_PMID(CLUSTER_PERDEV,9), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.block_map.unmap */
    { &sysfs_xfs.xs_blk_unmap,
      { PMDA_PMID(CLUSTER_PERDEV,10), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.block_map.add_exlist */
    { &sysfs_xfs.xs_add_exlist,
      { PMDA_PMID(CLUSTER_PERDEV,11), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.block_map.del_exlist */
    { &sysfs_xfs.xs_del_exlist,
      { PMDA_PMID(CLUSTER_PERDEV,12), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.block_map.look_exlist */
    { &sysfs_xfs.xs_look_exlist,
      { PMDA_PMID(CLUSTER_PERDEV,13), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.block_map.cmp_exlist */
    { &sysfs_xfs.xs_cmp_exlist,
      { PMDA_PMID(CLUSTER_PERDEV,14), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.bmap_btree.lookup */
    { &sysfs_xfs.xs_bmbt_lookup,
      { PMDA_PMID(CLUSTER_PERDEV,15), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.bmap_btree.compare */
    { &sysfs_xfs.xs_bmbt_compare,
      { PMDA_PMID(CLUSTER_PERDEV,16), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.bmap_btree.insrec */
    { &sysfs_xfs.xs_bmbt_insrec,
      { PMDA_PMID(CLUSTER_PERDEV,17), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.bmap_btree.delrec */
    { &sysfs_xfs.xs_bmbt_delrec,
      { PMDA_PMID(CLUSTER_PERDEV,18), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.dir_ops.lookup */
    { &sysfs_xfs.xs_dir_lookup,
      { PMDA_PMID(CLUSTER_PERDEV,19), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.dir_ops.create */
    { &sysfs_xfs.xs_dir_create,
      { PMDA_PMID(CLUSTER_PERDEV,20), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.dir_ops.remove */
    { &sysfs_xfs.xs_dir_remove,
      { PMDA_PMID(CLUSTER_PERDEV,21), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.dir_ops.getdents */
    { &sysfs_xfs.xs_dir_getdents,
      { PMDA_PMID(CLUSTER_PERDEV,22), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.transactions.sync */
    { &sysfs_xfs.xs_trans_sync,
      { PMDA_PMID(CLUSTER_PERDEV,23), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.transactions.async */
    { &sysfs_xfs.xs_trans_async,
      { PMDA_PMID(CLUSTER_PERDEV,24), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.transactions.empty */
    { &sysfs_xfs.xs_trans_empty,
      { PMDA_PMID(CLUSTER_PERDEV,25), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.inode_ops.ig_attempts */
    { &sysfs_xfs.xs_ig_attempts,
      { PMDA_PMID(CLUSTER_PERDEV,26), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.inode_ops.ig_found */
    { &sysfs_xfs.xs_ig_found,
      { PMDA_PMID(CLUSTER_PERDEV,27), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.inode_ops.ig_frecycle */
    { &sysfs_xfs.xs_ig_frecycle,
      { PMDA_PMID(CLUSTER_PERDEV,28), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.inode_ops.ig_missed */
    { &sysfs_xfs.xs_ig_missed,
      { PMDA_PMID(CLUSTER_PERDEV,29), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.inode_ops.ig_dup */
    { &sysfs_xfs.xs_ig_dup,
      { PMDA_PMID(CLUSTER_PERDEV,30), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.inode_ops.ig_reclaims */
    { &sysfs_xfs.xs_ig_reclaims,
      { PMDA_PMID(CLUSTER_PERDEV,31), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.inode_ops.ig_attrchg */
    { &sysfs_xfs.xs_ig_attrchg,
      { PMDA_PMID(CLUSTER_PERDEV,32), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.log.writes */
    { &sysfs_xfs.xs_log_writes,
      { PMDA_PMID(CLUSTER_PERDEV,33), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.log.blocks */
    { &sysfs_xfs.xs_log_blocks,
      { PMDA_PMID(CLUSTER_PERDEV,34), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* xfs.perdev.log.noiclogs */
    { &sysfs_xfs.xs_log_noiclogs,
      { PMDA_PMID(CLUSTER_PERDEV,35), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.log.force */
    { &sysfs_xfs.xs_log_force,
      { PMDA_PMID(CLUSTER_PERDEV,36), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.log.force_sleep */
    { &sysfs_xfs.xs_log_force_sleep,
      { PMDA_PMID(CLUSTER_PERDEV,37), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.log_tail.try_logspace */
    { &sysfs_xfs.xs_try_logspace,
      { PMDA_PMID(CLUSTER_PERDEV,38), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.log_tail.sleep_logspace */
    { &sysfs_xfs.xs_sleep_logspace,
      { PMDA_PMID(CLUSTER_PERDEV,39), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.log_tail.push_ail.pushes */
    { &sysfs_xfs.xs_push_ail,
      { PMDA_PMID(CLUSTER_PERDEV,40), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.log_tail.push_ail.success */
    { &sysfs_xfs.xs_push_ail_success,
      { PMDA_PMID(CLUSTER_PERDEV,41), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.log_tail.push_ail.pushbuf */
    { &sysfs_xfs.xs_push_ail_pushbuf,
      { PMDA_PMID(CLUSTER_PERDEV,42), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.log_tail.push_ail.pinned */
    { &sysfs_xfs.xs_push_ail_pinned,
      { PMDA_PMID(CLUSTER_PERDEV,43), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.log_tail.push_ail.locked */
    { &sysfs_xfs.xs_push_ail_locked,
      { PMDA_PMID(CLUSTER_PERDEV,44), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.log_tail.push_ail.flushing */
    { &sysfs_xfs.xs_push_ail_flushing,
      { PMDA_PMID(CLUSTER_PERDEV,45), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.log_tail.push_ail.restarts */
    { &sysfs_xfs.xs_push_ail_restarts,
      { PMDA_PMID(CLUSTER_PERDEV,46), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.log_tail.push_ail.flush */
    { &sysfs_xfs.xs_push_ail_flush,
      { PMDA_PMID(CLUSTER_PERDEV,47), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.xstrat.bytes */
    { &sysfs_xfs.xpc.xs_xstrat_bytes,
      { PMDA_PMID(CLUSTER_PERDEV,48), PM_TYPE_U64, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* xfs.perdev.xstrat.quick */
    { &sysfs_xfs.xs_xstrat_quick,
      { PMDA_PMID(CLUSTER_PERDEV,49), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.xstrat.split */
    { &sysfs_xfs.xs_xstrat_split,
      { PMDA_PMID(CLUSTER_PERDEV,50), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.write */
    { &sysfs_xfs.xs_write_calls,
      { PMDA_PMID(CLUSTER_PERDEV,51), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.write_bytes */
    { &sysfs_xfs.xpc.xs_write_bytes,
      { PMDA_PMID(CLUSTER_PERDEV,52), PM_TYPE_U64, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* xfs.perdev.read */
    { &sysfs_xfs.xs_read_calls,
      { PMDA_PMID(CLUSTER_PERDEV,53), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.read_bytes */
    { &sysfs_xfs.xpc.xs_read_bytes,
      { PMDA_PMID(CLUSTER_PERDEV,54), PM_TYPE_U64, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* xfs.perdev.attr.get */
    { &sysfs_xfs.xs_attr_get,
      { PMDA_PMID(CLUSTER_PERDEV,55), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.attr.set */
    { &sysfs_xfs.xs_attr_set,
      { PMDA_PMID(CLUSTER_PERDEV,56), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.attr.remove */
    { &sysfs_xfs.xs_attr_remove,
      { PMDA_PMID(CLUSTER_PERDEV,57), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.attr.list */
    { &sysfs_xfs.xs_attr_list,
      { PMDA_PMID(CLUSTER_PERDEV,58), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.quota.reclaims */
    { &sysfs_xfs.xs_qm_dqreclaims,
      { PMDA_PMID(CLUSTER_PERDEV,59), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.quota.reclaim_misses */
    { &sysfs_xfs.xs_qm_dqreclaim_misses,
      { PMDA_PMID(CLUSTER_PERDEV,60), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.quota.dquot_dups */
    { &sysfs_xfs.xs_qm_dquot_dups,
      { PMDA_PMID(CLUSTER_PERDEV,61), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.quota.cachemisses */
    { &sysfs_xfs.xs_qm_dqcachemisses,
      { PMDA_PMID(CLUSTER_PERDEV,62), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.quota.cachehits */
    { &sysfs_xfs.xs_qm_dqcachehits,
      { PMDA_PMID(CLUSTER_PERDEV,63), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.quota.wants */
    { &sysfs_xfs.xs_qm_dqwants,
      { PMDA_PMID(CLUSTER_PERDEV,64), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.quota.shake_reclaims */
    { &sysfs_xfs.xs_qm_dqshake_reclaims,
      { PMDA_PMID(CLUSTER_PERDEV,65), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.quota.inact_reclaims */
    { &sysfs_xfs.xs_qm_dqinact_reclaims,
      { PMDA_PMID(CLUSTER_PERDEV,66), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.iflush_count */
    { &sysfs_xfs.xs_iflush_count,
      { PMDA_PMID(CLUSTER_PERDEV,67), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.icluster_flushcnt */
    { &sysfs_xfs.xs_icluster_flushcnt,
      { PMDA_PMID(CLUSTER_PERDEV,68), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.icluster_flushinode */
    { &sysfs_xfs.xs_icluster_flushinode,
      { PMDA_PMID(CLUSTER_PERDEV,69), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.buffer.get */
    { &sysfs_xfs.xs_buf_get,
      { PMDA_PMID(CLUSTER_PERDEV,140), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.buffer.create */
    { &sysfs_xfs.xs_buf_create,
      { PMDA_PMID(CLUSTER_PERDEV,141), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.buffer.get_locked */
    { &sysfs_xfs.xs_buf_get_locked,
      { PMDA_PMID(CLUSTER_PERDEV,142), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.buffer.get_locked_waited */
    { &sysfs_xfs.xs_buf_get_locked_waited,
      { PMDA_PMID(CLUSTER_PERDEV,143), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.buffer.busy_locked */
    { &sysfs_xfs.xs_buf_busy_locked,
      { PMDA_PMID(CLUSTER_PERDEV,144), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.buffer.miss_locked */
    { &sysfs_xfs.xs_buf_miss_locked,
      { PMDA_PMID(CLUSTER_PERDEV,145), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.buffer.page_retries */
    { &sysfs_xfs.xs_buf_page_retries,
      { PMDA_PMID(CLUSTER_PERDEV,146), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.buffer.page_found */         
    { &sysfs_xfs.xs_buf_page_found,
      { PMDA_PMID(CLUSTER_PERDEV,147), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.buffer.get_read */         
    { &sysfs_xfs.xs_buf_get_read,
      { PMDA_PMID(CLUSTER_PERDEV,148), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.vnodes.active */
    { &sysfs_xfs.vnodes.vn_active,
      { PMDA_PMID(CLUSTER_PERDEV,70), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* xfs.perdev.vnodes.alloc */
    { &sysfs_xfs.vnodes.vn_alloc,
      { PMDA_PMID(CLUSTER_PERDEV,71), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.vnodes.get */
    { &sysfs_xfs.vnodes.vn_get,
      { PMDA_PMID(CLUSTER_PERDEV,72), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.vnodes.hold */
    { &sysfs_xfs.vnodes.vn_hold,
      { PMDA_PMID(CLUSTER_PERDEV,73), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.vnodes.rele */
    { &sysfs_xfs.vnodes.vn_rele,
      { PMDA_PMID(CLUSTER_PERDEV,74), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.vnodes.reclaim */
    { &sysfs_xfs.vnodes.vn_reclaim,
      { PMDA_PMID(CLUSTER_PERDEV,75), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.vnodes.remove */
    { &sysfs_xfs.vnodes.vn_remove,
      { PMDA_PMID(CLUSTER_PERDEV,76), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.vnodes.free */
    { &sysfs_xfs.vnodes.vn_free,
      { PMDA_PMID(CLUSTER_PERDEV,77), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.log.write_ratio */
    { &sysfs_xfs.xs_log_write_ratio,
      { PMDA_PMID(CLUSTER_PERDEV,78), PM_TYPE_FLOAT, DEVICES_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* xfs.perdev.control.reset */
    { NULL,
      { PMDA_PMID(CLUSTER_PERDEV,79), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* xfs.perdev.btree.alloc_blocks.lookup */
    { &sysfs_xfs.xs_abtb_2_lookup,
      { PMDA_PMID(CLUSTER_PERDEV,80), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_blocks.compare */
    { &sysfs_xfs.xs_abtb_2_compare,
      { PMDA_PMID(CLUSTER_PERDEV,81), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_blocks.insrec */
    { &sysfs_xfs.xs_abtb_2_insrec,
      { PMDA_PMID(CLUSTER_PERDEV,82), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_blocks.delrec */
    { &sysfs_xfs.xs_abtb_2_delrec,
      { PMDA_PMID(CLUSTER_PERDEV,83), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_blocks.newroot */
    { &sysfs_xfs.xs_abtb_2_newroot,
      { PMDA_PMID(CLUSTER_PERDEV,84), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_blocks.killroot */
    { &sysfs_xfs.xs_abtb_2_killroot,
      { PMDA_PMID(CLUSTER_PERDEV,85), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_blocks.increment */
    { &sysfs_xfs.xs_abtb_2_increment,
      { PMDA_PMID(CLUSTER_PERDEV,86), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_blocks.decrement */
    { &sysfs_xfs.xs_abtb_2_decrement,
      { PMDA_PMID(CLUSTER_PERDEV,87), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_blocks.lshift */
    { &sysfs_xfs.xs_abtb_2_lshift,
      { PMDA_PMID(CLUSTER_PERDEV,88), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_blocks.rshift */
    { &sysfs_xfs.xs_abtb_2_rshift,
      { PMDA_PMID(CLUSTER_PERDEV,89), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_blocks.split */
    { &sysfs_xfs.xs_abtb_2_split,
      { PMDA_PMID(CLUSTER_PERDEV,90), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_blocks.join */
    { &sysfs_xfs.xs_abtb_2_join,
      { PMDA_PMID(CLUSTER_PERDEV,91), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_blocks.alloc */
    { &sysfs_xfs.xs_abtb_2_alloc,
      { PMDA_PMID(CLUSTER_PERDEV,92), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_blocks.free */
    { &sysfs_xfs.xs_abtb_2_free,
      { PMDA_PMID(CLUSTER_PERDEV,93), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_blocks.moves */
    { &sysfs_xfs.xs_abtb_2_moves,
      { PMDA_PMID(CLUSTER_PERDEV,94), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.btree.alloc_contig.lookup */
    { &sysfs_xfs.xs_abtc_2_lookup,
      { PMDA_PMID(CLUSTER_PERDEV,95), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_contig.compare */
    { &sysfs_xfs.xs_abtc_2_compare,
      { PMDA_PMID(CLUSTER_PERDEV,96), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_contig.insrec */
    { &sysfs_xfs.xs_abtc_2_insrec,
      { PMDA_PMID(CLUSTER_PERDEV,97), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_contig.delrec */
    { &sysfs_xfs.xs_abtc_2_delrec,
      { PMDA_PMID(CLUSTER_PERDEV,98), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_contig.newroot */
    { &sysfs_xfs.xs_abtc_2_newroot,
      { PMDA_PMID(CLUSTER_PERDEV,99), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_contig.killroot */
    { &sysfs_xfs.xs_abtc_2_killroot,
      { PMDA_PMID(CLUSTER_PERDEV,100), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_contig.increment */
    { &sysfs_xfs.xs_abtc_2_increment,
      { PMDA_PMID(CLUSTER_PERDEV,101), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_contig.decrement */
    { &sysfs_xfs.xs_abtc_2_decrement,
      { PMDA_PMID(CLUSTER_PERDEV,102), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_contig.lshift */
    { &sysfs_xfs.xs_abtc_2_lshift,
      { PMDA_PMID(CLUSTER_PERDEV,103), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_contig.rshift */
    { &sysfs_xfs.xs_abtc_2_rshift,
      { PMDA_PMID(CLUSTER_PERDEV,104), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_contig.split */
    { &sysfs_xfs.xs_abtc_2_split,
      { PMDA_PMID(CLUSTER_PERDEV,105), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_contig.join */
    { &sysfs_xfs.xs_abtc_2_join,
      { PMDA_PMID(CLUSTER_PERDEV,106), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_contig.alloc */
    { &sysfs_xfs.xs_abtc_2_alloc,
      { PMDA_PMID(CLUSTER_PERDEV,107), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_contig.free */
    { &sysfs_xfs.xs_abtc_2_free,
      { PMDA_PMID(CLUSTER_PERDEV,108), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.alloc_contig.moves */
    { &sysfs_xfs.xs_abtc_2_moves,
      { PMDA_PMID(CLUSTER_PERDEV,109), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.btree.block_map.lookup */
    { &sysfs_xfs.xs_bmbt_2_lookup,
      { PMDA_PMID(CLUSTER_PERDEV,110), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.block_map.compare */
    { &sysfs_xfs.xs_bmbt_2_compare,
      { PMDA_PMID(CLUSTER_PERDEV,111), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.block_map.insrec */
    { &sysfs_xfs.xs_bmbt_2_insrec,
      { PMDA_PMID(CLUSTER_PERDEV,112), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.block_map.delrec */
    { &sysfs_xfs.xs_bmbt_2_delrec,
      { PMDA_PMID(CLUSTER_PERDEV,113), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.block_map.newroot */
    { &sysfs_xfs.xs_bmbt_2_newroot,
      { PMDA_PMID(CLUSTER_PERDEV,114), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.block_map.killroot */
    { &sysfs_xfs.xs_bmbt_2_killroot,
      { PMDA_PMID(CLUSTER_PERDEV,115), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.block_map.increment */
    { &sysfs_xfs.xs_bmbt_2_increment,
      { PMDA_PMID(CLUSTER_PERDEV,116), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.block_map.decrement */
    { &sysfs_xfs.xs_bmbt_2_decrement,
      { PMDA_PMID(CLUSTER_PERDEV,117), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.block_map.lshift */
    { &sysfs_xfs.xs_bmbt_2_lshift,
      { PMDA_PMID(CLUSTER_PERDEV,118), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.block_map.rshift */
    { &sysfs_xfs.xs_bmbt_2_rshift,
      { PMDA_PMID(CLUSTER_PERDEV,119), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.block_map.split */
    { &sysfs_xfs.xs_bmbt_2_split,
      { PMDA_PMID(CLUSTER_PERDEV,120), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.block_map.join */
    { &sysfs_xfs.xs_bmbt_2_join,
      { PMDA_PMID(CLUSTER_PERDEV,121), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.block_map.alloc */
    { &sysfs_xfs.xs_bmbt_2_alloc,
      { PMDA_PMID(CLUSTER_PERDEV,122), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.block_map.free */
    { &sysfs_xfs.xs_bmbt_2_free,
      { PMDA_PMID(CLUSTER_PERDEV,123), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.block_map.moves */
    { &sysfs_xfs.xs_bmbt_2_moves,
      { PMDA_PMID(CLUSTER_PERDEV,124), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.perdev.btree.inode.lookup */
    { &sysfs_xfs.xs_ibt_2_compare,
      { PMDA_PMID(CLUSTER_PERDEV,125), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.inode.compare */
    { &sysfs_xfs.xs_ibt_2_lookup,
      { PMDA_PMID(CLUSTER_PERDEV,126), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.inode.insrec */
    { &sysfs_xfs.xs_ibt_2_insrec,
      { PMDA_PMID(CLUSTER_PERDEV,127), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.inode.delrec */
    { &sysfs_xfs.xs_ibt_2_delrec,
      { PMDA_PMID(CLUSTER_PERDEV,128), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.inode.newroot */
    { &sysfs_xfs.xs_ibt_2_newroot,
      { PMDA_PMID(CLUSTER_PERDEV,129), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.inode.killroot */
    { &sysfs_xfs.xs_ibt_2_killroot,
      { PMDA_PMID(CLUSTER_PERDEV,130), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.inode.increment */
    { &sysfs_xfs.xs_ibt_2_increment,
      { PMDA_PMID(CLUSTER_PERDEV,131), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.inode.decrement */
    { &sysfs_xfs.xs_ibt_2_decrement,
      { PMDA_PMID(CLUSTER_PERDEV,132), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.inode.lshift */
    { &sysfs_xfs.xs_ibt_2_lshift,
      { PMDA_PMID(CLUSTER_PERDEV,133), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.inode.rshift */
    { &sysfs_xfs.xs_ibt_2_rshift,
      { PMDA_PMID(CLUSTER_PERDEV,134), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.inode.split */
    { &sysfs_xfs.xs_ibt_2_split,
      { PMDA_PMID(CLUSTER_PERDEV,135), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.inode.join */
    { &sysfs_xfs.xs_ibt_2_join,
      { PMDA_PMID(CLUSTER_PERDEV,136), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.inode.alloc */
    { &sysfs_xfs.xs_ibt_2_alloc,
      { PMDA_PMID(CLUSTER_PERDEV,137), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.inode.free */
    { &sysfs_xfs.xs_ibt_2_free,
      { PMDA_PMID(CLUSTER_PERDEV,138), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.perdev.btree.inode.moves */
    { &sysfs_xfs.xs_ibt_2_moves,
      { PMDA_PMID(CLUSTER_PERDEV,139), PM_TYPE_U32, DEVICES_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
};

char * 			xfs_statspath = "";

FILE *
xfs_statsfile(const char *path, const char *mode)
{
    char buffer[MAXPATHLEN];

    pmsprintf(buffer, sizeof(buffer), "%s%s", xfs_statspath, path);
    buffer[MAXPATHLEN-1] = '\0';
    return fopen(buffer, mode);
}

static void
xfs_refresh(pmdaExt *pmda, int *need_refresh)
{
    if (need_refresh[CLUSTER_QUOTA])
	refresh_filesys(INDOM(FILESYS_INDOM), INDOM(QUOTA_PRJ_INDOM));
    if (need_refresh[CLUSTER_PERDEV])
    	refresh_devices(INDOM(DEVICES_INDOM));
    if (need_refresh[CLUSTER_XFS] || need_refresh[CLUSTER_XFSBUF])
    	refresh_sysfs_xfs(&sysfs_xfs);
}

static int
xfs_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    __pmInDom_int	*indomp = (__pmInDom_int *)&indom;
    int			need_refresh[NUM_CLUSTERS] = { 0 };

    if (indomp->serial == DEVICES_INDOM)
	need_refresh[CLUSTER_PERDEV]++;
    if (indomp->serial == FILESYS_INDOM || indomp->serial == QUOTA_PRJ_INDOM)
	need_refresh[CLUSTER_QUOTA]++;
    xfs_refresh(pmda, need_refresh);
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
xfs_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    unsigned long long 	offset;
    struct sysfs_xfs	*xfs;
    struct filesys	*fs;
    int			sts;

    if (mdesc->m_user != NULL && idp->cluster != CLUSTER_PERDEV) {
	if ((idp->cluster == CLUSTER_XFS || idp->cluster == CLUSTER_XFSBUF) &&
	    sysfs_xfs.errcode != 0) {
	    /* no values available for XFS metrics */
	    return 0;
	}

	switch (mdesc->m_desc.type) {
	case PM_TYPE_U32:
	    atom->ul = *(__uint32_t *)mdesc->m_user;
	    break;
	case PM_TYPE_U64:
	    atom->ull = *(__uint64_t *)mdesc->m_user;
	    break;
	case PM_TYPE_FLOAT:
	    atom->f = *(float *)mdesc->m_user;
	    break;
	default:
	    return PM_ERR_TYPE;
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

    case CLUSTER_PERDEV:
	if (mdesc->m_user == NULL)
	    return PM_ERR_PMID;
	if ((xfs = refresh_device(INDOM(DEVICES_INDOM), inst)) == NULL)
	    return PM_ERR_INST;
	/*
	 * Using offset from start of global sysfs structure, calculate the
	 * offset within this device-specific xfs structure and use that.
	 */
	offset = (char *)mdesc->m_user - (char *)&sysfs_xfs;
	switch (mdesc->m_desc.type) {
	case PM_TYPE_U32:
	    atom->ul = *(__uint32_t *)((char *)xfs + offset);
	    break;
	case PM_TYPE_U64:
	    atom->ull = *(__uint64_t *)((char *)xfs + offset);
	    break;
	case PM_TYPE_FLOAT:
	    atom->f = *(float *)((char *)xfs + offset);
	    break;
	default:
	    return PM_ERR_TYPE;
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
xfs_text(int ident, int type, char **buf, pmdaExt *pmda)
{
    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) {
	__pmID_int	*idp = (__pmID_int *)&ident;

	/* share per-device help text with globals */
	if (idp->cluster == CLUSTER_PERDEV) {
	    if (idp->item >= 140 && idp->item <= 148)
		idp->cluster = CLUSTER_XFSBUF;
	    else
		idp->cluster = CLUSTER_XFS;
	}
    }
    return pmdaText(ident, type, buf, pmda);
}


static int
xfs_zero(pmValueSet *vsp)
{
    FILE	*fp;
    int		value;
    int		sts = 0;

    value = vsp->vlist[0].value.lval;
    if (value < 0)
	return PM_ERR_SIGN;

    fp = xfs_statsfile("/sys/fs/xfs/stats_clear", "w");
    if (!fp)	/* fallback to the original path */
	fp = xfs_statsfile("/proc/sys/fs/xfs/stats_clear", "w");
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
	    if ((sts = xfs_zero(vsp)) < 0)
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
    char *envpath;

    if ((envpath = getenv("XFS_STATSPATH")) != NULL)
	xfs_statspath = envpath;

    if (_isDSO) {
	char helppath[MAXPATHLEN];
	int sep = __pmPathSeparator();
	pmsprintf(helppath, sizeof(helppath), "%s%c" "xfs" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_3, "XFS DSO", helppath);
    }

    if (dp->status != 0)
	return;

    dp->version.any.instance = xfs_instance;
    dp->version.any.fetch = xfs_fetch;
    dp->version.any.store = xfs_store;
    dp->version.any.text = xfs_text;
    pmdaSetFetchCallBack(dp, xfs_fetchCallBack);

    xfs_indomtab[FILESYS_INDOM].it_indom = FILESYS_INDOM;
    xfs_indomtab[DEVICES_INDOM].it_indom = DEVICES_INDOM;
    xfs_indomtab[QUOTA_PRJ_INDOM].it_indom = QUOTA_PRJ_INDOM;

    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, xfs_indomtab, sizeof(xfs_indomtab)/sizeof(xfs_indomtab[0]),
		xfs_metrictab, sizeof(xfs_metrictab)/sizeof(xfs_metrictab[0]));
    pmdaCacheOp(INDOM(FILESYS_INDOM), PMDA_CACHE_CULL);
    pmdaCacheOp(INDOM(DEVICES_INDOM), PMDA_CACHE_CULL);
    pmdaCacheOp(INDOM(QUOTA_PRJ_INDOM), PMDA_CACHE_CULL);
}

pmLongOptions   longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions     opts = {
    .short_options = "D:d:l:?",
    .long_options = longopts,
};

int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    pmdaInterface	dispatch;
    char		helppath[MAXPATHLEN];

    _isDSO = 0;
    __pmSetProgname(argv[0]);
    pmsprintf(helppath, sizeof(helppath), "%s%c" "xfs" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_3, pmProgname, XFS, "xfs.log", helppath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    pmdaOpenLog(&dispatch);
    xfs_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
