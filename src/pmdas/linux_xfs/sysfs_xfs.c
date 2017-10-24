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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "sysfs_xfs.h"
#include <sys/stat.h>

static void
refresh_xfs(FILE *fp, sysfs_xfs_t *sysfs_xfs)
{
    char buf[4096];

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (strncmp(buf, "extent_alloc ", 13) == 0)
	    sscanf(buf, "extent_alloc %u %u %u %u",
		    &sysfs_xfs->xs_allocx,
		    &sysfs_xfs->xs_allocb,
		    &sysfs_xfs->xs_freex,
		    &sysfs_xfs->xs_freeb);
	else if (strncmp(buf, "abt ", 4) == 0)
	    sscanf(buf, "abt %u %u %u %u",
		    &sysfs_xfs->xs_abt_lookup,
		    &sysfs_xfs->xs_abt_compare,
		    &sysfs_xfs->xs_abt_insrec,
		    &sysfs_xfs->xs_abt_delrec);
	else if (strncmp(buf, "blk_map ", 8) == 0)
	    sscanf(buf, "blk_map %u %u %u %u %u %u %u", 
		    &sysfs_xfs->xs_blk_mapr,
		    &sysfs_xfs->xs_blk_mapw,
		    &sysfs_xfs->xs_blk_unmap,
		    &sysfs_xfs->xs_add_exlist,
		    &sysfs_xfs->xs_del_exlist,
		    &sysfs_xfs->xs_look_exlist,
		    &sysfs_xfs->xs_cmp_exlist);
	else if (strncmp(buf, "bmbt ", 5) == 0)
	    sscanf(buf, "bmbt %u %u %u %u",
		    &sysfs_xfs->xs_bmbt_lookup,
		    &sysfs_xfs->xs_bmbt_compare,
		    &sysfs_xfs->xs_bmbt_insrec,
		    &sysfs_xfs->xs_bmbt_delrec);
	else if (strncmp(buf, "dir ", 4) == 0)
	    sscanf(buf, "dir %u %u %u %u",
		    &sysfs_xfs->xs_dir_lookup,
		    &sysfs_xfs->xs_dir_create,
		    &sysfs_xfs->xs_dir_remove,
		    &sysfs_xfs->xs_dir_getdents);
	else if (strncmp(buf, "trans ", 6) == 0)
	    sscanf(buf, "trans %u %u %u",
		    &sysfs_xfs->xs_trans_sync,
		    &sysfs_xfs->xs_trans_async,
		    &sysfs_xfs->xs_trans_empty);
	else if (strncmp(buf, "ig ", 3) == 0)
	    sscanf(buf, "ig %u %u %u %u %u %u %u",
		    &sysfs_xfs->xs_ig_attempts,
		    &sysfs_xfs->xs_ig_found,
		    &sysfs_xfs->xs_ig_frecycle,
		    &sysfs_xfs->xs_ig_missed,
		    &sysfs_xfs->xs_ig_dup,
		    &sysfs_xfs->xs_ig_reclaims,
		    &sysfs_xfs->xs_ig_attrchg);
	else if (strncmp(buf, "log ", 4) == 0)
	    sscanf(buf, "log %u %u %u %u %u",
		    &sysfs_xfs->xs_log_writes,
		    &sysfs_xfs->xs_log_blocks,
		    &sysfs_xfs->xs_log_noiclogs,
		    &sysfs_xfs->xs_log_force,
		    &sysfs_xfs->xs_log_force_sleep);
	else if (strncmp(buf, "push_ail ", 9) == 0)
	    sscanf(buf, "push_ail %u %u %u %u %u %u %u %u %u %u", 
		    &sysfs_xfs->xs_try_logspace,
		    &sysfs_xfs->xs_sleep_logspace,
		    &sysfs_xfs->xs_push_ail,
		    &sysfs_xfs->xs_push_ail_success,
		    &sysfs_xfs->xs_push_ail_pushbuf,
		    &sysfs_xfs->xs_push_ail_pinned,
		    &sysfs_xfs->xs_push_ail_locked,
		    &sysfs_xfs->xs_push_ail_flushing,
		    &sysfs_xfs->xs_push_ail_restarts,
		    &sysfs_xfs->xs_push_ail_flush);
	else if (strncmp(buf, "xstrat ", 7) == 0)
	    sscanf(buf, "xstrat %u %u", 
		    &sysfs_xfs->xs_xstrat_quick,
		    &sysfs_xfs->xs_xstrat_split);
	else if (strncmp(buf, "rw ", 3) == 0)
	    sscanf(buf, "rw %u %u",
		    &sysfs_xfs->xs_write_calls,
		    &sysfs_xfs->xs_read_calls);
	else if (strncmp(buf, "attr ", 5) == 0)
	    sscanf(buf, "attr %u %u %u %u",
		    &sysfs_xfs->xs_attr_get,
		    &sysfs_xfs->xs_attr_set,
		    &sysfs_xfs->xs_attr_remove,
		    &sysfs_xfs->xs_attr_list);
	else if (strncmp(buf, "qm ", 3) == 0)
	    sscanf(buf, "qm %u %u %u %u %u %u %u %u",
		    &sysfs_xfs->xs_qm_dqreclaims,
		    &sysfs_xfs->xs_qm_dqreclaim_misses,
		    &sysfs_xfs->xs_qm_dquot_dups,
		    &sysfs_xfs->xs_qm_dqcachemisses,
		    &sysfs_xfs->xs_qm_dqcachehits,
		    &sysfs_xfs->xs_qm_dqwants,
		    &sysfs_xfs->xs_qm_dqshake_reclaims,
		    &sysfs_xfs->xs_qm_dqinact_reclaims);
	else if (strncmp(buf, "icluster ", 9) == 0)
	    sscanf(buf, "icluster %u %u %u",
		    &sysfs_xfs->xs_iflush_count,
		    &sysfs_xfs->xs_icluster_flushcnt,
		    &sysfs_xfs->xs_icluster_flushinode);
	else if (strncmp(buf, "buf ", 4) == 0)
	    sscanf(buf, "buf %u %u %u %u %u %u %u %u %u",
		    &sysfs_xfs->xs_buf_get,
		    &sysfs_xfs->xs_buf_create,
		    &sysfs_xfs->xs_buf_get_locked,
		    &sysfs_xfs->xs_buf_get_locked_waited,
		    &sysfs_xfs->xs_buf_busy_locked,
		    &sysfs_xfs->xs_buf_miss_locked,
		    &sysfs_xfs->xs_buf_page_retries,
		    &sysfs_xfs->xs_buf_page_found,
		    &sysfs_xfs->xs_buf_get_read);		    
	else if (strncmp(buf, "vnodes ", 7) == 0)
	    sscanf(buf, "vnodes %u %u %u %u %u %u %u %u",
		    &sysfs_xfs->vnodes.vn_active,
		    &sysfs_xfs->vnodes.vn_alloc,
		    &sysfs_xfs->vnodes.vn_get,
		    &sysfs_xfs->vnodes.vn_hold,
		    &sysfs_xfs->vnodes.vn_rele,
		    &sysfs_xfs->vnodes.vn_reclaim,
		    &sysfs_xfs->vnodes.vn_remove,
		    &sysfs_xfs->vnodes.vn_free);
	else if (strncmp(buf, "abtb2 ", 6) == 0)
	    sscanf(buf, "abtb2 %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
		    &sysfs_xfs->xs_abtb_2_lookup,
		    &sysfs_xfs->xs_abtb_2_compare,
		    &sysfs_xfs->xs_abtb_2_insrec,
		    &sysfs_xfs->xs_abtb_2_delrec,
		    &sysfs_xfs->xs_abtb_2_newroot,
		    &sysfs_xfs->xs_abtb_2_killroot,
		    &sysfs_xfs->xs_abtb_2_increment,
		    &sysfs_xfs->xs_abtb_2_decrement,
		    &sysfs_xfs->xs_abtb_2_lshift,
		    &sysfs_xfs->xs_abtb_2_rshift,
		    &sysfs_xfs->xs_abtb_2_split,
		    &sysfs_xfs->xs_abtb_2_join,
		    &sysfs_xfs->xs_abtb_2_alloc,
		    &sysfs_xfs->xs_abtb_2_free,
		    &sysfs_xfs->xs_abtb_2_moves);
	else if (strncmp(buf, "abtc2 ", 6) == 0)
	    sscanf(buf, "abtc2 %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
		    &sysfs_xfs->xs_abtc_2_lookup,
		    &sysfs_xfs->xs_abtc_2_compare,
		    &sysfs_xfs->xs_abtc_2_insrec,
		    &sysfs_xfs->xs_abtc_2_delrec,
		    &sysfs_xfs->xs_abtc_2_newroot,
		    &sysfs_xfs->xs_abtc_2_killroot,
		    &sysfs_xfs->xs_abtc_2_increment,
		    &sysfs_xfs->xs_abtc_2_decrement,
		    &sysfs_xfs->xs_abtc_2_lshift,
		    &sysfs_xfs->xs_abtc_2_rshift,
		    &sysfs_xfs->xs_abtc_2_split,
		    &sysfs_xfs->xs_abtc_2_join,
		    &sysfs_xfs->xs_abtc_2_alloc,
		    &sysfs_xfs->xs_abtc_2_free,
		    &sysfs_xfs->xs_abtc_2_moves);
	else if (strncmp(buf, "bmbt2 ", 6) == 0)
	    sscanf(buf, "bmbt2 %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
		    &sysfs_xfs->xs_bmbt_2_lookup,
		    &sysfs_xfs->xs_bmbt_2_compare,
		    &sysfs_xfs->xs_bmbt_2_insrec,
		    &sysfs_xfs->xs_bmbt_2_delrec,
		    &sysfs_xfs->xs_bmbt_2_newroot,
		    &sysfs_xfs->xs_bmbt_2_killroot,
		    &sysfs_xfs->xs_bmbt_2_increment,
		    &sysfs_xfs->xs_bmbt_2_decrement,
		    &sysfs_xfs->xs_bmbt_2_lshift,
		    &sysfs_xfs->xs_bmbt_2_rshift,
		    &sysfs_xfs->xs_bmbt_2_split,
		    &sysfs_xfs->xs_bmbt_2_join,
		    &sysfs_xfs->xs_bmbt_2_alloc,
		    &sysfs_xfs->xs_bmbt_2_free,
		    &sysfs_xfs->xs_bmbt_2_moves);
	else if (strncmp(buf, "ibt2 ", 5) == 0)
	    sscanf(buf, "ibt2 %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
		    &sysfs_xfs->xs_ibt_2_lookup,
		    &sysfs_xfs->xs_ibt_2_compare,
		    &sysfs_xfs->xs_ibt_2_insrec,
		    &sysfs_xfs->xs_ibt_2_delrec,
		    &sysfs_xfs->xs_ibt_2_newroot,
		    &sysfs_xfs->xs_ibt_2_killroot,
		    &sysfs_xfs->xs_ibt_2_increment,
		    &sysfs_xfs->xs_ibt_2_decrement,
		    &sysfs_xfs->xs_ibt_2_lshift,
		    &sysfs_xfs->xs_ibt_2_rshift,
		    &sysfs_xfs->xs_ibt_2_split,
		    &sysfs_xfs->xs_ibt_2_join,
		    &sysfs_xfs->xs_ibt_2_alloc,
		    &sysfs_xfs->xs_ibt_2_free,
		    &sysfs_xfs->xs_ibt_2_moves);
	else if (strncmp(buf, "xpc", 3) == 0)
		sscanf(buf, "xpc %llu %llu %llu",
		    (unsigned long long *)&sysfs_xfs->xpc.xs_xstrat_bytes,
		    (unsigned long long *)&sysfs_xfs->xpc.xs_write_bytes,
		    (unsigned long long *)&sysfs_xfs->xpc.xs_read_bytes);
    }

    if (sysfs_xfs->xs_log_writes)
	sysfs_xfs->xs_log_write_ratio = 
	sysfs_xfs->xs_log_blocks / sysfs_xfs->xs_log_writes;

    /* xs_log_blocks counted in units of 512 bytes/block, metric is Kbytes. */
    sysfs_xfs->xs_log_blocks >>= 1;

    sysfs_xfs->uptodate = 1;
    sysfs_xfs->errcode = 0;
}

