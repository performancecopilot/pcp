/*
 * Linux Partitions (disk and disk partition IO stats) Cluster
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
 */

#include <ctype.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "convert.h"
#include "clusters.h"
#include "indom.h"
#include "proc_partitions.h"

extern int _pm_numdisks;

/*
 * _pm_ispartition : return true if arg is a partition name
 *                   return false if arg is a disk name
 * ide disks are named e.g. hda
 * ide partitions are named e.g. hda1
 *
 * scsi disks are named e.g. sda
 * scsi partitions are named e.g. sda1
 *
 * devfs scsi disks are named e.g. scsi/host0/bus0/target1/lun0/disc
 * devfs scsi partitions are named e.g. scsi/host0/bus0/target1/lun0/part1
 *
 * Mylex raid disks are named e.g. rd/c0d0 or dac960/c0d0
 * Mylex raid partitions are named e.g. rd/c0d0p1 or dac960/c0d0p1
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
	 * default test : partition names end in a digit do not
	 * look like loopback devices.  Handle other special-cases
	 * here - mostly seems to be RAM-type disk drivers that're
	 * choosing to end device names with numbers.
	 */
	return isdigit((int)dname[m]) &&
		!_pm_isloop(dname) &&
		!_pm_isramdisk(dname) &&
		!_pm_ismmcdisk(dname);
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
    return !_pm_isloop(dname) && !_pm_isramdisk(dname) && !_pm_ispartition(dname) && !_pm_isxvmvol(dname);
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

