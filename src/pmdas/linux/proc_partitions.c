/*
 * Linux Partitions (disk and disk partition IO stats) Cluster
 *
 * Copyright (c) 2012-2018 Red Hat.
 * Copyright (c) 2015 Intel, Inc.  All Rights Reserved.
 * Copyright (c) 2008,2012 Aconex.  All Rights Reserved.
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

#include <ctype.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "linux.h"
#include "proc_partitions.h"

static int _pm_have_kernel_2_6_partition_stats;

/*
 * _pm_ispartition : return true if arg is a partition name
 *                   return false if arg is a disk name
 * ide disks are named e.g. hda
 * ide partitions are named e.g. hda1
 *
 * scsi disks are named e.g. sda
 * scsi partitions are named e.g. sda1
 *
 * device-mapper (DM) devices are named dm-[0-9]* and are mapped to their
 * persistent name using the symlinks in /dev/mapper.
 *
 * multi-device (MD) devices are named md[0-9]* and are mapped to optional
 * persistent names using the symlinks in /dev/md.
 *
 * Mylex raid disks are named e.g. rd/c0d0 or dac960/c0d0
 * Mylex raid partitions are named e.g. rd/c0d0p1 or dac960/c0d0p1
 *
 * Ceph RADOS block devices are named e.g. rbd0
 * Ceph RADOS block device partitions are named e.g. rbd0p1
 *
 * Network Block Devices (NBD) are named e.g. nbd0
 * Network Block Device partitions are named e.g. nbd0p1
 *
 * What this now tries to do is be a bit smarter, and guess that names
 * with slashes that end in the form .../c0t0d0[p0], and ones without
 * are good old 19th century device names like xx0 or xx0a.
 */
static int
_pm_isloop(char *dname)
{
    return strncmp(dname, "loop", 4) == 0;
}

static int
_pm_isramdisk(char *dname)
{
    return strncmp(dname, "ram", 3) == 0;
}

static int
_pm_ismmcdisk(char *dname)
{
    if (strncmp(dname, "mmcblk", 6) != 0)
	return 0;
    /*
     * Are we a disk or a partition of the disk? If there is a "p" 
     * assume it is a partition - e.g. mmcblk0p6.
     */
    return (strchr(dname + 6, 'p') == NULL);
}

static int
_pm_isnvmedrive(char *dname)
{
    if (strncmp(dname, "nvme", 4) != 0)
        return 0;
    /*
     * Are we a disk or a partition of the disk? If there is a "p"
     * assume it is a partition - e.g. nvme0n1p1.
     */
    return (strchr(dname + 4, 'p') == NULL);
}

static int
_pm_iscephrados(char *dname)
{
    if (strncmp(dname, "rbd", 3) != 0)
        return 0;
    /*
     * Are we a disk or a partition of the disk? If there is a "p"
     * assume it is a partition - e.g. rbd7p1.
     */
    return (strchr(dname + 3, 'p') == NULL);
}

static int
_pm_isnbd(char *dname)
{
    if (strncmp(dname, "nbd", 3) != 0)
        return 0;
    /*
     * Are we a disk or a partition of the disk? If there is a "p"
     * assume it is a partition - e.g. nbd7p1.
     */
    return (strchr(dname + 3, 'p') == NULL);
}

static int
_pm_ismd(char *dname)
{
    return strncmp(dname, "md", 2) == 0;
}

static int
_pm_isdm(char *dname)
{
    return strncmp(dname, "dm-", 3) == 0;
}

static int
_pm_iscdrom(char *dname)
{
    return (strncmp(dname, "sr", 2) == 0) && (isdigit(dname[2]));
}

/*
 * slight improvement to heuristic suggested by
 * Tim Bradshaw <tfb@cley.com> on 29 Dec 2003
 */
int
_pm_ispartition(char *dname)
{
    int p, m = strlen(dname) - 1;

    /*
     * looking at something like foo/x, and we hope x ends p<n>, for 
     * a partition, or not for a disk.
     */
    if (strchr(dname, '/')) {
	for (p = m; p > 0 && isdigit((int)dname[p]); p--)
	    ;
	if (p == m)
	    /* name had no trailing digits.  Wildly guess a disk. */
	    return 1;
	else 
	    /*
	     * ends with digits, if preceding character is a 'p' punt
	     * on a partition
	     */
	    return (dname[p] == 'p'? 1 : 0);
    }
    else {
	/*
	 * default test : partition names end in a digit do not look
	 * like loopback devices.  Handle other special cases here.
	 */
	return isdigit((int)dname[m]) &&
		!_pm_isloop(dname) &&
		!_pm_isramdisk(dname) &&
		!_pm_ismmcdisk(dname) &&
		!_pm_isnvmedrive(dname) &&
		!_pm_iscephrados(dname) &&
		!_pm_isnbd(dname) &&
		!_pm_ismd(dname) &&
		!_pm_isdm(dname) &&
		!_pm_iscdrom(dname);
    }
}

/*
 * return true is arg is an xvm volume name
 */
static int
_pm_isxvmvol(char *dname)
{
    return strstr(dname, "xvm") != NULL;
}

/*
 * return true is arg is a disk name
 */
static int
_pm_isdisk(char *dname)
{
    return (!_pm_isloop(dname) && !_pm_isramdisk(dname) &&
	    !_pm_iscdrom(dname) &&
	    !_pm_ispartition(dname) && !_pm_isxvmvol(dname) &&
	    !_pm_isdm(dname) && !_pm_ismd(dname));
}