static void
refresh_xqm(FILE *fp, sysfs_xfs_t *sysfs_xfs)
{
    char buf[4096];

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (strncmp(buf, "qm", 2) == 0)
	    sscanf(buf, "qm %u %u %u %u %u %u %u %u",
			&sysfs_xfs->xs_qm_dqreclaims,
			&sysfs_xfs->xs_qm_dqreclaim_misses,
			&sysfs_xfs->xs_qm_dquot_dups,
			&sysfs_xfs->xs_qm_dqcachemisses,
			&sysfs_xfs->xs_qm_dqcachehits,
			&sysfs_xfs->xs_qm_dqwants,
			&sysfs_xfs->xs_qm_dqshake_reclaims,
			&sysfs_xfs->xs_qm_dqinact_reclaims);
    }
}

int
refresh_sysfs_xfs(sysfs_xfs_t *xfs)
{
    FILE *fp;

    memset(xfs, 0, sizeof(sysfs_xfs_t));
    if ((fp = xfs_statsfile("/sys/fs/xfs/stats/stats", "r")) == NULL)
	/* backwards compat - fallback to the original procfs entry */
	if ((fp = xfs_statsfile("/proc/fs/xfs/stat", "r")) == NULL)
	    xfs->errcode = -oserror();

    if (fp) {
	refresh_xfs(fp, xfs);
	fclose(fp);

	/* backwards compat - now incorporated into main stats file */
	fp = xfs_statsfile("/proc/fs/xfs/xqmstat", "r");
	if (fp != NULL) {
	    refresh_xqm(fp, xfs);
	    fclose(fp);
	}
    }

    if (xfs->errcode == 0)
	return 0;
    return -1;
}

