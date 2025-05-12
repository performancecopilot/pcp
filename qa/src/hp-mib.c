/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/* short test program for hp-mib metrics */

#include <pcp/pmapi.h>
#include "libpcp.h"

static const char *metrics[] = {
    "proc.nprocs",
    "proc.psinfo.pid",
    "proc.psinfo.uid",
    "proc.psinfo.ppid",
    "proc.psinfo.nice",
    "proc.psinfo.ttydev",
    "proc.psinfo.gid",
    "proc.psinfo.pgrp",
    "proc.psinfo.pri",
    "proc.psinfo.oldpri",
    "proc.psinfo.addr",
    "proc.psinfo.cpu",
    "proc.psinfo.uname",
    "proc.psinfo.gname",
    "proc.psinfo.ttyname",
    "proc.pstatus.utime",
    "proc.pstatus.stime",
    "proc.psusage.starttime",
    "proc.pstatus.flags",
    "proc.psinfo.state",
    "proc.psinfo.sname",
    "proc.psinfo.wchan",
    "proc.psinfo.fname",
    "proc.psinfo.psargs",
    "proc.psinfo.time",
    "proc.psusage.tstamp",
    "proc.psusage.starttime",
    "proc.psinfo.clname",
    "proc.psinfo.time",
    "proc.psusage.rss",
    "proc.pscred.suid",
    "proc.psinfo.ttydev",
    "proc.memory.virtual.txt",
    "proc.memory.virtual.dat",
    "proc.memory.virtual.bss",
    "proc.memory.virtual.stack",
    "proc.memory.virtual.shm",
    "proc.memory.physical.txt",
    "proc.memory.physical.dat",
    "proc.memory.physical.bss",
    "proc.memory.physical.stack",
    "proc.memory.physical.shm"
};

#define NMETRICS (sizeof(metrics) / sizeof(metrics[0]))
static pmID pmids[NMETRICS];

static int
int_compare(const void *a, const void *b)
{
    const int x = *(const int *)a;
    const int y = *(const int *)b;
    return x - y;
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	*namespace = PM_NS_DEFAULT;
    pmResult	*result;
    pmDesc      desc;
    int		all_n;
    int		*all_inst;
    char	**all_names;
    static char	*usage = "[-D debugspec] [-n namespace]";

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:n:")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr, "Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }


    /*
     * Startup:
     * a) load the name space
     * b) lookup the metrics in the name space
     * c) establish a context
     * d) get the instance domain identifier
     * [Note: these steps only have to be done once on startup]
     */
    if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), namespace, pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmLookupName(NMETRICS, metrics, pmids)) < 0) {
	printf("%s: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    putenv("PMDA_LOCAL_PROC=");
    if ((sts = pmNewContext(PM_CONTEXT_LOCAL, NULL)) < 0) {
	printf("%s: Cannot make local connection: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmLookupDesc(pmids[1], &desc)) < 0) {
	fprintf(stderr, "pmLookupDesc: %s\n", pmErrStr(sts));
	exit(1);
    }



    /*
     * Enumerate the instance domain
     * [This has to be done before every fetch]
     */
    if (desc.indom != PM_INDOM_NULL) {
	all_n = pmGetInDom(desc.indom, &all_inst, &all_names);
	if (all_n < 0) {
	    printf("%s: pmGetInDom: %s\n", pmGetProgname(), pmErrStr(all_n));
	    exit(1);
	}
    }
    else {
	printf("%s: botch: metric %s (%s) should have an instance domain\n", pmGetProgname(), metrics[1], pmIDStr(pmids[1]));
	exit(1);
    }

    /*
     * sort the instance identifiers
     * [This has to be done before every fetch]
     */
    qsort(all_inst, all_n, sizeof(int), int_compare);

    /*
     * establish an explicit instance profile
     * [This has to be done before every fetch]
     */
    pmDelProfile(desc.indom, 0, (int *)0); /* exclude everything first */
    pmAddProfile(desc.indom, all_n, all_inst); /* and then explicitly include our list of pids */

    /*
     * fetch the desired metrics
     */
    sts = pmFetch(NMETRICS, pmids, &result);
    if (sts < 0) {
	printf("%s: fetch all %d instances : %s\n", pmGetProgname(), all_n, pmErrStr(sts));
	exit(1);
    }



    printf("\n\n-------- Result Dump --------------\n");
    __pmDumpResult(stdout, result);

    /* note: in a loop, you need to free(all_inst); free(all_names); on every iteration */

    free(all_inst);
    free(all_names);
    pmFreeResult(result);
    if ((sts = pmWhichContext()) < 0) {
	printf("%s: pmWhichContext: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    pmDestroyContext(sts);
    exit(0);
}

