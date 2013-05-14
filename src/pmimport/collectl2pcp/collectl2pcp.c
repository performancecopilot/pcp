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
 * Import collectl raw data file and create a PCP archive.
 * Mark Goodwin <mgoodwin@redhat.com> May 2013.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include <pcp/import.h>

#include "metrics.h"

handler_t handlers[] = {
	{ ">>>",	timestamp_handler },
	{ "cpu",	cpu_handler },
	{ "disk",	disk_handler },
	{ "Net",	net_handler },
	{ "load",	load_handler },
	{ NULL }
};

int indom_cnt[NUM_INDOMS] = {0};

static char buf[1048576], rbuf[1048576];

int
main(int argc, char *argv[])
{
    int         sts;
    int         ctx;
    int         errflag = 0;
    int		verbose = 0;
    int         c;
    int		ncpus = 0;
    int		ndisks = 0;
    int		nnets = 0;
    int		physmem;
    int		hz;
    int		pagesize;
    char	*infile = NULL;
    char	*archive = NULL;
    char	*hostname = NULL;
    int		filenum;
    int		j;
    char	*s;
    char	*p;
    int		gzipped;
    FILE	*fp;
    metric_t	*m;
    handler_t	*h;
    int		unhandled_metric_cnt = 0;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:v")) != EOF) {
        switch (c) {

        case 'D':       /* debug flag */
            sts = __pmParseDebug(optarg);
            if (sts < 0) {
                fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
                    pmProgname, optarg);
                errflag++;
            }
            else
                pmDebug |= sts;
            break;
	case 'a':
	    archive = optarg;
	    break;
	case 'v':
	    verbose = 1;
	    break;
        case '?':
        default:
            errflag++;
            break;
        }
    }

    if (!archive)
    	errflag++;

    if (errflag) {
usage:	fprintf(stderr,
	"Usage %s [-v] [-D N] -a archive file [file ...]\n"
	"where 'archive' is the base name for the PCP archive to be created.\n"
	"Each 'file' is a collectl archive for the same host (may be gzipped).\n", pmProgname);
        exit(1);
    }

    ctx = pmiStart(archive, 0);
    if ((sts = pmiUseContext(ctx)) < 0) {
	fprintf(stderr, "Error: pmiUseContext failed: %s\n", pmiErrStr(sts));
	exit(1);
    }

    for (filenum=0; optind < argc; filenum++) {
	infile = argv[optind++];
	gzipped = strstr(infile, ".gz") != NULL;
	if (gzipped) {
	    sprintf(buf, "gzip -c -d %s", infile);
	    if ((fp = popen(buf, "r")) == NULL)
		perror(buf);
	}
	else
	if ((fp = fopen(infile, "r")) == NULL)
	    perror(infile);

	if (fp == NULL)
	    goto usage;

	/*
	 * parse collectl header
	 */
	while(fgets(buf, sizeof(buf), fp)) {
	    if ((s = strrchr(buf, '\n')) != NULL)
		*s = '\0';

	    if (strncmp(buf, ">>>", 3) == 0) {
		/* first timestamp: we're finished parsing the header */
		timestamp_handler(buf);
		break;
	    }

	    if (strncmp(buf, "# Host:", 7) == 0) {
		/* # Host:       somehostname ... */  
		s = strfield_r(buf, 3, rbuf);
		if (hostname && strcmp(hostname, s) != 0) {
		    fprintf(stderr, "Error: \"%s\" contains data for host \"%s\", not \"%s\"\n",
		    	infile, s, hostname);
		    exit(1); /* FATAL */
		}

		if (!hostname) {
		    hostname = strdup(s);
		    pmiSetHostname(s);
		}
		continue;
	    }

	    if (filenum == 0 && strncmp(buf, "# Date:", 7) == 0) {
		/* # Date:       20130505-170328  Secs: 1367791408 TZ: -0500 */
		if ((s = strstr(buf, "TZ: ")) != NULL) {
		    sscanf(s, "TZ: %s", rbuf);
		    sts = pmiSetTimezone(rbuf);
		}
		else
		    sts = pmiSetTimezone("UTC");
		continue;
	    }

	    if (filenum == 0 && strncmp(buf, "# SubSys:", 9) == 0) {
		/* # SubSys:     bcdfijmnstYZ Options:  Interval: 10:60 NumCPUs: 24  NumBud: 3 Flags: i */
		if ((s = strstr(buf, "NumCPUs: ")) != NULL)
		    sscanf(s, "NumCPUs: %d", &ncpus);
		continue;
	    }

	    if (filenum == 0 && strncmp(buf, "# HZ:", 3) == 0) {
		/* # HZ:         100  Arch: x86_64-linux-thread-multi PageSize: 4096 */
		if ((s = strfield_r(buf, 3, rbuf)) != NULL)
		    sscanf(s, "%d", &hz);
		if ((s = strstr(buf, "PageSize: ")) != NULL)
		    sscanf(s, "PageSize: %d", &pagesize);
		continue;
	    }

	    if (filenum == 0 && strncmp(buf, "# Kernel:", 9) == 0) {
		/* # Kernel:     2.6.18-274.17.1.el5  Memory: 131965176 kB  Swap: 134215000 kB */
		if ((s = strstr(buf, "Memory: ")) != NULL)
		    sscanf(s, "Memory: %d", &physmem);
		continue;
	    }

	    if (strncmp(buf, "# NumDisks:", 11) == 0) {
		/* # NumDisks:   846 DiskNames: sda sdb .... */
		sscanf(buf, "# NumDisks:%6d", &ndisks);

		/* disk.dev.* instance domain */
		if ((s = strstr(buf, "DiskNames:")) != NULL) {
		    s = strtok(s, " ");
		    for (j=0; j < ndisks; j++) {
			s = strtok(NULL, " ");
			sts = pmiAddInstance(pmInDom_build(LINUX_DOMAIN,DISK_INDOM), s, j);
			if (filenum == 0 && sts < 0)
			    fprintf(stderr, "Warning: failed to add disk \"%s\" failed: %s\n", s, pmiErrStr(sts));
			else {
			    if (verbose)
				printf("Added disk instance [%d] \"%s\"\n", j, s);
			}
		    }
		}
		indom_cnt[DISK_INDOM] = ndisks;
		continue;
	    }

	    if (strncmp(buf, "# NumNets:", 10) == 0) {
		/* # NumNets:    5 NetNames: em1: lo: ... */
		sscanf(buf, "# NumNets:%5d", &nnets);

		/* network.interface instance domain */
		if ((s = strstr(buf, "NetNames:")) != NULL) {
		    s = strtok(s, " ");
		    for (j=0; j < nnets; j++) {
			s = strtok(NULL, " ");
			if ((p = strchr(s, ':')) != NULL)
			    *p = '\0';
			sts = pmiAddInstance(pmInDom_build(LINUX_DOMAIN,NET_DEV_INDOM), s, j);
			if (filenum == 0 && sts < 0)
			    fprintf(stderr, "Warning: failed to add net \"%s\" failed: %s\n", s, pmiErrStr(sts));
			else {
			    if (verbose)
				printf("Added net instance [%d] \"%s\"\n", j, s);
			}
		    }
		}
	    }
	    indom_cnt[NET_DEV_INDOM] = nnets;
	    continue;
	}

	/*
	 * Populate other instance domains.
	 * Only report errors for the first input file.
	 */
	for (j=0; j < ncpus; j++) {
	    /* per-CPU instance domain */
	    sprintf(buf, "cpu%d", j);
	    sts = pmiAddInstance(pmInDom_build(LINUX_DOMAIN, CPU_INDOM), buf, j);
	    if (filenum == 0 && sts < 0) {
		fprintf(stderr, "Error: failed to add instance \"%s\": %s\n", buf, pmiErrStr(sts));
		exit(1);
	    }
	    if (verbose)
		printf("Added cpu instance [%d] \"%s\"\n", j, buf);
	}
	indom_cnt[CPU_INDOM] = ncpus;

	/*
	 * Populate instance domains that never change (first file only)
	 */
	if (filenum == 0) {
	    pmiAddInstance(pmInDom_build(LINUX_DOMAIN, LOADAVG_INDOM), "1 minute", 1);
	    pmiAddInstance(pmInDom_build(LINUX_DOMAIN, LOADAVG_INDOM), "5 minute", 5);
	    pmiAddInstance(pmInDom_build(LINUX_DOMAIN, LOADAVG_INDOM), "15 minute", 15);
	    indom_cnt[LOADAVG_INDOM] = 3;
	}

	/*
	 * Define the metrics name space, see metrics.c (generated by pmdesc)
	 */
	if (filenum == 0) {
	    for (m = metrics; m->name; m++) {
		pmDesc *d = &m->desc;

		sts = pmiAddMetric(m->name, d->pmid, d->type, d->indom, d->sem, d->units);
		if (sts < 0) {
		    fprintf(stderr, "Error: failed to add metric %s: %s\n", m->name, pmiErrStr(sts));
		    exit(1);
		}
	    }
	}

	/*
	 * Assorted singular metrics: emit these once for each input file.
	 * hinv.ndisk can change due to hotplug and also for dm devices.
	 * hinv.ncpu can change on a VM due to hotplug CPU.
	 */
	put_int_value("hinv.ncpu", PM_INDOM_NULL, NULL, ncpus);
	put_int_value("hinv.ndisk", PM_INDOM_NULL, NULL, ndisks);
	put_int_value("hinv.physmem", PM_INDOM_NULL, NULL, physmem/1024); /* mbytes */
	put_int_value("hinv.pagesize", PM_INDOM_NULL, NULL, pagesize);
	put_int_value("kernel.all.hz", PM_INDOM_NULL, NULL, hz);

	if (verbose) {
	    printf("file:\"%s\" host:\"%s\" ncpus:%d ndisks:%d nnets:%d physmem:%d pagesize:%d hz:%d\n",
		infile, hostname, ncpus, ndisks, nnets, physmem, pagesize, hz);
	}

	/*
	 * Parse remaining data stream for this input file
	 */
	while(fgets(buf, sizeof(buf), fp)) {
	    if ((s = strrchr(buf, '\n')) != NULL)
		*s = '\0';

	    if ((h = find_handler(buf)) == NULL)
		unhandled_metric_cnt++;
	    else {
		sts = h->handler(buf);
		if (sts < 0 && h->handler == timestamp_handler) {
		    fprintf(stderr, "Error: %s\n", pmiErrStr(sts));
		    exit(1);
		}
	    }
	}

	/* final flush for this file */
	if ((sts = timestamp_flush()) < 0) {
	    fprintf(stderr, "Error: failed to write final timestamp: %s\n", pmiErrStr(sts));
	    exit(1);
	}

	if (gzipped)
	    pclose(fp);
	else
	    fclose(fp);
    }

    sts = pmiEnd();
    if (unhandled_metric_cnt && verbose)
    	fprintf(stderr, "Warning: %d unhandled metric/values\n", unhandled_metric_cnt);

    exit(0);
}
