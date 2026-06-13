/*
 * Copyright (c) 2026 Red Hat.
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
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "btrfs_utils.h"
#include "btrfs_fs.h"

static void
btrfs_fs_refresh_alloc(const char *fs_path, const char *type, btrfs_alloc_t *alloc)
{
    char path[MAXPATHLEN];

    pmsprintf(path, sizeof(path), "%s/allocation/%s/total_bytes",
	    fs_path, type);
    btrfs_read_u64(path, &alloc->total_bytes);

    pmsprintf(path, sizeof(path), "%s/allocation/%s/bytes_used",
	    fs_path, type);
    btrfs_read_u64(path, &alloc->bytes_used);

    pmsprintf(path, sizeof(path), "%s/allocation/%s/bytes_pinned",
	    fs_path, type);
    btrfs_read_u64(path, &alloc->bytes_pinned);

    pmsprintf(path, sizeof(path), "%s/allocation/%s/bytes_reserved",
	    fs_path, type);
    btrfs_read_u64(path, &alloc->bytes_reserved);

    pmsprintf(path, sizeof(path), "%s/allocation/%s/bytes_readonly",
	    fs_path, type);
    btrfs_read_u64(path, &alloc->bytes_readonly);

    pmsprintf(path, sizeof(path), "%s/allocation/%s/disk_used",
	    fs_path, type);
    btrfs_read_u64(path, &alloc->disk_used);

    pmsprintf(path, sizeof(path), "%s/allocation/%s/disk_total",
	    fs_path, type);
    btrfs_read_u64(path, &alloc->disk_total);
}

static void
btrfs_fs_refresh_commit(const char *fs_path, btrfs_commit_t *commit)
{
    char path[MAXPATHLEN], *line = NULL;
    size_t len = 0;
    FILE *fp;

    memset(commit, 0, sizeof(*commit));
    pmsprintf(path, sizeof(path), "%s/commit_stats", fs_path);
    fp = fopen(path, "r");
    if (fp == NULL)
	return;
    while (getline(&line, &len, fp) != -1) {
	if (strncmp(line, "commits", 7) == 0)
	    sscanf(line, "commits %llu", (unsigned long long *)&commit->commits);
	else if (strncmp(line, "last_commit_ms", 14) == 0)
	    sscanf(line, "last_commit_ms %llu", (unsigned long long *)&commit->last_commit_ms);
	else if (strncmp(line, "max_commit_ms", 13) == 0)
	    sscanf(line, "max_commit_ms %llu", (unsigned long long *)&commit->max_commit_ms);
	else if (strncmp(line, "total_commit_ms", 15) == 0)
	    sscanf(line, "total_commit_ms %llu", (unsigned long long *)&commit->total_commit_ms);
    }
    free(line);
    fclose(fp);
}

static void
btrfs_fs_refresh_discard(const char *fs_path, btrfs_discard_t *discard)
{
    char path[MAXPATHLEN];

    memset(discard, 0, sizeof(*discard));

    pmsprintf(path, sizeof(path), "%s/discard/discardable_bytes", fs_path);
    btrfs_read_u64(path, &discard->discardable_bytes);

    pmsprintf(path, sizeof(path), "%s/discard/discardable_extents", fs_path);
    btrfs_read_u64(path, &discard->discardable_extents);

    pmsprintf(path, sizeof(path), "%s/discard/discard_bitmap_bytes",
	    fs_path);
    btrfs_read_u64(path, &discard->discard_bitmap_bytes);

    pmsprintf(path, sizeof(path), "%s/discard/discard_extent_bytes",
	    fs_path);
    btrfs_read_u64(path, &discard->discard_extent_bytes);

    pmsprintf(path, sizeof(path), "%s/discard/discard_bytes_saved",
	    fs_path);
    btrfs_read_u64(path, &discard->discard_bytes_saved);

    pmsprintf(path, sizeof(path), "%s/discard/iops_limit",
	    fs_path);
    btrfs_read_u32(path, &discard->iops_limit);

    pmsprintf(path, sizeof(path), "%s/discard/kbps_limit",
	    fs_path);
    btrfs_read_u32(path, &discard->kbps_limit);

    pmsprintf(path, sizeof(path), "%s/discard/max_discard_size",
	    fs_path);
    btrfs_read_u64(path, &discard->max_discard_size);
}

static void
btrfs_fs_refresh_info(const char *fs_path, btrfs_info_t *info)
{
    char path[MAXPATHLEN];

    memset(info, 0, sizeof(*info));

    pmsprintf(path, sizeof(path), "%s/label", fs_path);
    btrfs_read_string(path, info->label, sizeof(info->label));

    pmsprintf(path, sizeof(path), "%s/nodesize", fs_path);
    btrfs_read_u64(path, &info->nodesize);

    pmsprintf(path, sizeof(path), "%s/sectorsize", fs_path);
    btrfs_read_u64(path, &info->sectorsize);

    pmsprintf(path, sizeof(path), "%s/generation", fs_path);
    btrfs_read_u64(path, &info->generation);

    pmsprintf(path, sizeof(path), "%s/checksum", fs_path);
    btrfs_read_string(path, info->checksum, sizeof(info->checksum));

    pmsprintf(path, sizeof(path), "%s/metadata_uuid", fs_path);
    btrfs_read_string(path, info->metadata_uuid, sizeof(info->metadata_uuid));
}

void
btrfs_fs_refresh(pmInDom indom)
{
    DIR *dp;
    struct dirent *ep;
    struct stat sstat;
    char fs_path[MAXPATHLEN], path[MAXPATHLEN];
    int sts;
    btrfs_fs_t *fs;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    dp = opendir(btrfs_path);
    if (dp == NULL) {
	if (pmDebugOptions.appl0)
	    pmNotifyErr(LOG_WARNING,
		"btrfs_fs_refresh: failed to open \"%s\": %s",
		btrfs_path, pmErrStr(-errno));
	return;
    }

    while ((ep = readdir(dp)) != NULL) {
	if (ep->d_name[0] == '.')
	    continue;
	if (strcmp(ep->d_name, "features") == 0)
	    continue;
	pmsprintf(fs_path, sizeof(fs_path), "%s/%s",
		btrfs_path, ep->d_name);
	if (stat(fs_path, &sstat) < 0 || !S_ISDIR(sstat.st_mode))
	    continue;

	sts = pmdaCacheLookupName(indom, ep->d_name, NULL, (void **)&fs);
	if (sts == PM_ERR_INST || (sts >= 0 && fs == NULL)) {
	    fs = calloc(1, sizeof(*fs));
	    if (fs == NULL) {
		pmNoMem("btrfs_fs", sizeof(*fs), PM_RECOV_ERR);
		continue;
	    }
	}
	else if (sts < 0)
	    continue;

	pmstrncpy(fs->uuid, sizeof(fs->uuid), ep->d_name);
	btrfs_fs_refresh_info(fs_path, &fs->info);
	btrfs_fs_refresh_commit(fs_path, &fs->commit);
	btrfs_fs_refresh_alloc(fs_path, "data", &fs->alloc_data);
	btrfs_fs_refresh_alloc(fs_path, "metadata", &fs->alloc_metadata);
	btrfs_fs_refresh_alloc(fs_path, "system", &fs->alloc_system);

	pmsprintf(path, sizeof(path), "%s/allocation/global_rsv_size",
		fs_path);
	btrfs_read_u64(path, &fs->global_rsv.size);

	pmsprintf(path, sizeof(path), "%s/allocation/global_rsv_reserved",
		fs_path);
	btrfs_read_u64(path, &fs->global_rsv.reserved);

	btrfs_fs_refresh_discard(fs_path, &fs->discard);

	pmdaCacheStore(indom, PMDA_CACHE_ADD, ep->d_name, (void *)fs);
    }
    closedir(dp);
}
