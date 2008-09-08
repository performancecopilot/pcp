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

#ident "$Id: filesys.c,v 1.10 2008/02/27 03:25:38 kimbrr Exp $"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

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
project_quotas(pmInDom qindom, filesys_entry_t *entryp)
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

    if (quotactl(QCMD(Q_XGETQSTAT, PRJQUOTA), entryp->device, 0, (void *)&s) < 0)
	return;

    entryp->flags &= ~(FSF_QUOT_PROJ_ACC | FSF_QUOT_PROJ_ENF);
    if (s.qs_flags & XFS_QUOTA_PDQ_ACCT) 
	entryp->flags |= FSF_QUOT_PROJ_ACC;
	
    if (s.qs_flags & XFS_QUOTA_PDQ_ENFD)
	entryp->flags |= FSF_QUOT_PROJ_ENF;

    quotactl(Q_XQUOTASYNC, entryp->device, 0, NULL);
    projects = fopen("/etc/projects", "r");
    if (projects == NULL)
	return; 

    while (fgets(projects_buffer, PROJBUFSIZ, projects)) {

	if (projects_buffer[0] == '#')
	    continue;

	prid = strtol(projects_buffer, &idend, 10);
	idsz = idend - projects_buffer;
	if (idsz == 0 || 
	    quotactl(QCMD(Q_XGETQUOTA, PRJQUOTA), entryp->device, 
		     prid, (void *)&d) < 0)
	    continue;

	devsz = strlen(entryp->device);
	p = malloc(idsz+devsz+2);
	memcpy(p, projects_buffer, idsz);
	p[idsz] = ':'; 
	memcpy(&p[idsz+1], entryp->device, devsz+1);

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
refresh_filesys(filesys_t *filesys, pmInDom qindom) 
{
   static char realdevice[MAXPATHLEN];
    static char buf[MAXPATHLEN];

    FILE *fp;
    char *path;
    char *device;
    char *type;
    int i;
    int n;
    int old_device;

    pmdaIndom *indomp = filesys->indom;
    static int next_id = -1;

    if (next_id < 0) {
	next_id = 0;
	filesys->nmounts = 0;
    	filesys->mounts = NULL;
	indomp->it_numinst = 0;
	indomp->it_set = NULL;
	pmdaCacheOp(qindom, PMDA_CACHE_LOAD);
    }
    pmdaCacheOp(qindom, PMDA_CACHE_INACTIVE);

    if ((fp = fopen("/proc/mounts", "r")) == (FILE *)NULL)
    	return -errno;

    for (i=0; i < filesys->nmounts; i++) {
    	filesys->mounts[i].flags &= ~FSF_SEEN;
    }

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

	old_device = -1;
	for (i=0; i < filesys->nmounts; i++) {
	    if (filesys->mounts[i].device != NULL && strcmp(filesys->mounts[i].device, device) == 0) {
		if (filesys->mounts[i].flags & FSF_VALID)
		    break;
		else
		    old_device = i;
	    }
	}
	if (i == filesys->nmounts) {
	    /* new mount */
	    if (old_device >= 0) {
		/* same device as last time mounted: reuse the id and device name */ 
	    	i = old_device;
	    }
	    else {
		filesys->nmounts++;
	    	filesys->mounts = (filesys_entry_t *)realloc(filesys->mounts,
		    filesys->nmounts * sizeof(filesys_entry_t));
		filesys->mounts[i].device = strdup(device);
		filesys->mounts[i].id = next_id++;
	    }
	    filesys->mounts[i].path = strdup(path);
	    /* Whether the struct itself is new or recycled, this is
	     * initialisation for a new mount, so "flags =" here, 
	     * rather than |=
	     */
	    filesys->mounts[i].flags = FSF_VALID;
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
		fprintf(stderr, "refresh_filesys: add \"%s\" \"%s\"\n",
		    filesys->mounts[i].path, filesys->mounts[i].device);
	    }
#endif
	}
	filesys->mounts[i].flags |= FSF_SEEN;

	/* Quotas : so far only xfs project quota metrics are implemented */
	if (strcmp(type, "xfs") != 0)
	    continue;

	project_quotas(qindom, &filesys->mounts[i]);
    }
    pmdaCacheOp(qindom, PMDA_CACHE_SAVE);
    /* check for filesystems that have been unmounted */
    for (n=0, i=0; i < filesys->nmounts; i++) {
	if (filesys->mounts[i].flags & FSF_VALID) {
	    if (!(filesys->mounts[i].flags & FSF_SEEN)) {
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    fprintf(stderr, "refresh_filesys: drop \"%s\" \"%s\"\n",
			filesys->mounts[i].path, filesys->mounts[i].device);
		}
#endif
		free(filesys->mounts[i].path);
		filesys->mounts[i].path = NULL;
		filesys->mounts[i].flags &= ~FSF_VALID;
	    }
	    else
		n++;
    	}
    }

    /* refresh indom */
    if (indomp->it_numinst != n) {
        indomp->it_numinst = n;
        indomp->it_set = (pmdaInstid *)realloc(indomp->it_set, n * sizeof(pmdaInstid));
        memset(indomp->it_set, 0, n * sizeof(pmdaInstid));
    }
    for (n=0, i=0; i < filesys->nmounts; i++) {
        if (filesys->mounts[i].flags & FSF_VALID) {
            if (filesys->mounts[i].id != indomp->it_set[n].i_inst || indomp->it_set[n].i_name == NULL) {
                indomp->it_set[n].i_inst = filesys->mounts[i].id;
                indomp->it_set[n].i_name = filesys->mounts[i].device;
            }
	    filesys->mounts[i].flags &= ~FSF_FETCHED; /* avoid multiple calls to statfs */
            n++;
        }
    }

    /*
     * success
     * Note: we do not call statfs() here since only some instances
     * may be requested (rather, we do it in linux_fetch, see pmda.c).
     */
    fclose(fp);
    return 0;
}
