/*
 * Linux Filesystem Cluster
 *
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "filesys.h"

/* after pmapi.h */
#ifdef HAVE_LINUX_SPINLOCK_H
#include <linux/spinlock.h>
#else
typedef struct { } spinlock_t;
#endif
#include <linux/quota.h>
#include <linux/dqblk_xfs.h>

#define PRJQUOTA 2


static void 
project_quotas(pmInDom qindom, filesys_t *fs)
{
#define PROJBUFSIZ 512
    static char 	projects_buffer[PROJBUFSIZ];
    fs_quota_stat_t	s;
    fs_disk_quota_t	d;
    size_t		idsz, devsz;
    quota_entry_t	*quotap;
    FILE		*projects;
    char        	*p, *idend;
    uint32_t    	prid;
    int         	sts;

    if (quotactl(QCMD(Q_XGETQSTAT, PRJQUOTA), fs->device, 0, (void *)&s) < 0)
	return;

    fs->flags &= ~(FSF_QUOT_PROJ_ACC | FSF_QUOT_PROJ_ENF);
    if (s.qs_flags & XFS_QUOTA_PDQ_ACCT) 
	fs->flags |= FSF_QUOT_PROJ_ACC;
	
    if (s.qs_flags & XFS_QUOTA_PDQ_ENFD)
	fs->flags |= FSF_QUOT_PROJ_ENF;

    quotactl(Q_XQUOTASYNC, fs->device, 0, NULL);
    projects = fopen("/etc/projects", "r");
    if (projects == NULL)
	return; 

    while (fgets(projects_buffer, PROJBUFSIZ, projects)) {

	if (projects_buffer[0] == '#')
	    continue;

	prid = strtol(projects_buffer, &idend, 10);
	idsz = idend - projects_buffer;
	if (idsz == 0 || 
	    quotactl(QCMD(Q_XGETQUOTA, PRJQUOTA), fs->device, 
		     prid, (void *)&d) < 0)
	    continue;

	devsz = strlen(fs->device);
	p = malloc(idsz+devsz+2);
	memcpy(p, projects_buffer, idsz);
	p[idsz] = ':'; 
	memcpy(&p[idsz+1], fs->device, devsz+1);

	sts = pmdaCacheLookupName(qindom, p, NULL, (void **)&quotap);
	if (sts == PM_ERR_INST || (sts >= 0 && quotap == NULL)) {
	    /* first time since re-loaded, else new one */
	    quotap = (quota_entry_t *)calloc(1, sizeof(quota_entry_t));
	}
	quotap->space_hard = d.d_blk_hardlimit;
	quotap->space_soft = d.d_blk_softlimit;
	quotap->space_used = d.d_bcount;
	quotap->space_time_left = d.d_btimer;
	quotap->files_hard = d.d_ino_hardlimit;
	quotap->files_soft = d.d_ino_softlimit;
	quotap->files_used = d.d_icount;
	quotap->files_time_left = d.d_itimer;
	pmdaCacheStore(qindom, PMDA_CACHE_ADD, p, (void *)quotap);
    }
    fclose(projects);
}

int
refresh_filesys(pmInDom filesys_indom, pmInDom quota_indom)
{
    static int seen;
    char buf[MAXPATHLEN];
    char realdevice[MAXPATHLEN];
    filesys_t *fs;
    FILE *fp;
    char *path;
    char *device;
    char *type;
    char *p;
    int sts;

    if (!seen) {
	seen = 1;
	pmdaCacheOp(quota_indom, PMDA_CACHE_LOAD);
    }
    pmdaCacheOp(quota_indom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(filesys_indom, PMDA_CACHE_INACTIVE);

    if ((fp = fopen("/proc/mounts", "r")) == (FILE *)NULL)
	return -errno;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (( device = strtok(buf, " ")) == 0
	   || strncmp(device, "/dev", 4) != 0)
	    continue;
	if (realpath(device, realdevice) != NULL)
	    device = realdevice;

	path = strtok(NULL, " ");
	type = strtok(NULL, " ");
	if (strcmp(type, "proc") == 0 ||
	    strcmp(type, "nfs") == 0 ||
	    strcmp(type, "devfs") == 0 ||
	    strcmp(type, "devpts") == 0 ||
	    strncmp(type, "auto", 4) == 0)
	    continue;

	sts = pmdaCacheLookupName(filesys_indom, device, NULL, (void **)&fs);
	if (sts == PMDA_CACHE_ACTIVE)	/* repeated line in /proc/mounts? */
	    continue;
	if (sts == PMDA_CACHE_INACTIVE) { /* re-activate an old mount */
	    pmdaCacheStore(filesys_indom, PMDA_CACHE_ADD, device, fs);
	    if (strcmp(path, fs->path) != 0) {	/* old device, new path */
		free(fs->path);
		fs->path = strdup(path);
	    }
	}
	else {	/* new mount */
	    if ((fs = malloc(sizeof(filesys_t))) == NULL)
		continue;
	    fs->path = strdup(path);
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
		fprintf(stderr, "refresh_filesys: add \"%s\" \"%s\"\n",
		    fs->path, device);
	    }
#endif
	    pmdaCacheStore(filesys_indom, PMDA_CACHE_ADD, device, fs);
	}

	/* Quotas : so far only xfs project quota metrics are implemented */
	if (strcmp(type, "xfs") != 0)
	    continue;

	project_quotas(quota_indom, fs);
    }
    pmdaCacheOp(quota_indom, PMDA_CACHE_SAVE);

    /*
     * success
     * Note: we do not call statfs() here since only some instances
     * may be requested (rather, we do it in linux_fetch, see pmda.c).
     */
    fclose(fp);
    return 0;
}
