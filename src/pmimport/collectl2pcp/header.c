/*
 * Copyright (c) 2013 Red Hat Inc.
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
 * Parse collectl header up to (and including) the first timestamp.
 */

#include "metrics.h"

static char *hostname = NULL;
int header_time = 0;

int
header_handler(FILE *fp, char *fname, char *buf, int maxbuf)
{
    int i;
    char *s;
    fields_t *f;
    int sts = 0;


    while(fgets(buf, maxbuf, fp)) {
	if ((s = strrchr(buf, '\n')) != NULL)
	    *s = '\0';
	if (!buf[0])
	    continue;

	f = fields_new(buf, strlen(buf)+1);
	if (f->nfields == 0) {
	    fields_free(f);
	    continue;
	}

	if (strncmp(f->fields[0], ">>>", 3) == 0) {
	    /* first timestamp: we're finished parsing the header */
	    sts = timestamp_handler(find_handler(">>>"), f);
	    fields_free(f);
	    break;
	}

	if (f->nfields > 3 && strncmp(f->fields[1], "Host:", 5) == 0) {
	    /* # Host:       somehostname ... */  
	    if (hostname && strcmp(hostname, f->fields[2]) != 0) {
		fprintf(stderr, "FATAL Error: host mismatch: \"%s\" contains data for host \"%s\", not \"%s\"\n",
		    fname, f->fields[2], hostname);
		exit(1);
	    }

	    if (!hostname) {
		hostname = strdup(f->fields[2]);
		pmiSetHostname(hostname);
		put_str_value("kernel.uname.nodename", PM_INDOM_NULL, NULL, hostname);
		put_str_value("kernel.uname.sysname", PM_INDOM_NULL, NULL, "Linux");
	    }
	}

	if (f->nfields > 2 && strncmp(f->fields[1], "Distro:", 7) == 0) {
	    strcpy(buf, f->fields[2]);
	    for (i=3; i < f->nfields; i++) {
		if (strcmp(f->fields[i], "Platform:") == 0)
		    break;
		strcat(buf, " ");
	    	strcat(buf, f->fields[i]);
	    }
	    put_str_value("kernel.uname.distro", PM_INDOM_NULL, NULL, buf);

#if 0	/* TODO -- add hinv.platform */
	    if (i < f->nfields) {	/* found embedded platform */
		strcpy(buf, f->fields[++i]);
		for (; i < f->nfields; i++) {
		    strcat(buf, " ");
		    strcat(buf, f->fields[i]);
		}
		put_str_value("hinv.platform", PM_INDOM_NULL, NULL, buf);
	    }
#endif
	}

	if (f->nfields == 7 && strncmp(f->fields[1], "Date:", 5) == 0) {
	    /* # Date:       20130505-170328  Secs: 1367791408 TZ: -0500 */
	    int d = strtol(f->fields[4], NULL, 0);

	    if (d < header_time) {
		fprintf(stderr, "FATAL Error: input file order mismatch: \"%s\" contains data at %d, prior to %d\n",
		    fname, d, header_time);
		exit(1);
	    }
	    header_time = d;
	    sts = pmiSetTimezone(f->fields[6]);
	    utc_offset = strtol(f->fields[6], NULL, 0);
	    sscanf(f->fields[6], "%d", &utc_offset); /* e.g. -0500 */
	    utc_offset /= 100;
	    if (vflag)
	    	printf("Timezone set to \"%s\" utc_offset=%d hours, sts=%d\n", f->fields[6], utc_offset, sts);
	}

	if (f->nfields > 8 && strncmp(f->fields[1], "SubSys:", 7) == 0) {
	    /* # SubSys:     bcdfijmnstYZ Options:  Interval: 10:60 NumCPUs: 24  NumBud: 3 Flags: i */
	    put_str_value("hinv.ncpu", PM_INDOM_NULL, NULL, f->fields[7]);
	}

	if (f->nfields == 7 && strncmp(f->fields[1], "HZ:", 3) == 0) {
	    /* # HZ:         100  Arch: x86_64-linux-thread-multi PageSize: 4096 */
	    kernel_all_hz = strtol(f->fields[2], NULL, 0);
	    put_str_value("kernel.all.hz", PM_INDOM_NULL, NULL, f->fields[2]);
	    put_str_value("kernel.uname.machine", PM_INDOM_NULL, NULL, f->fields[4]);
	    put_str_value("hinv.pagesize", PM_INDOM_NULL, NULL, f->fields[6]);
	}

	if (f->nfields == 9 && strncmp(f->fields[1], "Kernel:", 7) == 0) {
	    /* # Kernel:     2.6.18-274.17.1.el5  Memory: 131965176 kB  Swap: 134215000 kB */
	    put_str_value("kernel.uname.release", PM_INDOM_NULL, NULL, f->fields[2]);
	    put_int_value("hinv.physmem", PM_INDOM_NULL, NULL, atoi(f->fields[4])/1024);
	    put_str_value("hinv.machine", PM_INDOM_NULL, NULL, "linux");
	}

	if (f->nfields > 4 && strncmp(f->fields[1], "NumDisks:", 9) == 0) {
	    /* # NumDisks:   846 DiskNames: sda sdb .... */
	    put_str_value("hinv.ndisk", PM_INDOM_NULL, NULL, f->fields[2]);
	}

	if (f->nfields > 4 && strncmp(f->fields[1], "NumNets:", 8) == 0) {
	    /* # NumNets:    5 NetNames: em1: lo: ... */
	    put_str_value("hinv.ninterface", PM_INDOM_NULL, NULL, f->fields[2]);
	}

	fields_free(f);
    }

    if (vflag)
	printf("Parsed header in file:\"%s\" host:\"%s\" sts=%d\n", fname, hostname, sts);

    return sts;
}