static int
refresh_mdadm(const char *name)
{
    char mdadm[MAXPATHLEN];
    char args[] = "--detail --test";
    FILE *pfp;

    if (access(linux_mdadm, R_OK) != 0)
    	return -1;
    pmsprintf(mdadm, sizeof(mdadm), "%s %s /dev/%s 2>&1 >/dev/null",
	linux_mdadm, args, name);	/* discard any/all output */
    mdadm[sizeof(mdadm)-1] = '\0';
    /* popen() is SAFE, command built from literal strings */
    if (!(pfp = popen(mdadm, "r")))
    	return -1;
    return pclose(pfp);
}

static void
refresh_udev(pmInDom disk_indom, pmInDom partitions_indom)
{
    char buf[MAXNAMELEN];
    char realname[MAXNAMELEN];
    char *shortname;
    char *p;
    char *udevname;
    FILE *pfp;
    partitions_entry_t *entry;
    int indom;
    int inst;

    if (access("/dev/xscsi", R_OK) != 0)
    	return;
    /* popen() is SAFE, command is a literal string */
    if (!(pfp = popen("find /dev/xscsi -name disc -o -name part[0-9]*", "r")))
    	return;
    while (fgets(buf, sizeof(buf), pfp)) {
	if ((p = strrchr(buf, '\n')) != NULL)
	    *p = '\0';
	if (realpath(buf, realname)) {
	    udevname = buf + 5; /* /dev/xscsi/.... */
	    if ((shortname = strrchr(realname, '/')) != NULL) {
		shortname++;
		indom = _pm_ispartition(shortname) ?
		    partitions_indom : disk_indom;
		if (pmdaCacheLookupName(indom, shortname, &inst, (void **)&entry) != PMDA_CACHE_ACTIVE)
		    continue;
		entry->udevnamebuf = strdup(udevname);
		pmdaCacheStore(indom, PMDA_CACHE_HIDE, shortname, entry); /* inactive */
		pmdaCacheStore(indom, PMDA_CACHE_ADD, udevname, entry); /* active */
	    }
	}
    }
    pclose(pfp);
}

/*
 * Replace dm-* in namebuf with it's persistent name. This is a symlink in
 * /dev/mapper/something -> ../dm-X where dm-X is currently in namebuf. Some
 * older platforms (e.g. RHEL5) don't have the symlinks, just block devices
 * in /dev/mapper.  On newer kernels, the persistent name mapping is also
 * exported via sysfs, which we use in preference. If this fails we leave
 * the argument namebuf unaltered and return 0.
 */
static int
persistent_dm_name(char *namebuf, int namelen, int devmajor, int devminor)
{
    int fd;
    char *p;
    DIR *dp;
    int found = 0;
    struct dirent *dentry;
    struct stat sb;
    char path[MAXPATHLEN];

    pmsprintf(path, sizeof(path), "%s/sys/block/%s/dm/name", linux_statspath, namebuf);
    if ((fd = open(path, O_RDONLY)) >= 0) {
	memset(path, 0, sizeof(path));
    	if (read(fd, path, sizeof(path)-1) > 0) {
	    if ((p = strchr(path, '\n')) != NULL)
	    	*p = '\0';
	    strncpy(namebuf, path, MIN((sizeof(path)-1), namelen));
	    found = 1;
	}
    	close(fd);
    }

    if (!found) {
	/*
	 * The sysfs name isn't available, so we'll have to walk /dev/mapper
	 * and match up dev_t instead [happens on RHEL5 and maybe elsewhere].
	 */
	pmsprintf(path, sizeof(path), "%s/dev/mapper", linux_statspath);
	if ((dp = opendir(path)) != NULL) {
	    while ((dentry = readdir(dp)) != NULL) {
		pmsprintf(path, sizeof(path),
			"%s/dev/mapper/%s", linux_statspath, dentry->d_name);
		if (stat(path, &sb) != 0 || !S_ISBLK(sb.st_mode))
		    continue; /* only interested in block devices */
		if (devmajor == major(sb.st_rdev) && devminor == minor(sb.st_rdev)) {
		    strncpy(namebuf, dentry->d_name, namelen);
		    found = 1;
		    break;
		}
	    }
	    closedir(dp);
	}
    }

    return found;
}

/*
 * Replace md* in namebuf with its persistent name.  This is a symlink
 * in /dev/md/something -> ../mdX where mdX is currently in namebuf.
 * However, if its just the default entry ("0", "1", ...) skip it.
 */
static int
persistent_md_name(char *namebuf, int namelen)
{
    DIR *dp;
    ssize_t size;
    int found = 0;
    struct dirent *dentry;
    char path[MAXPATHLEN];
    char name[MAXPATHLEN];

    pmsprintf(path, sizeof(path), "%s/dev/md", linux_statspath);
    if ((dp = opendir(path)) != NULL) {
	while ((dentry = readdir(dp)) != NULL) {
	    if (dentry->d_name[0] == '.')
		continue;
	    if (isdigit(dentry->d_name[0]))
		continue;
	    pmsprintf(path, sizeof(path), "%s/dev/md/%s",
			linux_statspath, dentry->d_name);
	    if ((size = readlink(path, name, sizeof(name))) < 0)
		continue;
	    name[size] = '\0';
	    if (strcmp(basename(name), namebuf) == 0) {
		strncpy(namebuf, dentry->d_name, namelen);
		found = 1;
		break;
	    }
	}
	closedir(dp);
    }
    return found;
}

