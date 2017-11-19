/*
 * Copyright (c) 2013-2014 Red Hat.
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
 */

#include <pcp/pmapi.h>
#include <pcp/libpcp.h>
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
	{ "ctxt",		generic1_handler,	"kernel.all.pswitch" },

	/* /proc/diskstats */
	{ "disk",		disk_handler },

	/* /proc/net/dev */
	{ "Net",		net_handler },

	/* /proc/net/snmp */
	{ "tcp-Tcp:", 		net_tcp_handler },
	{ "tcp-Udp:", 		net_udp_handler },

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

int indom_cnt[NUM_INDOMS];

/* global options */
int vflag;
int Fflag;
int kernel_all_hz;
int utc_offset;

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "force", 0, 'f', 0, "forces overwrite of 'archive' if it already exists" },
    { "verbose", 0, 'v', 0, "enables increasingly verbose messages" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "FD:v?",
    .long_options = longopts,
    .short_usage = "inputfile [inputfile ...] archive\n"
"Each 'inputfile' is a collectl archive, must be for the same host (may be gzipped).\n"
"'archive' is the base name for the PCP archive to be created."
};

int
main(int argc, char *argv[])
{
    int         sts;
    int         ctx;
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

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
        switch (c) {
	case 'F':
	    Fflag = 1;
	    break;
	case 'v':
	    vflag++;
	    break;
        }
    }

    nfilelist = argc - opts.optind - 1;
    if (nfilelist < 1)
    	opts.errors++;
    else
	archive = argv[argc - 1];

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if ((buf = malloc(BUFSIZE)) == NULL) {
    	perror("Error: out of memory:");
	exit(1);
    }

    if (Fflag) {
    	pmsprintf(buf, BUFSIZE, "%s.meta", archive); unlink(buf);
    	pmsprintf(buf, BUFSIZE, "%s.index", archive); unlink(buf);
	for (j=0;; j++) {
	    pmsprintf(buf, BUFSIZE, "%s.%d", archive, j);
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
	infile = argv[opts.optind + filenum];
	gzipped = strstr(infile, ".gz") != NULL;
	if (gzipped) {
	    int sts;
	    __pmExecCtl_t *argp = NULL;
	    sts = __pmProcessAddArg(&argp, "gzip");
	    if (sts == 0) sts = __pmProcessAddArg(&argp, "-c");
	    if (sts == 0) sts = __pmProcessAddArg(&argp, "-d");
	    if (sts == 0) sts = __pmProcessAddArg(&argp, infile);
	    if (sts < 0) {
		fprintf(stderr, "Error: __pmProcessAddArg: gzip -c -d %s failed: %s\n",
		    infile, pmErrStr(sts));
		exit(1);
	    }
	    if ((sts = __pmProcessPipe(&argp, "r", PM_EXEC_TOSS_NONE, &fp)) < 0) {
		fprintf(stderr, "Error: __pmProcessPipe: %s failed: %s\n",
		    buf, pmErrStr(sts));
		exit(1);
	    }
	}
	else {
	    if ((fp = fopen(infile, "r")) == NULL) {
		perror(infile);
		exit(1);
	    }
	}

	if (fp == NULL) {
	    pmUsageMessage(&opts);
	    exit(1);
	}

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
	    /*
	     * Fix up the proc/stat kernel debarcle where cmd can have spaces,
             * e.g. : proc:28896 stat 28896 (bash  ) S 2 0 0 0 -1 ....
	     */
	    if (strncmp(buf, "proc:", 5) == 0 && strstr(buf, "stat") != NULL) {
		char *p = strchr(buf, '(');
		char *t = strchr(buf, ')');
		for (; p && t && p < t; p++) {
		    if (*p == ' ')
			*p = '_';
		}
	    }

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

	/* final flush for this collectl input log */
	if ((sts = timestamp_flush()) < 0) {
	    fprintf(stderr, "Error: failed to write final timestamp: %s\n", pmiErrStr(sts));
	    exit(1);
	}

	if (filenum < nfilelist-1) {
	    /*
	     * End of this collectl input log and there's more input files.
	     * Emit a <mark> to signify a temporal gap since the output PCP
	     * archive is effectively a merged archive.
	     */
	    if (vflag)
		fprintf(stderr, "End of collectl input file '%s', <mark> record emitted\n", infile);
	    pmiPutMark();
	}

	if (gzipped)
	    __pmProcessPipeClose(fp);
	else
	    fclose(fp);
    }

    sts = pmiEnd();
    if (unhandled_metric_cnt && vflag)
    	fprintf(stderr, "Warning: %d unhandled metric/values\n", unhandled_metric_cnt);

    exit(0);
}
