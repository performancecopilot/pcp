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

#define BUFSIZE 1048576

handler_t handlers[] = {
	{ ">>>",		timestamp_handler },

	/* /proc/PID/... */
	{ "proc:*",		proc_handler },

	/* /proc/stat */
	{ "cpu*",		cpu_handler },
	{ "processes",		generic1_handler,	"kernel.all.nprocs" },
	{ "intr",		generic1_handler,	"kernel.all.intr" },
	{ "ctx",		generic1_handler,	"kernel.all.pswitch" },

	/* /proc/diskstats */
	{ "disk",		disk_handler },

	/* /proc/net/dev */
	{ "Net",		net_handler },

	/* /proc/loadavg */
	{ "load",		loadavg_handler },

	/* /proc/meminfo */
	{ "MemTotal:",		generic1_handler,	"mem.physmem" },
	{ "MemFree:",		generic1_handler,	"mem.util.free" },
	{ "Buffers:",		generic1_handler,	"mem.util.bufmem" },
	{ "Cached:",		generic1_handler,	"mem.util.cached" },
	{ "SwapCached:",	generic1_handler,	"mem.util.swapCached" },
	{ "Active:",		generic1_handler,	"mem.util.active" },
	{ "Inactive:",		generic1_handler,	"mem.util.inactive" },
	{ "Active(anon):",	generic1_handler,	"mem.util.active_anon" },
	{ "Inactive(anon):",	generic1_handler,	"mem.util.inactive_anon" },
	{ "Active(file):",	generic1_handler,	"mem.util.active_file" },
	{ "Inactive(file):",	generic1_handler,	"mem.util.inactive_file" },
	{ "Unevictable:",	generic1_handler,	"mem.util.unevictable" },
	{ "Mlocked:",		generic1_handler,	"mem.util.mlocked" },
	{ "SwapTotal:",		generic1_handler,	"mem.util.swapTotal" },
	{ "SwapFree:",		generic1_handler,	"mem.util.swapFree" },
	{ "Dirty:",		generic1_handler,	"mem.util.dirty" },
	{ "Writeback:",		generic1_handler,	"mem.util.writeback" },
	{ "AnonPages:",		generic1_handler,	"mem.util.anonpages" },
	{ "Mapped:",		generic1_handler,	"mem.util.mapped" },
	{ "Shmem:",		generic1_handler,	"mem.util.shmem" },
	{ "Slab:",		generic1_handler,	"mem.util.slab" },
	{ "SReclaimable:",	generic1_handler,	"mem.util.slabReclaimable" },
	{ "SUnreclaim:",	generic1_handler,	"mem.util.slabUnreclaimable" },
	{ "KernelStack:",	generic1_handler,	"mem.util.kernelStack" },
	{ "PageTables:",	generic1_handler,	"mem.util.pageTables" },
	{ "NFS_Unstable:",	generic1_handler,	"mem.util.NFS_Unstable" },
	{ "Bounce:",		generic1_handler,	"mem.util.bounce" },
	{ "WritebackTmp:",	generic1_handler,	"unknown" },
	{ "CommitLimit:",	generic1_handler,	"mem.util.commitLimit" },
	{ "Committed_AS:",	generic1_handler,	"mem.util.committed_AS" },
	{ "VmallocTotal:",	generic1_handler,	"mem.util.vmallocTotal" },
	{ "VmallocUsed:",	generic1_handler,	"mem.util.vmallocUsed" },
	{ "VmallocChunk:",	generic1_handler,	"mem.util.vmallocChunk" },
	{ "HardwareCorrupted:",	generic1_handler,	"mem.util.corrupthardware" },
	{ "AnonHugePages:",	generic1_handler,	"mem.util.anonhugepages" },
	{ "HugePages_Total:",	generic1_handler,	"mem.util.hugepagesTotal" },
	{ "HugePages_Free:",	generic1_handler,	"mem.util.hugepagesFree" },
	{ "HugePages_Rsvd:",	generic1_handler,	"mem.util.hugepagesRsvd" },
	{ "HugePages_Surp:",	generic1_handler,	"mem.util.hugepagesSurp" },
	{ "Hugepagesize:",	generic1_handler,	"unknown" },
	{ "DirectMap4k:",	generic1_handler,	"mem.util.directMap4k" },
	{ "DirectMap2M:",	generic1_handler,	"mem.util.directMap2M" },

	{ NULL }
};

int indom_cnt[NUM_INDOMS] = {0};

/* global options */
int vflag = 0;
int Fflag = 0;
int kernel_all_hz = 0;
int utc_offset = 0;

