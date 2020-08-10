/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017,2020 Ken McDonell.  All Rights Reserved.
 *
 * Juggling contexts and derived metrics.
 *
 * Metrics exercised is a combination of hard-coded ones
 * - testme.one
 * - testme.two and testme.sub.two
 * - testme.three and testme.sub.three and testme.sub.sub.three
 * and any passed in from the command line.
 *
 * Test involves several state transitions, with all the metrics being
 * exercised (pmLookupName, pmTraversePMNS, pmLookupDesc and pmFetch)
 * across both contexts after each state change:
 * 1. epoch, ctx[0] created
 * 2. dup ctx[0] -> ctx[1]
 * 3. register testme.one (via ctx[0])
 * 4. register testme.two and testme.sub.two (via ctx[1])
 * 5. destroy ctx[0]
 * 6. register testme.three and testme.sub.three and testme.sub.sub.three
 *    (via ctx[1])
 * 7. dup ctx[1] -> ctx[0]
 * 8. destroy ctx[1]
 * 9. destroy ctx[0]
 */

#include <pcp/pmapi.h>

static pmLongOptions longopts[] = {
    PMOPT_ARCHIVE,	/* -a */
    { PMLONGOPT_DEBUG,      1, 'D', "OPTS", "set pmDebug options" },
    PMOPT_HOST,		/* -h */
    { "private", 0, 'P', "use per-context (private) registration" },
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_BOUNDARIES | PM_OPTFLAG_STDOUT_TZ,
    .short_options = PMAPI_OPTIONS "P",
    .long_options = longopts,
    .short_usage = "[options] metricname ...",
};

static int	ctx[2] = { -1, -1 };
static char	**namelist = NULL;
static int	numnames = 0;
static pmID	*pmidlist;
static pmDesc	*desclist;

void
add_name(char *name)
{
    char	**tmp_names;
    pmID	*tmp_pmids;
    pmDesc	*tmp_descs;

    numnames++;
    tmp_names = (char **)realloc(namelist, numnames*sizeof(namelist[0]));
    if (tmp_names == NULL) {
	fprintf(stderr, "add_name: realloc failed for nameslist[%d]\n", numnames);
	exit(1);
    }
    namelist = tmp_names;
    namelist[numnames-1] = name;
    tmp_pmids = (pmID *)realloc(pmidlist, numnames*sizeof(pmidlist[0]));
    if (tmp_pmids == NULL) {
	fprintf(stderr, "add_name: realloc failed for pmidlist[%d]\n", numnames);
	exit(1);
    }
    pmidlist = tmp_pmids;
    tmp_descs = (pmDesc *)realloc(desclist, numnames*sizeof(desclist[0]));
    if (tmp_descs == NULL) {
	fprintf(stderr, "add_name: realloc failed for desclist[%d]\n", numnames);
	exit(1);
    }
    desclist = tmp_descs;
}

void
dometric(const char *name)
{
    printf("pmTraversePMNS found: %s\n", name);
}