/*
 * The XFS stats kernel export uses device names that may not be persistent e.g. dm-xx,
 * so we have to map from the persistent name used in /proc/mounts and also by the
 * xfs.perdev indom name to the non-persistent name used by xfsstats.
 */
static char *
xfs_stats_device_name(char *devname, char *statsname, int maxlen)
{
    char *slash;

    if (realpath(devname, statsname) == NULL)
    	strcpy(statsname, devname);
    if ((slash = strrchr(statsname, '/')) != NULL)
	return slash+1;
    return statsname;
}

sysfs_xfs_t *
refresh_device(pmInDom devices_indom, int inst)
{
    char path[MAXPATHLEN], *dev;
    char statsdev[MAXPATHLEN];
    sysfs_xfs_t *xfs;
    FILE *fp;
    int sts;

    /* Indom 'dev' name is the full persistent device name, e.g. /dev/mapper/vg-lv1 */
    sts = pmdaCacheLookup(devices_indom, inst, &dev, (void **)&xfs);
    if (sts != PMDA_CACHE_ACTIVE)
	return NULL;
    if (xfs->uptodate)
	return xfs;

    pmsprintf(path, sizeof(path), "%s/sys/fs/xfs/%s/stats/stats",
    	xfs_statspath, xfs_stats_device_name(dev, statsdev, sizeof(statsdev)));
    memset(xfs, 0, sizeof(sysfs_xfs_t));
    if ((fp = fopen(path, "r")) == NULL)
	/* backwards compat - fallback to the original procfs entry */
	if ((fp = xfs_statsfile("/proc/fs/xfs/stat", "r")) == NULL)
	    xfs->errcode = -oserror();

    if (fp) {
	refresh_xfs(fp, xfs);
	fclose(fp);
    }

    if (xfs->errcode == 0)
	return xfs;
    return NULL;
}