static partitions_entry_t *
refresh_disk_indom(char *namebuf, size_t namelen, int devmaj, int devmin,
		pmInDom disk_indom, pmInDom part_indom,
		pmInDom dm_indom, pmInDom md_indom, int *indom_changes)
{
    int			indom, inst;
    char		*dmname = NULL, *mdname = NULL;
    partitions_entry_t	*p = NULL;

    if (_pm_isdm(namebuf)) {
	indom = dm_indom;
	dmname = strdup(namebuf);
    }
    else if (_pm_ismd(namebuf)) {
	indom = md_indom;
	mdname = strdup(namebuf);
    }
    else if (_pm_ispartition(namebuf))
	indom = part_indom;
    else if (_pm_isdisk(namebuf))
	indom = disk_indom;
    else
	return NULL;

    if (indom == dm_indom) {
	/* replace dm-[0-9]* with the persistent name from /dev/mapper */
	if (!persistent_dm_name(namebuf, namelen, devmaj, devmin)) {
	    /* skip dm devices that have no persistent name mapping */
	    free(dmname);
	    return NULL;
	}
    } else if (indom == md_indom) {
	/* replace md[0-9]* with the persistent name from /dev/md */
	persistent_md_name(namebuf, namelen);
	/* continue with md devices that have no persistent mapping */
    }

    if (pmdaCacheLookupName(indom, namebuf, &inst, (void **)&p) < 0 || !p) {
	/* not found: allocate and add a new entry */
	p = (partitions_entry_t *)calloc(1, sizeof(partitions_entry_t));
	*indom_changes += 1;
    } else {
	if (p->dmname)
	    free(p->dmname);
	if (p->mdname)
	    free(p->mdname);
	p->nr_blocks = 0;		/* zero if not read/needed */
    }

    p->dmname = dmname;		/* NULL if not a DM device */
    p->mdname = mdname;		/* NULL if not a MD device */

    if (!p->namebuf) {
	p->namebuf = strdup(namebuf);
    } else if (strcmp(namebuf, p->namebuf) != 0) {
	free(p->namebuf);
	p->namebuf = strdup(namebuf);
    }

    /* activate and return this entry */
    if (p->udevnamebuf)
	/* long xscsi name */
	pmdaCacheStore(indom, PMDA_CACHE_ADD, p->udevnamebuf, p);
    else
	/* short /proc/diskstats or /proc/partitions name */
	pmdaCacheStore(indom, PMDA_CACHE_ADD, namebuf, p);
    return p;
}

static int
refresh_diskstats(FILE *fp, pmInDom disk_indom, pmInDom part_indom,
			    pmInDom dm_indom, pmInDom md_indom)
{
    int			indom_changes = 0;
    int			devmin, devmaj, n;
    char		buf[MAXPATHLEN];
    char		name[MAXPATHLEN];
    partitions_entry_t	*p;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	/* skip heading */
	if (buf[0] != ' ' || buf[0] == '\n')
	    continue;

	if ((n = sscanf(buf, "%d %d %s", &devmaj, &devmin, name)) != 3)
	    continue;

	if (!(p = refresh_disk_indom(name, sizeof(name), devmaj, devmin,
		disk_indom, part_indom, dm_indom, md_indom, &indom_changes)))
	    continue;

	/* 2.6 style /proc/diskstats */
	name[0] = '\0';
	/* Linux source: block/genhd.c::diskstats_show(1) */
	n = sscanf(buf, "%u %u %s %lu %lu %llu %u %lu %lu %llu %u %u %u %u",
		&p->major, &p->minor, name,
		&p->rd_ios, &p->rd_merges, &p->rd_sectors, &p->rd_ticks,
		&p->wr_ios, &p->wr_merges, &p->wr_sectors, &p->wr_ticks,
		&p->ios_in_flight, &p->io_ticks, &p->aveq);
	if (n != 14) {
	    /*
	     * From 2.6.25 onward, the full set of statistics is
	     * available again for both partitions and disks.
	     */
	    _pm_have_kernel_2_6_partition_stats = 1;
	    p->rd_merges = p->wr_merges = p->wr_ticks =
			p->ios_in_flight = p->io_ticks = p->aveq = 0;
	    /* Linux source: block/genhd.c::diskstats_show(2) */
	    sscanf(buf, "%u %u %s %u %u %u %u\n",
		    &p->major, &p->minor, name,
		    (unsigned int *)&p->rd_ios, (unsigned int *)&p->rd_sectors,
		    (unsigned int *)&p->wr_ios, (unsigned int *)&p->wr_sectors);
	}
    }
    return indom_changes;
}

static int
refresh_partitions(FILE *fp, pmInDom disk_indom, pmInDom part_indom,
			     pmInDom dm_indom, pmInDom md_indom)
{
    int			indom_changes = 0;
    int			devmin, devmaj, n;
    unsigned long long	nop;
    partitions_entry_t	*p;
    char		buf[MAXPATHLEN];
    char		name[MAXPATHLEN];

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	/* skip heading */
	if (buf[0] != ' ' || buf[0] == '\n')
	    continue;

	/* /proc/partitions */
	if ((n = sscanf(buf, "%d %d %llu %s", &devmaj, &devmin, &nop, name)) != 4)
	    continue;

	if (!(p = refresh_disk_indom(name, sizeof(name), devmaj, devmin,
		disk_indom, part_indom, dm_indom, md_indom, &indom_changes)))
	    continue;

	/* 2.4 format /proc/partitions (distro patched) */
	name[0] = '\0';
	sscanf(buf, "%u %u %llu %s %lu %lu %llu %u %lu %lu %llu %u %u %u %u",
		&p->major, &p->minor, &p->nr_blocks, name,
		&p->rd_ios, &p->rd_merges, &p->rd_sectors,
		&p->rd_ticks, &p->wr_ios, &p->wr_merges,
		&p->wr_sectors, &p->wr_ticks, &p->ios_in_flight,
		&p->io_ticks, &p->aveq);
    }
    return indom_changes;
}