void
do_work()
{
    int		c;
    int		m;
    int		j;
    int		sts;
    pmResult	*rp;
    static int	do_first = 0;	/* see comment at the end of function */

    for (c = 0; c < 2; c++) {
	if (ctx[c] < 0) {
	    printf("ctx[%d] < 0, skip work\n", c);
	    continue;
	}
	sts = pmUseContext(ctx[c]);
	if (sts < 0) {
	    printf("ctx[%d]: pmUseContext: %s, skip work\n", c, pmErrStr(sts));
	    continue;
	}
	if (do_first == 0) {
	    /* nothing, or more accurately pmGetDesc() below */
	    ;
	}
	else if (do_first == 1) {
	    sts = pmTraversePMNS("testme", dometric);
	    if (sts < 0)
		printf("ctx[%d]: pmTraversePMNS: %s\n", c, pmErrStr(sts));
	}
	else if (do_first == 2) {
	    char	**chn;
	    sts = pmGetChildren("testme", &chn);
	    if (sts < 0)
		printf("pmGetChildren: %s\n", pmErrStr(sts));
	    else {
		for (j = 0; j < sts; j++) {
		    printf("ctx[%d]: pmGetChldren found: %s\n", c, chn[j]);
		}
		free(chn);
	    }
	}
	sts = pmLookupName(numnames, namelist, pmidlist);
	if (sts < 0) {
	    printf("ctx[%d]: pmLookupName: %s, skip work\n", c, pmErrStr(sts));
	    continue;
	}
	j = 0;
	for (m = 0; m < numnames; m++) {
	    printf("ctx[%d]: %s: ", c, namelist[m]);
	    if (pmidlist[m] == PM_ID_NULL) {
		printf("not in PMNS\n");
		continue;
	    }
	    sts = pmLookupDesc(pmidlist[m], &desclist[m]);
	    if (sts < 0)
		printf("pmLookupDesc: %s\n", pmErrStr(sts));
	    else {
		putchar('\n');
		pmPrintDesc(stdout, &desclist[m]);
		/* only deal with valid PMIDs from here on, this one is OK */
		pmidlist[j++] = pmidlist[m];
	    }
	}
	if (j == 0) {
	    printf("ctx[%d]: No valid PMIDs, nothing to fetch\n", c);
	    continue;
	}
	sts = pmFetch(j, pmidlist, &rp);
	if (sts < 0) {
	    printf("ctx[%d]: pmFetch: %s, skip work\n", c, pmErrStr(sts));
	    continue;
	}
	for (m = 0; m < j; m++) {
	    printf("ctx[%d]: %s: ", c, namelist[m]);
	    if (rp->vset[m]->numval < 0)
		printf("%s\n", pmErrStr(rp->vset[m]->numval));
	    else {
		printf("%d values:", rp->vset[m]->numval);
		int	v;
		for (v = 0; v < rp->vset[m]->numval; v++) {
		    putchar(' ');
		    pmPrintValue(stdout, rp->vset[m]->valfmt, desclist[m].type, &rp->vset[m]->vlist[v], 8);
		}
		putchar('\n');
	    }  
	}
	pmFreeResult(rp);
    }
    /* do nothing, pmTraversePMNS(), pmGetChildren() in turn */
    if (do_first < 2)
	do_first++;
    else
	do_first = 0;
}