int
main(int argc, char *argv[])
{
    int         sts;
    int         ctx;
    int         errflag = 0;
    int         c;
    char	*infile;
    int		nfilelist;
    int		filenum;
    char	*archive = NULL;
    int		j;
    char	*buf;
    fields_t	*f;
    char	*s;
    int		gzipped;
    FILE	*fp;
    metric_t	*m;
    handler_t	*h;
    int		unhandled_metric_cnt = 0;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "FD:v")) != EOF) {
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
	case 'F':
	    Fflag = 1;
	    break;
	case 'v':
	    vflag++;
	    break;
        case '?':
        default:
            errflag++;
            break;
        }
    }

    nfilelist = argc - optind - 1;
    if (nfilelist < 1)
    	errflag++;
    else
	archive = argv[argc-1];

    if (errflag) {
usage:	fprintf(stderr,
	"Usage: %s [-F] [-v] [-D N] inputfile [inputfile ...] archive\n"
	"Each 'inputfile' is a collectl archive, must be for the same host (may be gzipped).\n"
	"'archive' is the base name for the PCP archive to be created.\n"
	"-F forces overwrite of 'archive' if it already exists.\n"
	"-v enables verbose messages. Use more -v for extra verbosity.\n"
	"-D N turns on debugging bits 'N', see pmdbg(1).\n", pmProgname);
        exit(1);
    }

    if ((buf = malloc(BUFSIZE)) == NULL) {
    	perror("Error: out of memory:");
	exit(1);
    }

    if (Fflag) {
    	snprintf(buf, BUFSIZE, "%s.meta", archive); unlink(buf);
    	snprintf(buf, BUFSIZE, "%s.index", archive); unlink(buf);
	for (j=0;; j++) {
	    snprintf(buf, BUFSIZE, "%s.%d", archive, j);
	    if (unlink(buf) < 0)
	    	break;
	}
    }

    ctx = pmiStart(archive, 0);
    if ((sts = pmiUseContext(ctx)) < 0) {
	fprintf(stderr, "Error: pmiUseContext failed: %s\n", pmiErrStr(sts));
	exit(1);
    }

    /*
     * Define the metrics name space, see metrics.c (generated by pmdesc)
     */
    for (m = metrics; m->name; m++) {
	pmDesc *d = &m->desc;

	sts = pmiAddMetric(m->name, d->pmid, d->type, d->indom, d->sem, d->units);
	if (sts < 0) {
	    fprintf(stderr, "Error: failed to add metric %s: %s\n", m->name, pmiErrStr(sts));
	    exit(1);
	}
    }

    /*
     * Populate special case instance domains
     */
    pmiAddInstance(pmInDom_build(LINUX_DOMAIN, LOADAVG_INDOM), "1 minute", 1);
    pmiAddInstance(pmInDom_build(LINUX_DOMAIN, LOADAVG_INDOM), "5 minute", 5);
    pmiAddInstance(pmInDom_build(LINUX_DOMAIN, LOADAVG_INDOM), "15 minute", 15);
    indom_cnt[LOADAVG_INDOM] = 3;

    for (filenum=0; filenum < nfilelist; filenum++) {
	infile = argv[optind + filenum];
	gzipped = strstr(infile, ".gz") != NULL;
	if (gzipped) {
	    snprintf(buf, BUFSIZE, "gzip -c -d %s", infile);
	    if ((fp = popen(buf, "r")) == NULL)
		perror(buf);
	}
	else
	if ((fp = fopen(infile, "r")) == NULL)
	    perror(infile);

	if (fp == NULL)
	    goto usage;

	/*
	 * parse the header
	 */
	sts = header_handler(fp, infile, buf, BUFSIZE);

	/*
	 * Parse remaining data stream for this input file
	 */
	while(fgets(buf, BUFSIZE, fp)) {
	    if ((s = strrchr(buf, '\n')) != NULL)
		*s = '\0';
	    if (!buf[0])
	    	continue;
	    f = fields_new(buf, strlen(buf)+1);
	    if (f->nfields > 0) {
		if ((h = find_handler(f->fields[0])) == NULL) {
		    unhandled_metric_cnt++;
		    if (vflag > 1)
			printf("Unhandled tag: \"%s\"\n", f->fields[0]);
		}
		else {
		    sts = h->handler(h, f);
		    if (sts < 0 && h->handler == timestamp_handler) {
			fprintf(stderr, "Error: %s\n", pmiErrStr(sts));
			exit(1);
		    }
		}
	    }
	    fields_free(f);
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
    if (unhandled_metric_cnt && vflag)
    	fprintf(stderr, "Warning: %d unhandled metric/values\n", unhandled_metric_cnt);

    exit(0);
}