int
refresh_devices(pmInDom devices_indom)
{
    DIR *dp;
    char path[MAXPATHLEN], *statsdevice;
    char device[MAXPATHLEN];
    pmInDom indom = devices_indom;
    struct sysfs_xfs *xfs;
    struct dirent *dentry;
    struct stat sb;
    int sts, i;
    FILE *fp;

    /* mark any device currently active as having stale values */
    for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(indom, i, NULL, (void **)&xfs) || !xfs)
	    continue;
	xfs->uptodate = 0;
    }
    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    pmsprintf(path, sizeof(path), "%s/sys/fs/xfs", xfs_statspath);
    if ((dp = opendir(path)) != NULL) {
	while ((dentry = readdir(dp)) != NULL) {
	    statsdevice = dentry->d_name;
	    if (*statsdevice == '.')
		continue;
	    pmsprintf(path, sizeof(path), "%s/sys/fs/xfs/%s/stats/stats",
			xfs_statspath, statsdevice);
	    if (stat(path, &sb) != 0 || !S_ISREG(sb.st_mode))
		continue;

	    /* map to the persistent name for the indom name */
	    device[0] = '\0';
	    if (pmsprintf(path, sizeof(path), "%s/sys/block/%s/dm/name", xfs_statspath, statsdevice) > 0) {
		if ((fp = fopen(path, "r")) != NULL) {
		    if (fgets(path, sizeof(device), fp) != NULL) {
		    	char *p = strrchr(path, '\n');
			if (p) *p = '\0';
			pmsprintf(device, sizeof(device), "/dev/mapper/%s", path);
		    }
		    fclose(fp);
		}
	    }
	    if (strnlen(device, sizeof(device)) == 0)
	    	pmsprintf(device, sizeof(device), "/dev/%s", statsdevice);

	    sts = pmdaCacheLookupName(indom, device, NULL, (void **)&xfs);
	    if (sts == PMDA_CACHE_ACTIVE)
		continue;
	    if (sts == PMDA_CACHE_INACTIVE)	/* re-activate an old device */
		pmdaCacheStore(indom, PMDA_CACHE_ADD, device, xfs);
	    else {	/* new device, first time its been observed */
		if ((xfs = calloc(1, sizeof(sysfs_xfs_t))) == NULL)
		    continue;
		if (pmDebugOptions.libpmda)
		    fprintf(stderr, "refresh_devices: add \"%s\"\n", device);
		pmdaCacheStore(indom, PMDA_CACHE_ADD, device, xfs);
	    }
	}
	closedir(dp);
    }

    return 0;
}