int
main(int argc, char **argv)
{
    int		c;
    char	*p;
    int		sts;
    int		Pflag = 0;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'P':	/* use private per-context registrations */
	    Pflag++;
	    break;

	}
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.narchives == 1) {
	if ((ctx[0] = pmNewContext(PM_CONTEXT_ARCHIVE, opts.archives[0])) < 0) {
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), opts.archives[0],  pmErrStr(ctx[0]));
	    exit(EXIT_FAILURE);
	}
	if ((sts = pmGetContextOptions(ctx[0], &opts)) < 0) {
	    pmflush();
	    fprintf(stderr, "%s: pmGetContextOptions(%d, ...) failed: %s\n",
			pmGetProgname(), pmWhichContext(), pmErrStr(sts));
		exit(EXIT_FAILURE);
	}
    }
    else if (opts.narchives > 0) {
	fprintf(stderr, "%s: at most one archive allowed\n", pmGetProgname());
	exit(EXIT_FAILURE);
    }

    if (opts.nhosts == 0 && opts.narchives == 0) { 
	if ((ctx[0] = pmNewContext(PM_CONTEXT_HOST, "local:")) < 0) {
	    fprintf(stderr, "%s: Cannot connect to pmcd on host \"local:\": %s\n",
		    pmGetProgname(), pmErrStr(ctx[0]));
	    exit(EXIT_FAILURE);
	}
    }
    else if (opts.nhosts == 1) {
	if ((ctx[0] = pmNewContext(PM_CONTEXT_HOST, opts.hosts[0])) < 0) {
	    fprintf(stderr, "%s: Cannot connect to pmcd on host \"%s\": %s\n",
		    pmGetProgname(), opts.hosts[0],  pmErrStr(ctx[0]));
	    exit(EXIT_FAILURE);
	}
    }
    else if (opts.nhosts > 0) {
	fprintf(stderr, "%s: at most one host allowed\n", pmGetProgname());
	exit(EXIT_FAILURE);
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    add_name("testme.one");
    add_name("testme.two");
    add_name("testme.sub.two");
    add_name("testme.three");
    add_name("testme.sub.three");
    add_name("testme.sub.sub.three");

    /* non-flag args are argv[opts.optind] ... argv[argc-1] */
    while (opts.optind < argc) {
	add_name(argv[opts.optind]);
	opts.optind++;
    }

    for (c = 0; c < numnames; c++) {
	printf("name[%d] %s\n", c, namelist[c]);
    }

    /* 1. epoch, ctx[0] created */
    printf("\n=== State 1 ===\n");
    do_work();

    /* 2. dup ctx[0] -> ctx[1] */
    printf("\n=== State 2 ===\n");
    sts = ctx[1] = pmDupContext();
    if (sts < 0) {
	printf("pmDupContext: %s\n", pmErrStr(sts));
	exit(1);
    }
    do_work();

    /*
     * 3. register testme.one (via ctx[0]) == sample.bin[] - 50
     */
    printf("\n=== State 3 ===\n");
    sts = pmUseContext(ctx[0]);
    if (sts < 0) {
	printf("ctx[0]: pmUseContext: %s\n", pmErrStr(sts));
	exit(1);
    }
    sts = pmRegisterDerivedMetric(namelist[0], "sample.bin - 50", &p);
    if (sts < 0) {
	printf("%s: %s\n", namelist[0], p);
	free(p);
    }
    do_work();

    /*
     * 4. register testme.two and testme.sub.two (via ctx[1])
     *    == 13 and "hullo world!"
     */
    printf("\n=== State 4 ===\n");
    sts = pmUseContext(ctx[1]);
    if (sts < 0) {
	printf("ctx[1]: pmUseContext: %s\n", pmErrStr(sts));
	exit(1);
    }
    sts = pmRegisterDerivedMetric(namelist[1], "sample.long.ten + sample.long.one * 3", &p);
    if (sts < 0) {
	printf("%s: %s\n", namelist[1], p);
	free(p);
    }
    sts = pmRegisterDerivedMetric(namelist[2], "sample.string.hullo", &p);
    if (sts < 0) {
	printf("%s: %s\n", namelist[2], p);
	free(p);
    }
    do_work();

    /* 5. destroy ctx[0] */
    printf("\n=== State 5 ===\n");
    sts = pmDestroyContext(ctx[0]);
    if (sts < 0) {
	printf("pmDestroyContext(0): %s\n", pmErrStr(sts));
	exit(1);
    }
    do_work();

    /*
     * 6. register testme.three and testme.sub.three and testme.sub.sub.three
     *    (via ctx[1])
     *    == 42, sample.bin[] - sample.bin[], and 123456
     */
    printf("\n=== State 6 ===\n");
    sts = pmRegisterDerivedMetric(namelist[3], "5*sample.long.ten - 8*sample.long.one", &p);
    if (sts < 0) {
	printf("%s: %s\n", namelist[3], p);
	free(p);
    }
    sts = pmRegisterDerivedMetric(namelist[4], "sample.bin - sample.bin", &p);
    if (sts < 0) {
	printf("%s: %s\n", namelist[4], p);
	free(p);
    }
    sts = pmRegisterDerivedMetric(namelist[5], "123456", &p);
    if (sts < 0) {
	printf("%s: %s\n", namelist[5], p);
	free(p);
    }
    do_work();

    /* 7. dup ctx[1] -> ctx[0] */
    printf("\n=== State 7 ===\n");
    sts = ctx[0] = pmDupContext();
    if (sts < 0) {
	printf("pmDupContext: %s\n", pmErrStr(sts));
	exit(1);
    }
    do_work();

    /* 8. destroy ctx[1] */
    printf("\n=== State 8 ===\n");
    sts = pmDestroyContext(ctx[1]);
    if (sts < 0) {
	printf("pmDestroyContext(1): %s\n", pmErrStr(sts));
	exit(1);
    }
    do_work();

    /* 9. destroy ctx[0] */
    printf("\n=== State 8 ===\n");
    sts = pmDestroyContext(ctx[0]);
    if (sts < 0) {
	printf("pmDestroyContext(0): %s\n", pmErrStr(sts));
	exit(1);
    }
    do_work();

    return 0;
}