int
refresh_proc_partitions(pmInDom disk_indom, pmInDom part_indom,
			pmInDom dm_indom, pmInDom md_indom,
			int need_diskstats, int need_partitions)
{
    FILE	*fp;
    int		indom_changes = 0;
    char	buf[MAXPATHLEN];
    static int	first = 1;

    if (first) {
	/* initialize the instance domain caches */
	pmdaCacheOp(disk_indom, PMDA_CACHE_LOAD);
	pmdaCacheOp(part_indom, PMDA_CACHE_LOAD);
	pmdaCacheOp(dm_indom, PMDA_CACHE_LOAD);
	pmdaCacheOp(md_indom, PMDA_CACHE_LOAD);
	indom_changes = 1;
	first = 0;
    }

    pmdaCacheOp(disk_indom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(part_indom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(dm_indom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(md_indom, PMDA_CACHE_INACTIVE);

    /* 2.6 style disk stats */
    if (need_diskstats) {
	if ((fp = linux_statsfile("/proc/diskstats", buf, sizeof(buf)))) {
	    indom_changes += refresh_diskstats(fp, disk_indom, part_indom,
						dm_indom, md_indom);
	    fclose(fp);
	} else {
	    need_partitions = 1;
	}
    }

    /* 2.4 style disk stats *and* device capacity */
    if (need_partitions) {
	if ((fp = linux_statsfile("/proc/partitions", buf, sizeof(buf)))) {
	    indom_changes += refresh_partitions(fp, disk_indom, part_indom,
						dm_indom, md_indom);
	    fclose(fp);
	}
    }

    /*
     * If any new disks or partitions have appeared then we
     * we need to remap the long device names (if /dev/xscsi
     * exists) and then flush the pmda cache.
     *
     * We just let inactive instances rot in the inactive state
     * (this doesn't happen very often, so is only a minor leak).
     */
    if (indom_changes) {
	refresh_udev(disk_indom, part_indom);
	pmdaCacheOp(disk_indom, PMDA_CACHE_SAVE);
	pmdaCacheOp(part_indom, PMDA_CACHE_SAVE);
	pmdaCacheOp(dm_indom, PMDA_CACHE_SAVE);
	pmdaCacheOp(md_indom, PMDA_CACHE_SAVE);
    }

    return 0;
}

/*
 * This table must always match the definitions in root_linux
 * and metrictab[] in pmda.c
 */
static pmID disk_metric_table[] = {
    /* disk.dev.read */		     PMDA_PMID(CLUSTER_STAT,4),
    /* disk.dev.write */	     PMDA_PMID(CLUSTER_STAT,5),
    /* disk.dev.total */	     PMDA_PMID(CLUSTER_STAT,28),
    /* disk.dev.blkread */	     PMDA_PMID(CLUSTER_STAT,6),
    /* disk.dev.blkwrite */	     PMDA_PMID(CLUSTER_STAT,7),
    /* hinv.ndisk */                 PMDA_PMID(CLUSTER_STAT,33),
    /* disk.dev.blktotal */	     PMDA_PMID(CLUSTER_STAT,36),
    /* disk.dev.read_bytes */	     PMDA_PMID(CLUSTER_STAT,38),
    /* disk.dev.write_bytes */	     PMDA_PMID(CLUSTER_STAT,39),
    /* disk.dev.total_bytes */	     PMDA_PMID(CLUSTER_STAT,40),
    /* disk.dev.read_merge */	     PMDA_PMID(CLUSTER_STAT,49),
    /* disk.dev.write_merge */	     PMDA_PMID(CLUSTER_STAT,50),
    /* disk.dev.avactive */	     PMDA_PMID(CLUSTER_STAT,46),
    /* disk.dev.aveq */		     PMDA_PMID(CLUSTER_STAT,47),
    /* disk.dev.scheduler */	     PMDA_PMID(CLUSTER_STAT,59),
    /* disk.dev.read_rawactive */    PMDA_PMID(CLUSTER_STAT,72),
    /* disk.dev.write_rawactive	*/   PMDA_PMID(CLUSTER_STAT,73),
    /* disk.dev.total_rawactive	*/   PMDA_PMID(CLUSTER_STAT,79),
    /* disk.dev.capacity */	     PMDA_PMID(CLUSTER_STAT,87),

    /* disk.all.read */		     PMDA_PMID(CLUSTER_STAT,24),
    /* disk.all.write */	     PMDA_PMID(CLUSTER_STAT,25),
    /* disk.all.total */	     PMDA_PMID(CLUSTER_STAT,29),
    /* disk.all.blkread */	     PMDA_PMID(CLUSTER_STAT,26),
    /* disk.all.blkwrite */	     PMDA_PMID(CLUSTER_STAT,27),
    /* disk.all.blktotal */	     PMDA_PMID(CLUSTER_STAT,37),
    /* disk.all.read_bytes */	     PMDA_PMID(CLUSTER_STAT,41),
    /* disk.all.write_bytes */	     PMDA_PMID(CLUSTER_STAT,42),
    /* disk.all.total_bytes */	     PMDA_PMID(CLUSTER_STAT,43),
    /* disk.all.read_merge */	     PMDA_PMID(CLUSTER_STAT,51),
    /* disk.all.write_merge */	     PMDA_PMID(CLUSTER_STAT,52),
    /* disk.all.avactive */	     PMDA_PMID(CLUSTER_STAT,44),
    /* disk.all.aveq */		     PMDA_PMID(CLUSTER_STAT,45),
    /* disk.all.read_rawactive */    PMDA_PMID(CLUSTER_STAT,74),
    /* disk.all.write_rawactive	*/   PMDA_PMID(CLUSTER_STAT,75),
    /* disk.all.total_rawactive	*/   PMDA_PMID(CLUSTER_STAT,80),

    /* disk.partitions.read */	     PMDA_PMID(CLUSTER_PARTITIONS,0),
    /* disk.partitions.write */	     PMDA_PMID(CLUSTER_PARTITIONS,1),
    /* disk.partitions.total */	     PMDA_PMID(CLUSTER_PARTITIONS,2),
    /* disk.partitions.blkread */    PMDA_PMID(CLUSTER_PARTITIONS,3),
    /* disk.partitions.blkwrite */   PMDA_PMID(CLUSTER_PARTITIONS,4),
    /* disk.partitions.blktotal */   PMDA_PMID(CLUSTER_PARTITIONS,5),
    /* disk.partitions.read_bytes */ PMDA_PMID(CLUSTER_PARTITIONS,6),
    /* disk.partitions.write_bytes */PMDA_PMID(CLUSTER_PARTITIONS,7),
    /* disk.partitions.total_bytes */PMDA_PMID(CLUSTER_PARTITIONS,8),
    /* disk.partitions.read_merge */ PMDA_PMID(CLUSTER_PARTITIONS,9),
    /* disk.partitions.write_merge */PMDA_PMID(CLUSTER_PARTITIONS,10),
    /* disk.partitions.avactive */   PMDA_PMID(CLUSTER_PARTITIONS,11),
    /* disk.partitions.aveq */       PMDA_PMID(CLUSTER_PARTITIONS,12),
    /* disk.partitions.read_rawactive */  PMDA_PMID(CLUSTER_PARTITIONS,13),
    /* disk.partitions.write_rawactive */ PMDA_PMID(CLUSTER_PARTITIONS,14),
    /* disk.partitions.total_rawactive */ PMDA_PMID(CLUSTER_PARTITIONS,15),
    /* disk.partitions.capacity */   PMDA_PMID(CLUSTER_PARTITIONS,16),

    /* disk.dm.read */               PMDA_PMID(CLUSTER_DM,0),
    /* disk.dm.write */		     PMDA_PMID(CLUSTER_DM,1),
    /* disk.dm.total */		     PMDA_PMID(CLUSTER_DM,2),
    /* disk.dm.blkread */	     PMDA_PMID(CLUSTER_DM,3),
    /* disk.dm.blkwrite */	     PMDA_PMID(CLUSTER_DM,4),
    /* disk.dm.blktotal */	     PMDA_PMID(CLUSTER_DM,5),
    /* disk.dm.read_bytes */         PMDA_PMID(CLUSTER_DM,6),
    /* disk.dm.write_bytes */        PMDA_PMID(CLUSTER_DM,7),
    /* disk.dm.total_bytes */        PMDA_PMID(CLUSTER_DM,8),
    /* disk.dm.read_merge */	     PMDA_PMID(CLUSTER_DM,9),
    /* disk.dm.write_merge */	     PMDA_PMID(CLUSTER_DM,10),
    /* disk.dm.avactive */	     PMDA_PMID(CLUSTER_DM,11),
    /* disk.dm.aveq */		     PMDA_PMID(CLUSTER_DM,12),
    /* hinv.map.dmname */	     PMDA_PMID(CLUSTER_DM,13),
    /* disk.dm.read_rawactive */     PMDA_PMID(CLUSTER_DM,14),
    /* disk.dm.write_rawactive */    PMDA_PMID(CLUSTER_DM,15),
    /* disk.dm.total_rawactive */    PMDA_PMID(CLUSTER_DM,16),
    /* disk.dm.capacity */	     PMDA_PMID(CLUSTER_DM,17),

    /* disk.md.read */               PMDA_PMID(CLUSTER_MD,0),
    /* disk.md.write */		     PMDA_PMID(CLUSTER_MD,1),
    /* disk.md.total */		     PMDA_PMID(CLUSTER_MD,2),
    /* disk.md.blkread */	     PMDA_PMID(CLUSTER_MD,3),
    /* disk.md.blkwrite */	     PMDA_PMID(CLUSTER_MD,4),
    /* disk.md.blktotal */	     PMDA_PMID(CLUSTER_MD,5),
    /* disk.md.read_bytes */         PMDA_PMID(CLUSTER_MD,6),
    /* disk.md.write_bytes */        PMDA_PMID(CLUSTER_MD,7),
    /* disk.md.total_bytes */        PMDA_PMID(CLUSTER_MD,8),
    /* disk.md.read_merge */	     PMDA_PMID(CLUSTER_MD,9),
    /* disk.md.write_merge */	     PMDA_PMID(CLUSTER_MD,10),
    /* disk.md.avactive */	     PMDA_PMID(CLUSTER_MD,11),
    /* disk.md.aveq */		     PMDA_PMID(CLUSTER_MD,12),
    /* hinv.map.dmname */	     PMDA_PMID(CLUSTER_MD,13),
    /* disk.md.read_rawactive */     PMDA_PMID(CLUSTER_MD,14),
    /* disk.md.write_rawactive */    PMDA_PMID(CLUSTER_MD,15),
    /* disk.md.total_rawactive */    PMDA_PMID(CLUSTER_MD,16),
    /* disk.md.capacity */	     PMDA_PMID(CLUSTER_MD,17),
};

int
is_partitions_metric(pmID full_pmid)
{
    int			i;
    static pmID		*p = NULL;
    pmID		pmid;
    int			n = sizeof(disk_metric_table) / sizeof(disk_metric_table[0]);

    pmid = PMDA_PMID(pmID_cluster(full_pmid), pmID_item(full_pmid));

    /* fast test for same matched metric as last time */
    if (p && *p == pmid)
    	return 1;

    for (p = disk_metric_table, i=0; i < n; i++, p++) {
    	if (*p == pmid)
	    return 1;
    }

    /* no match, clear fast test state */
    p = NULL;
    return 0;
}

int
is_capacity_metric(int cluster, int item)
{
    /* test for disk.{dev,dm,md,partitions}.capacity metrics */
    if (item == 87 && cluster == CLUSTER_STAT)
	return 1;
    if (item == 16 && cluster == CLUSTER_PARTITIONS)
	return 1;
    if (item == 17 && (cluster == CLUSTER_DM || cluster == CLUSTER_MD))
	return 1;
    return 0;
}

char *
_pm_ioscheduler(const char *device)
{
    FILE *fp;
    char *p, *q;
    static char buf[1024];
    char path[MAXNAMELEN];

    /*
     * Extract scheduler from /sys/block/<device>/queue/scheduler.
     *     File format: "noop anticipatory [deadline] cfq"
     * In older kernels (incl. RHEL5 and SLES10) this doesn't exist,
     * but we can still look in /sys/block/<device>/queue/iosched to
     * intuit the ones we know about (cfq, deadline, as, noop) based
     * on the different status files they create.
     */
    pmsprintf(path, sizeof(path), "%s/sys/block/%s/queue/scheduler",
		    linux_statspath, device);
    if ((fp = fopen(path, "r")) != NULL) {
	p = fgets(buf, sizeof(buf), fp);
	fclose(fp);
	if (!p)
	    goto unknown;
	for (p = q = buf; p && *p && *p != ']'; p++) {
	    if (*p == '[')
		q = p+1;
	}
	if (q == buf)
	    goto unknown;
	if (*p != ']')
	    goto unknown;
	*p = '\0';
	return q;
    }
    else {
#define BLKQUEUE	"%s/sys/block/%s/queue/"
	/* sniff around, maybe we'll get lucky and find something */
	pmsprintf(path, sizeof(path), BLKQUEUE "iosched/quantum",
			linux_statspath, device);
	if (access(path, F_OK) == 0)
	    return "cfq";
	pmsprintf(path, sizeof(path), BLKQUEUE "iosched/fifo_batch",
			linux_statspath, device);
	if (access(path, F_OK) == 0)
	    return "deadline";
	pmsprintf(path, sizeof(path), BLKQUEUE "iosched/antic_expire",
			linux_statspath, device);
	if (access(path, F_OK) == 0)
	    return "anticipatory";
	/* punt.  noop has no files to match on ... */
	pmsprintf(path, sizeof(path), BLKQUEUE "iosched",
			linux_statspath, device);
	if (access(path, F_OK) == 0)
	    return "noop";
	/* else fall though ... */
#undef BLKQUEUE
    }

unknown:
    return "unknown";
}

int
proc_partitions_fetch(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    unsigned int	cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int	item = pmID_item(mdesc->m_desc.pmid);
    int                 i;
    partitions_entry_t	*p = NULL;

    if (inst != PM_IN_NULL) {
	if (pmdaCacheLookup(mdesc->m_desc.indom, inst, NULL, (void **)&p) < 0)
	    return PM_ERR_INST;
    }

    switch (cluster) {
    case CLUSTER_STAT:
	/*
	 * disk.{dev,all} remain in CLUSTER_STAT for backward compatibility
	 */
	switch(item) {
	case 4: /* disk.dev.read */
	    if (p == NULL)
		return PM_ERR_INST;
	    _pm_assign_ulong(atom, p->rd_ios);
	    break;
	case 5: /* disk.dev.write */
	    if (p == NULL)
		return PM_ERR_INST;
	    _pm_assign_ulong(atom, p->wr_ios);
	    break;
	case 6: /* disk.dev.blkread */
	    if (p == NULL)
		return PM_ERR_INST;
	    _pm_assign_ulong(atom, p->rd_sectors);
	    break;
	case 7: /* disk.dev.blkwrite */
	    if (p == NULL)
		return PM_ERR_INST;
	    _pm_assign_ulong(atom, p->wr_sectors);
	    break;
	case 28: /* disk.dev.total */
	    if (p == NULL)
		return PM_ERR_INST;
	    atom->ull = p->rd_ios + p->wr_ios;
	    break;
	case 36: /* disk.dev.blktotal */
	    if (p == NULL)
		return PM_ERR_INST;
	    atom->ull = p->rd_sectors + p->wr_sectors;
	    break;
	case 38: /* disk.dev.read_bytes */
	    if (p == NULL)
		return PM_ERR_INST;
	    atom->ul = p->rd_sectors / 2;
	    break;
	case 39: /* disk.dev.write_bytes */
	    if (p == NULL)
		return PM_ERR_INST;
	    atom->ul = p->wr_sectors / 2;
	    break;
	case 40: /* disk.dev.total_bytes */
	    if (p == NULL)
		return PM_ERR_INST;
	    atom->ul = (p->rd_sectors + p->wr_sectors) / 2;
	    break;
	case 46: /* disk.dev.avactive ... already msec from /proc/diskstats */
	    if (p == NULL)
		return PM_ERR_INST;
	    atom->ul = p->io_ticks;
	    break;
	case 47: /* disk.dev.aveq ... already msec from /proc/diskstats */
	    if (p == NULL)
		return PM_ERR_INST;
	    atom->ul = p->aveq;
	    break;
	case 49: /* disk.dev.read_merge */
	    if (p == NULL)
		return PM_ERR_INST;
	    _pm_assign_ulong(atom, p->rd_merges);
	    break;
	case 50: /* disk.dev.write_merge */
	    if (p == NULL)
		return PM_ERR_INST;
	    _pm_assign_ulong(atom, p->wr_merges);
	    break;
	case 59: /* disk.dev.scheduler */
	    if (p == NULL)
		return PM_ERR_INST;
	    atom->cp = _pm_ioscheduler(p->namebuf);
	    break;
	case 72: /* disk.dev.read_rawactive already ms from /proc/diskstats */
	    if (p == NULL)
		return PM_ERR_INST;
	    atom->ul = p->rd_ticks;
	    break;
	case 73: /* disk.dev.write_rawactive already ms from /proc/diskstats */
	    if (p == NULL)
		return PM_ERR_INST;
	    atom->ul = p->wr_ticks;
	    break;
	case 79: /* disk.dev.total_rawactive already ms from /proc/diskstats */
	    if (p == NULL)
		return PM_ERR_INST;
	    atom->ul = p->rd_ticks + p->wr_ticks;
	    break;
	case 87: /* disk.dev.capacity already kb from /proc/partitions */
	    if (p == NULL)
		return PM_ERR_INST;
	    if (!p->nr_blocks)
		return 0;
	    atom->ull = p->nr_blocks;
	    break;
	default:
	    /* disk.all.* is a singular instance domain */
	    atom->ull = 0;
	    for (pmdaCacheOp(INDOM(DISK_INDOM), PMDA_CACHE_WALK_REWIND);;) {
	        if ((i = pmdaCacheOp(INDOM(DISK_INDOM), PMDA_CACHE_WALK_NEXT)) < 0)
		    break;
		if (!pmdaCacheLookup(INDOM(DISK_INDOM), i, NULL, (void **)&p) || !p)
		    continue;
		switch (item) {
		case 24: /* disk.all.read */
		    atom->ull += p->rd_ios;
		    break;
		case 25: /* disk.all.write */
		    atom->ull += p->wr_ios;
		    break;
		case 26: /* disk.all.blkread */
		    atom->ull += p->rd_sectors;
		    break;
		case 27: /* disk.all.blkwrite */
		    atom->ull += p->wr_sectors;
		    break;
		case 29: /* disk.all.total */
		    atom->ull += p->rd_ios + p->wr_ios;
		    break;
		case 37: /* disk.all.blktotal */
		    atom->ull += p->rd_sectors + p->wr_sectors;
		    break;
		case 41: /* disk.all.read_bytes */
		    atom->ul += p->rd_sectors / 2;
		    break;
		case 42: /* disk.all.write_bytes */
		    atom->ul += p->wr_sectors / 2;
		    break;
		case 43: /* disk.all.total_bytes */
		    atom->ul += (p->rd_sectors + p->wr_sectors) / 2;
		    break;
		case 44: /* disk.all.avactive ... already msec from /proc/diskstats */
		    atom->ull += p->io_ticks;
		    break;
		case 45: /* disk.all.aveq ... already msec from /proc/diskstats */
		    atom->ull += p->aveq;
		    break;
		case 51: /* disk.all.read_merge */
		    atom->ull += p->rd_merges;
		    break;
		case 52: /* disk.all.write_merge */
		    atom->ull += p->wr_merges;
		    break;
		case 74: /* disk.all.read_rawactive ... already msec from /proc/diskstats */
		    atom->ull += p->rd_ticks;
		    break;
		case 75: /* disk.all.write_rawactive ... already msec from /proc/diskstats */
		    atom->ull += p->wr_ticks;
		    break;
		case 80: /* disk.all.total_rawactive ... already msec from /proc/diskstats */
		    atom->ull += p->rd_ticks + p->wr_ticks;
		    break;
		default:
		    return PM_ERR_PMID;
		}
	    } /* loop */
	}
	break;

    case CLUSTER_PARTITIONS:
	if (p == NULL)
	    return PM_ERR_INST;
	switch(item) {
	    /* disk.partitions */
	    case 0: /* disk.partitions.read */
		_pm_assign_ulong(atom, p->rd_ios);
		break;
	    case 1: /* disk.partitions.write */
		_pm_assign_ulong(atom, p->wr_ios);
		break;
	    case 2: /* disk.partitions.total */
		atom->ul = p->wr_ios + p->rd_ios;
		break;
	    case 3: /* disk.partitions.blkread */
		_pm_assign_ulong(atom, p->rd_sectors);
		break;
	    case 4: /* disk.partitions.blkwrite */
		_pm_assign_ulong(atom, p->wr_sectors);
		break;
	    case 5: /* disk.partitions.blktotal */
		atom->ul = p->rd_sectors + p->wr_sectors;
		break;
	    case 6: /* disk.partitions.read_bytes */
		atom->ul = p->rd_sectors / 2;
		break;
	    case 7: /* disk.partitions.write_bytes */
		atom->ul = p->wr_sectors / 2;
		break;
	    case 8: /* disk.partitions.total_bytes */
		atom->ul = (p->rd_sectors +
			   p->wr_sectors) / 2;
		break;
            case 9: /* disk.partitions.read_merge */
                if (_pm_have_kernel_2_6_partition_stats)
                    return PM_ERR_APPVERSION; /* no read_merge for partition in 2.6 */
                else
                    _pm_assign_ulong(atom, p->rd_merges);
                break;
            case 10: /* disk.partitions.write_merge */
                if (_pm_have_kernel_2_6_partition_stats)
                    return PM_ERR_APPVERSION; /* no write_merge for partition in 2.6 */
                else
                    _pm_assign_ulong(atom, p->wr_merges);
                break;
            case 11: /* disk.partitions.avactive */
                if (_pm_have_kernel_2_6_partition_stats)
                    return PM_ERR_APPVERSION; /* no avactive for partition in 2.6 */
                else
                    atom->ul = p->io_ticks;
                break;
            case 12: /* disk.partitions.aveq */
                if (_pm_have_kernel_2_6_partition_stats)
                    return PM_ERR_APPVERSION; /* no aveq for partition in 2.6 */
                else
                    atom->ul = p->aveq;
                break;
            case 13: /* disk.partitions.read_rawactive */
                if (_pm_have_kernel_2_6_partition_stats)
                    return PM_ERR_APPVERSION; /* no read_rawactive for partition in 2.6 */
                else
                    atom->ul = p->rd_ticks;
                break;
            case 14: /* disk.partitions.write_rawactive */
                if (_pm_have_kernel_2_6_partition_stats)
                    return PM_ERR_APPVERSION; /* no write_rawactive for partition in 2.6 */
                else
                    atom->ul = p->wr_ticks;
                break;
            case 15: /* disk.partitions.total_rawactive */
                if (_pm_have_kernel_2_6_partition_stats)
                    return PM_ERR_APPVERSION; /* no read_rawactive or write_rawactive for partition in 2.6 */
                else
                    atom->ul = p->rd_ticks + p->wr_ticks;
                break;
	    case 16: /* disk.partitions.capacity */
		if (!p->nr_blocks)
		    return 0;
		atom->ull = p->nr_blocks;
		break;
	    default:
		return PM_ERR_PMID;
	}
	break;

    case CLUSTER_DM:
    case CLUSTER_MD:
	if (p == NULL)
	    return PM_ERR_INST;
	switch(item) {
	case 0: /* disk.{dm,md}.read */
	    _pm_assign_ulong(atom, p->rd_ios);
	    break;
	case 1: /* disk.{dm,md}.write */
	    _pm_assign_ulong(atom, p->wr_ios);
	    break;
	case 2: /* disk.{dm,md}.total */
	    atom->ull = p->rd_ios + p->wr_ios;
	    break;
	case 3: /* disk.{dm,md}.blkread */
	    _pm_assign_ulong(atom, p->rd_sectors);
	    break;
	case 4: /* disk.{dm,md}.blkwrite */
	    _pm_assign_ulong(atom, p->wr_sectors);
	    break;
	case 5: /* disk.{dm,md}.blktotal */
	    atom->ull = p->rd_sectors + p->wr_sectors;
	    break;
	case 6: /* disk.{dm,md}.read_bytes */
	    atom->ul = p->rd_sectors / 2;
	    break;
	case 7: /* disk.{dm,md}.write_bytes */
	    atom->ul = p->wr_sectors / 2;
	    break;
	case 8: /* disk.{dm,md}.total_bytes */
	    atom->ul = (p->rd_sectors + p->wr_sectors) / 2;
	    break;
	case 9: /* disk.{dm,md}.read_merge */
	    _pm_assign_ulong(atom, p->rd_merges);
	    break;
	case 10: /* disk.{dm,md}.write_merge */
	    _pm_assign_ulong(atom, p->wr_merges);
	    break;
	case 11: /* disk.{dm,md}.avactive */
	    atom->ul = p->io_ticks;
	    break;
	case 12: /* disk.{dm,md}.aveq */
	    atom->ul = p->aveq;
	    break;
	case 13: /* hinv.map.{dm,md}name */
	    atom->cp = (cluster == CLUSTER_DM) ? p->dmname : p->mdname;
	    break;
	case 14: /* disk.{dm,md}.read_rawactive */
	    atom->ul = p->rd_ticks;
	    break;
	case 15: /* disk.{dm,md}.write_rawactive */
	    atom->ul = p->wr_ticks;
	    break;
	case 16: /* disk.{dm,md}.total_rawactive */
	    atom->ul = p->rd_ticks + p->wr_ticks;
	    break;
	case 17: /* disk.{dm,md}.capacity */
	    if (!p->nr_blocks)
		return 0;
	    atom->ull = p->nr_blocks;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_MDADM:
	if (p == NULL)
	    return PM_ERR_INST;
	switch(item) {
	case 0: /* disk.md.status */
	    atom->l = refresh_mdadm(p->namebuf);
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    default: /* switch cluster */
	return PM_ERR_PMID;
    }

    return 1;
}