int
refresh_proc_partitions(pmInDom disk_indom, pmInDom partitions_indom)
{
    char buf[1024];
    char namebuf[1024];
    FILE *fp;
    int devmin;
    int devmaj;
    int n;
    int indom;
    int have_proc_diskstats;
    int inst;
    unsigned long long blocks;
    partitions_entry_t *p;
    int indom_changes = 0;
    static int first = 1;

    if (first) {
	/* initialize the instance domain caches */
	pmdaCacheOp(disk_indom, PMDA_CACHE_LOAD);
	pmdaCacheOp(partitions_indom, PMDA_CACHE_LOAD);

	first = 0;
	indom_changes = 1;
    }

    pmdaCacheOp(disk_indom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(partitions_indom, PMDA_CACHE_INACTIVE);

    if ((fp = fopen("/proc/diskstats", "r")) != (FILE *)NULL)
    	/* 2.6 style disk stats */
	have_proc_diskstats = 1;
    else {
	if ((fp = fopen("/proc/partitions", "r")) != (FILE *)NULL)
	    have_proc_diskstats = 0;
	else
	    return -oserror();
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (buf[0] != ' ' || buf[0] == '\n') {
	    /* skip heading */
	    continue;
	}

	if (have_proc_diskstats) {
	    if ((n = sscanf(buf, "%d %d %s", &devmaj, &devmin, namebuf)) != 3)
		continue;
	}
	else {
	    /* /proc/partitions */
	    if ((n = sscanf(buf, "%d %d %llu %s", &devmaj, &devmin, &blocks, namebuf)) != 4)
		continue;
	}

	if (_pm_ispartition(namebuf))
	    indom = partitions_indom;
	else if (_pm_isdisk(namebuf))
	    indom = disk_indom;
	else
	    continue;

	p = NULL;
	if (pmdaCacheLookupName(indom, namebuf, &inst, (void **)&p) < 0 || !p) {
	    /* not found: allocate and add a new entry */
	    p = (partitions_entry_t *)malloc(sizeof(partitions_entry_t));
	    memset(p, 0, sizeof(partitions_entry_t));
	    indom_changes++;
	}

	/* activate this entry */
	if (p->udevnamebuf)
	    /* long xscsi name */
	    inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, p->udevnamebuf, p);
	else
	    /* short /proc/diskstats or /proc/partitions name */
	    inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, namebuf, p);

	if (have_proc_diskstats) {
	    /* 2.6 style /proc/diskstats */
	    p->nr_blocks = 0;
	    namebuf[0] = '\0';
	    /* Linux source: block/genhd.c::diskstats_show(1) */
	    n = sscanf(buf, "%u %u %s %lu %lu %llu %u %lu %lu %llu %u %u %u %u",
		&p->major, &p->minor, namebuf,
		&p->rd_ios, &p->rd_merges, &p->rd_sectors, &p->rd_ticks,
		&p->wr_ios, &p->wr_merges, &p->wr_sectors, &p->wr_ticks,
		&p->ios_in_flight, &p->io_ticks, &p->aveq);
	    if (n != 14) {
		p->rd_merges = p->wr_merges = p->wr_ticks =
			p->ios_in_flight = p->io_ticks = p->aveq = 0;
		/* Linux source: block/genhd.c::diskstats_show(2) */
		n = sscanf(buf, "%u %u %s %u %u %u %u\n",
		    &p->major, &p->minor, namebuf,
		    (unsigned int *)&p->rd_ios, (unsigned int *)&p->rd_sectors,
		    (unsigned int *)&p->wr_ios, (unsigned int *)&p->wr_sectors);
	    }
	}
	else {
	    /* 2.4 style /proc/partitions */
	    namebuf[0] = '\0';
	    n = sscanf(buf, "%u %u %lu %s %lu %lu %llu %u %lu %lu %llu %u %u %u %u",
		&p->major, &p->minor, &p->nr_blocks, namebuf,
		&p->rd_ios, &p->rd_merges, &p->rd_sectors,
		&p->rd_ticks, &p->wr_ios, &p->wr_merges,
		&p->wr_sectors, &p->wr_ticks, &p->ios_in_flight,
		&p->io_ticks, &p->aveq);
	}

	if (!p->namebuf)
	    p->namebuf = strdup(namebuf);
	else
	if (strcmp(namebuf, p->namebuf) != 0) {
	    free(p->namebuf);
	    p->namebuf = strdup(namebuf);
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
	refresh_udev(disk_indom, partitions_indom);
	pmdaCacheOp(disk_indom, PMDA_CACHE_SAVE);
	pmdaCacheOp(partitions_indom, PMDA_CACHE_SAVE);
    }

    /*
     * success
     */
    if (fp)
	fclose(fp);
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

    /* disk.partitions.read */	     PMDA_PMID(CLUSTER_PARTITIONS,0),
    /* disk.partitions.write */	     PMDA_PMID(CLUSTER_PARTITIONS,1),
    /* disk.partitions.total */	     PMDA_PMID(CLUSTER_PARTITIONS,2),
    /* disk.partitions.blkread */    PMDA_PMID(CLUSTER_PARTITIONS,3),
    /* disk.partitions.blkwrite */   PMDA_PMID(CLUSTER_PARTITIONS,4),
    /* disk.partitions.blktotal */   PMDA_PMID(CLUSTER_PARTITIONS,5),
    /* disk.partitions.read_bytes */ PMDA_PMID(CLUSTER_PARTITIONS,6),
    /* disk.partitions.write_bytes */PMDA_PMID(CLUSTER_PARTITIONS,7),
    /* disk.partitions.total_bytes */PMDA_PMID(CLUSTER_PARTITIONS,8),

    /* hinv.ndisk */                 PMDA_PMID(CLUSTER_STAT,33),
};

