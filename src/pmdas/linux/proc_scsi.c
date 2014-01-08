/*
 * Linux Scsi Devices Cluster
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "proc_scsi.h"

static char diskname[64];
static char tapename[64];
static char cdromname[64];

int
refresh_proc_scsi(proc_scsi_t *scsi) {
    char buf[1024];
    char name[1024];
    int i;
    int n;
    FILE *fp;
    char *sp;
    static int have_devfs = -1;
    static int next_id = -1;

    if (next_id < 0) {
	/* one trip initialization */
	next_id = 0;

	scsi->nscsi = 0;
    	scsi->scsi = (scsi_entry_t *)malloc(sizeof(scsi_entry_t));

	/* scsi indom */
	scsi->scsi_indom->it_numinst = 0;
	scsi->scsi_indom->it_set = (pmdaInstid *)malloc(sizeof(pmdaInstid));

	/* devfs naming convention */
	have_devfs = access("/dev/.devfsd", F_OK) == 0;
	if (have_devfs) {
	    strcpy(diskname, "scsi/host%d/bus%d/target%d/lun%d/disc");
	    strcpy(tapename, "st0");
	    strcpy(cdromname, "scd0");
	}
	else {
	    strcpy(diskname, "sda");
	    strcpy(tapename, "st0");
	    strcpy(cdromname, "scd0");
	}
    }

    if ((fp = fopen("/proc/scsi/scsi", "r")) == (FILE *)NULL)
    	return -oserror();

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	scsi_entry_t	x = { 0 };

	if (strncmp(buf, "Host:", 5) != 0)
	    continue;

	n = sscanf(buf, "Host: scsi%d Channel: %d Id: %d Lun: %d",
	    &x.dev_host, &x.dev_channel, &x.dev_id, &x.dev_lun);
	if (n != 4)
	    continue;
	for (i=0; i < scsi->nscsi; i++) {
	    if (scsi->scsi[i].dev_host == x.dev_host && 
	    	scsi->scsi[i].dev_channel == x.dev_channel &&
	    	scsi->scsi[i].dev_id == x.dev_id &&
	    	scsi->scsi[i].dev_lun == x.dev_lun)
		break;
	}

	if (i == scsi->nscsi) {
	    scsi->nscsi++;
	    scsi->scsi = (scsi_entry_t *)realloc(scsi->scsi,
		scsi->nscsi * sizeof(scsi_entry_t));
	    memcpy(&scsi->scsi[i], &x, sizeof(scsi_entry_t));
	    scsi->scsi[i].id = next_id++;
	    /* scan for the Vendor: and Type: strings */
	    while (fgets(buf, sizeof(buf), fp) != NULL) {
		if ((sp = strstr(buf, "Type:")) != (char *)NULL) {
		    if (sscanf(sp, "Type:   %s", name) == 1)
			scsi->scsi[i].dev_type = strdup(name);
		    else
			scsi->scsi[i].dev_type = strdup("unknown");
		    break;
		}
	    }

	    if (strcmp(scsi->scsi[i].dev_type, "Direct-Access") == 0) {
		if (have_devfs) {
		    scsi->scsi[i].dev_name = (char *)malloc(64);
		    sprintf(scsi->scsi[i].dev_name, diskname, 
			scsi->scsi[i].dev_host, scsi->scsi[i].dev_channel,
			scsi->scsi[i].dev_id, scsi->scsi[i].dev_lun);
		}
		else {
		    scsi->scsi[i].dev_name = strdup(diskname);
		    diskname[2]++; /* sd[a-z] bump to next disk device name */
		}
	    }
	    else
	    if (strcmp(scsi->scsi[i].dev_type, "Sequential-Access") == 0) {
	    	scsi->scsi[i].dev_name = strdup(tapename);
		tapename[2]++; /* st[0-9] bump to next tape device name */
	    }
	    else
	    if (strcmp(scsi->scsi[i].dev_type, "CD-ROM") == 0) {
	    	scsi->scsi[i].dev_name = strdup(cdromname);
		cdromname[3]++; /* scd[0-9] bump to next CDROM device name */
	    }
	    else
	    if (strcmp(scsi->scsi[i].dev_type, "Processor") == 0)
	    	scsi->scsi[i].dev_name = strdup("SCSI Controller");
	    else
	    	scsi->scsi[i].dev_name = strdup("Unknown SCSI device");
	    	
	    sprintf(name, "scsi%d:%d:%d:%d %s", scsi->scsi[i].dev_host,
	    	scsi->scsi[i].dev_channel, scsi->scsi[i].dev_id, scsi->scsi[i].dev_lun, scsi->scsi[i].dev_type);
	    scsi->scsi[i].namebuf = strdup(name);
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
		fprintf(stderr, "refresh_proc_scsi: add host=scsi%d channel=%d id=%d lun=%d type=%s\n",
		    scsi->scsi[i].dev_host, scsi->scsi[i].dev_channel,
		    scsi->scsi[i].dev_id, scsi->scsi[i].dev_lun,
		    scsi->scsi[i].dev_type);
	    }
#endif
	}
    }

    /* refresh scsi indom */
    if (scsi->scsi_indom->it_numinst != scsi->nscsi) {
        scsi->scsi_indom->it_numinst = scsi->nscsi;
        scsi->scsi_indom->it_set = (pmdaInstid *)realloc(scsi->scsi_indom->it_set,
	    scsi->nscsi * sizeof(pmdaInstid));
        memset(scsi->scsi_indom->it_set, 0, scsi->nscsi * sizeof(pmdaInstid));
    }
    for (i=0; i < scsi->nscsi; i++) {
	scsi->scsi_indom->it_set[i].i_inst = scsi->scsi[i].id;
	scsi->scsi_indom->it_set[i].i_name = scsi->scsi[i].namebuf;
    }

    /*
     * success
     */
    fclose(fp);
    return 0;
}
