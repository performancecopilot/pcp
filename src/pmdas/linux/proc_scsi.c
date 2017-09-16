/*
 * Linux Scsi Devices Cluster
 *
 * Copyright (c) 2014 Red Hat.
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
#include "linux.h"
#include "proc_scsi.h"

int
refresh_proc_scsi(pmInDom indom)
{
    char buf[1024];
    char name[64];
    char type[64];
    int n, failed;
    FILE *fp;
    char *sp;
    int sts;
    DIR *dirp;
    struct dirent *dentry;
    scsi_entry_t *se;
    static int first = 1;

    if (first) {
	first = 0;
	/*
	 * failure here maybe OK, e.g. if the external cache file does
	 * not exist
	 */
    	sts = pmdaCacheOp(indom, PMDA_CACHE_LOAD);
	if (pmDebugOptions.libpmda && sts < 0)
	    fprintf(stderr, "refresh_proc_scsi: pmdaCacheOp(%s, LOAD): %s\n",
		    pmInDomStr(indom), pmErrStr(sts));
    }

    if ((fp = linux_statsfile("/proc/scsi/scsi", buf, sizeof(buf))) == NULL)
    	return -oserror();

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	scsi_entry_t x = { 0 };

	if (strncmp(buf, "Host:", 5) != 0)
	    continue;

	n = sscanf(buf, "Host: scsi%d Channel: %d Id: %d Lun: %d",
	    &x.dev_host, &x.dev_channel, &x.dev_id, &x.dev_lun);
	if (n != 4)
	    continue;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
	    if ((sp = strstr(buf, "Type:")) != (char *)NULL) {
		if (sscanf(sp, "Type:   %s", type) == 1)
		    break;
	    }
	}

	pmsprintf(name, sizeof(name), "scsi%d:%d:%d:%d %s",
	    x.dev_host, x.dev_channel, x.dev_id, x.dev_lun, type);

	failed = 0;
	sts = pmdaCacheLookupName(indom, name, NULL, (void **)&se);
	if (sts == PMDA_CACHE_ACTIVE)
	    continue;
	if (sts != PMDA_CACHE_INACTIVE || se == NULL) {
	    /* New device, not in indom cache */
	    if ((se = (scsi_entry_t *)malloc(sizeof(scsi_entry_t))) == NULL) {
		failed++;
		continue;
	    }
	    *se = x; /* struct copy */

	    /* find the block device name from sysfs */
	    pmsprintf(buf, sizeof(buf),
			    "/sys/class/scsi_device/%d:%d:%d:%d/device/block",
		se->dev_host, se->dev_channel, se->dev_id, se->dev_lun);
	    if ((dirp = opendir(buf)) == NULL) {
		failed++;
	    } else {
		se->dev_name = NULL;
		while ((dentry = readdir(dirp)) != NULL) {
	    	    if (dentry->d_name[0] == '.')
			continue;
		    se->dev_name = strdup(dentry->d_name);
		    break;
		}
		if (!se->dev_name) {
		    failed++;
		}
		closedir(dirp);
	    }
	}
	if (!failed) {
	    sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, name, (void *)se);
	    if (sts < 0) {
		fprintf(stderr, "Warning: refresh_proc_scsi: pmdaCacheOp(%s, ADD, \"%s\", (%s)): %s\n",
		    pmInDomStr(indom), name, se->dev_name, pmErrStr(sts));
		free(se->dev_name);
		free(se);
	    }
	    else {
		if (pmDebugOptions.libpmda)
		    fprintf(stderr, "refresh_proc_scsi: instance \"%s\" = \"%s\"\n",
			name, se->dev_name);
	    }
	}
	else
	    free(se);
    }

    pmdaCacheOp(indom, PMDA_CACHE_SAVE);
    fclose(fp);
    return 0;
}