int
is_partitions_metric(pmID full_pmid)
{
    int			i;
    static pmID		*p = NULL;
    __pmID_int          *idp = (__pmID_int *)&(full_pmid);
    pmID		pmid = PMDA_PMID(idp->cluster, idp->item);
    int			n = sizeof(disk_metric_table) / sizeof(disk_metric_table[0]);

    if (p && *p == PMDA_PMID(idp->cluster, idp->item))
    	return 1;
    for (p = disk_metric_table, i=0; i < n; i++, p++) {
    	if (*p == pmid)
	    return 1;
    }
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
    sprintf(path, "/sys/block/%s/queue/scheduler", device);
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
	/* sniff around, maybe we'll get lucky and find something */
	sprintf(path, "/sys/block/%s/queue/iosched/quantum", device);
	if (access(path, F_OK) == 0)
	    return "cfq";
	sprintf(path, "/sys/block/%s/queue/iosched/fifo_batch", device);
	if (access(path, F_OK) == 0)
	    return "deadline";
	sprintf(path, "/sys/block/%s/queue/iosched/antic_expire", device);
	if (access(path, F_OK) == 0)
	    return "anticipatory";
	/* punt.  noop has no files to match on ... */
	sprintf(path, "/sys/block/%s/queue/iosched", device);
	if (access(path, F_OK) == 0)
	    return "noop";
	/* else fall though ... */
    }

unknown:
    return "unknown";
}

int
proc_partitions_fetch(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int          *idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int                 i;
    partitions_entry_t	*p = NULL;

    if (inst != PM_IN_NULL) {
	if (pmdaCacheLookup(mdesc->m_desc.indom, inst, NULL, (void **)&p) < 0)
	    return PM_ERR_INST;
    }

    switch (idp->cluster) {
    case CLUSTER_STAT:
	/*
	 * disk.{dev,all} remain in CLUSTER_STAT for backward compatibility
	 */
	switch(idp->item) {
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
	    atom->ull = p->rd_sectors;
	    break;
	case 7: /* disk.dev.blkwrite */
	    if (p == NULL)
		return PM_ERR_INST;
	    atom->ull = p->wr_sectors;
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
	    atom->ull = p->rd_sectors / 2;
	    break;
	case 39: /* disk.dev.write_bytes */
	    if (p == NULL)
		return PM_ERR_INST;
	    atom->ull = p->wr_sectors / 2;
	    break;
	case 40: /* disk.dev.total_bytes */
	    if (p == NULL)
		return PM_ERR_INST;
	    atom->ull = (p->rd_sectors + p->wr_sectors) / 2;
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
	default:
	    /* disk.all.* is a singular instance domain */
	    atom->ull = 0;
	    for (pmdaCacheOp(INDOM(DISK_INDOM), PMDA_CACHE_WALK_REWIND);;) {
	        if ((i = pmdaCacheOp(INDOM(DISK_INDOM), PMDA_CACHE_WALK_NEXT)) < 0)
		    break;
		if (!pmdaCacheLookup(INDOM(DISK_INDOM), i, NULL, (void **)&p) || !p)
		    continue;
		switch (idp->item) {
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
		    atom->ull += p->rd_sectors / 2;
		    break;
		case 42: /* disk.all.write_bytes */
		    atom->ull += p->wr_sectors / 2;
		    break;
		case 43: /* disk.all.total_bytes */
		    atom->ull += (p->rd_sectors + p->wr_sectors) / 2;
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
		default:
		    return PM_ERR_PMID;
		}
	    } /* loop */
	}
	break;

    case CLUSTER_PARTITIONS:
	if (p == NULL)
	    return PM_ERR_INST;
	switch(idp->item) {
	    /* disk.partitions */
	    case 0: /* disk.partitions.read */
		atom->ul = p->rd_ios;
		break;
	    case 1: /* disk.partitions.write */
		atom->ul = p->wr_ios;
		break;
	    case 2: /* disk.partitions.total */
		atom->ul = p->wr_ios +
			   p->rd_ios;
		break;
	    case 3: /* disk.partitions.blkread */
		atom->ul = p->rd_sectors;
		break;
	    case 4: /* disk.partitions.blkwrite */
		atom->ul = p->wr_sectors;
		break;
	    case 5: /* disk.partitions.blktotal */
		atom->ul = p->rd_sectors +
			   p->wr_sectors;
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
	    default:
	    return PM_ERR_PMID;
	}
	break;

    default: /* switch cluster */
	return PM_ERR_PMID;
    }

    return 1;
}
