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
    { PMLONGOPT_DEBUG,      1, 'D', "OPTS", "set debug options" },
    PMOPT_HOST,		/* -h */
    { "control", 0, 'c', "play pm{Set,Get}DerivedControl() games" },
    { "globlimit", 1, 'l', "limit max # of global derived metrics" },
    { "ctxlimit", 1, 'L', "limit max # of per-context derived metrics" },
    { "private", 0, 'P', "use per-context (private) registration" },
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "a:cD:h:l:M:P?",
    .long_options = longopts,
    .short_usage = "[options] metricname ...",
};

static int	ctx[2] = { -1, -1 };
static const char **namelist = NULL;
static int	numnames = 0;
static pmID	*pmidlist;
static pmDesc	*desclist;

void
add_name(const char *name)
{
    const char	**tmp_names;
    pmID	*tmp_pmids;
    pmDesc	*tmp_descs;

    numnames++;
    tmp_names = (const char **)realloc(namelist, numnames*sizeof(namelist[0]));
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
	    /* nothing, or more accurately pmLookupDesc() below */
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
	sts = pmLookupDescs(numnames, pmidlist, desclist);
	if (sts < 0) {
	    printf("ctx[%d]: pmLookupDescs: %s, skip work\n", c, pmErrStr(sts));
	    continue;
	}
	for (j = m = 0; m < numnames; m++) {
	    printf("ctx[%d]: %s: ", c, namelist[m]);
	    if (pmidlist[m] != PM_ID_NULL && desclist[m].pmid == PM_ID_NULL) {
		/* focus on this one failed PMID for detailed diagnostics */
		if ((sts = pmLookupDesc(pmidlist[m], &desclist[m])) < 0) {
		    printf("pmLookupDesc: %s\n", pmErrStr(sts));
		    continue;
		}
	    }
	    if (pmidlist[m] == PM_ID_NULL) {
		printf("not in PMNS\n");
		continue;
	    }
	    putchar('\n');
	    pmPrintDesc(stdout, &desclist[m]);
	    j++;
	}
	if (j == 0) {
	    printf("ctx[%d]: No valid PMIDs, nothing to fetch\n", c);
	    continue;
	}
	sts = pmFetch(numnames, pmidlist, &rp);
	if (sts < 0) {
	    printf("ctx[%d]: pmFetch: %s, skip work\n", c, pmErrStr(sts));
	    continue;
	}
	for (m = 0; m < numnames; m++) {
	    if (pmidlist[m] == PM_ID_NULL)
		continue;
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

void
set_context_limit(int limit)
{
    int	sts;
    int	c;
    int ctxid = pmWhichContext();

    sts = pmGetDerivedControl(PCP_DERIVED_CONTEXT_LIMIT, &c);
    if (sts < 0)
	printf("ctx %d: pmGetDerivedControl(PCP_DERIVED_CONTEXT_LIMIT, ...): %s\n", ctxid, pmErrStr(sts));
    else
	printf("Context %d limit was: %d\n", ctxid, c);
    sts = pmSetDerivedControl(PCP_DERIVED_CONTEXT_LIMIT, limit);
    if (sts < 0)
	printf("ctx %d: pmSetDerivedControl(PCP_DERIVED_CONTEXT_LIMIT, %d %s\n", ctxid, limit, pmErrStr(sts));
    sts = pmGetDerivedControl(PCP_DERIVED_CONTEXT_LIMIT, &c);
    if (sts < 0)
	printf("ctx %d: pmGetDerivedControl(PCP_DERIVED_CONTEXT_LIMIT, ...): %s\n", ctxid, pmErrStr(sts));
    else
	printf("Context %d limit now: %d\n", ctxid, c);
}

int
main(int argc, char **argv)
{
    int		c;
    char	*p;
    int		sts;
    int		cflag = 0;
    int		lflag = 0;
    int		Mflag = 0;
    int		Pflag = 0;
    int		limit = -1;
    int		Limit = -1;

    pmSetProgname(argv[0]);
    setlinebuf(stdout);
    setlinebuf(stderr);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'c':
	    cflag++;
	    break;

	case 'l':
	    lflag++;
	    limit = atoi(opts.optarg);
	    break;

	case 'M':
	    Mflag++;
	    Limit = atoi(opts.optarg);
	    break;

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

    if (lflag) {
	sts = pmGetDerivedControl(PCP_DERIVED_GLOBAL_LIMIT, &c);
	if (sts < 0)
	    printf("pmGetDerivedControl(PCP_DERIVED_GLOBAL_LIMIT, ...): %s\n", pmErrStr(sts));
	else
	    printf("Global limit was: %d\n", c);
	sts = pmSetDerivedControl(PCP_DERIVED_GLOBAL_LIMIT, limit);
	if (sts < 0)
	    printf("pmSetDerivedControl(PCP_DERIVED_GLOBAL_LIMIT, %d %s\n", limit, pmErrStr(sts));
	sts = pmGetDerivedControl(PCP_DERIVED_GLOBAL_LIMIT, &c);
	if (sts < 0)
	    printf("pmGetDerivedControl(PCP_DERIVED_GLOBAL_LIMIT, ...): %s\n", pmErrStr(sts));
	else
	    printf("Global limit now: %d\n", c);
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
    if (Mflag && Pflag)
	set_context_limit(Limit);
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
    if (cflag) {
	sts = pmSetDerivedControl(PCP_DERIVED_DEBUG_SYNTAX, 1);
	if (sts < 0)
	    printf("ctx[0]: pmSetDerivedControl(PCP_DERIVED_DEBUG_SYNTAX, ...): %s\n", pmErrStr(sts));
	sts = pmGetDerivedControl(PCP_DERIVED_DEBUG_SYNTAX, &c);
	if (sts < 0)
	    printf("ctx[0]: pmGetDerivedControl(PCP_DERIVED_DEBUG_SYNTAX, ...): %s\n", pmErrStr(sts));
	else {
	    if (c != 1)
		printf("ctx[0]: pmGetDerivedControl(PCP_DERIVED_DEBUG_SYNTAX, ...) -> %d not 1 as expected\n", c);
	    printf("ctx[0]: DEBUG_SYNTAX=%d (derive=%d appl0=%d)\n", c, pmDebugOptions.derive, pmDebugOptions.appl0);
	}
    }
    if (Pflag)
	sts = pmAddDerivedMetric(namelist[0], "sample.bin - 50", &p);
    else
	sts = pmRegisterDerivedMetric(namelist[0], "sample.bin - 50", &p);
    if (sts < 0) {
	printf("%s: %s\n", namelist[0], p);
	free(p);
    }
    if (cflag) {
	sts = pmSetDerivedControl(PCP_DERIVED_DEBUG_SYNTAX, 0);
	if (sts < 0)
	    printf("ctx[0]: pmSetDerivedControl(PCP_DERIVED_DEBUG_SYNTAX, ...): %s\n", pmErrStr(sts));
	sts = pmGetDerivedControl(PCP_DERIVED_DEBUG_SYNTAX, &c);
	if (sts < 0)
	    printf("ctx[0]: pmGetDerivedControl(PCP_DERIVED_DEBUG_SYNTAX, ...): %s\n", pmErrStr(sts));
	else {
	    if (c != 0)
	    printf("ctx[0]: pmGetDerivedControl(PCP_DERIVED_DEBUG_SYNTAX, -> %d not 1 as expected\n", c);
	    printf("ctx[0]: DEBUG_SYNTAX=%d (derive=%d appl0=%d)\n", c, pmDebugOptions.derive, pmDebugOptions.appl0);
	}
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
    if (cflag) {
	sts = pmSetDerivedControl(PCP_DERIVED_DEBUG_SEMANTICS, 1);
	if (sts < 0)
	    printf("ctx[1]: pmSetDerivedControl(PCP_DERIVED_DEBUG_SEMANTICS, ...): %s\n", pmErrStr(sts));
	sts = pmGetDerivedControl(PCP_DERIVED_DEBUG_SEMANTICS, &c);
	if (sts < 0)
	    printf("ctx[1]: pmGetDerivedControl(PCP_DERIVED_DEBUG_SEMANTICS, ...): %s\n", pmErrStr(sts));
	else {
	    if (c != 1)
		printf("ctx[1]: pmGetDerivedControl(PCP_DERIVED_DEBUG_SEMANTICS, ...) -> %d not 1 as expected\n", c);
	    printf("ctx[1]: DEBUG_SEMANTICS=%d (derive=%d appl1=%d)\n", c, pmDebugOptions.derive, pmDebugOptions.appl1);
	}
    }
    if (Pflag)
	sts = pmAddDerivedMetric(namelist[1], "sample.long.ten + sample.long.one * 3", &p);
    else
	sts = pmRegisterDerivedMetric(namelist[1], "sample.long.ten + sample.long.one * 3", &p);
    if (sts < 0) {
	printf("%s: %s\n", namelist[1], p);
	free(p);
    }
    if (Pflag)
	sts = pmAddDerivedMetric(namelist[2], "sample.string.hullo", &p);
    else
	sts = pmRegisterDerivedMetric(namelist[2], "sample.string.hullo", &p);
    if (sts < 0) {
	printf("%s: %s\n", namelist[2], p);
	free(p);
    }
    do_work();
    if (cflag) {
	sts = pmSetDerivedControl(PCP_DERIVED_DEBUG_SEMANTICS, 0);
	if (sts < 0)
	    printf("ctx[0]: pmSetDerivedControl(PCP_DERIVED_DEBUG_SEMANTICS, ...): %s\n", pmErrStr(sts));
	sts = pmGetDerivedControl(PCP_DERIVED_DEBUG_SEMANTICS, &c);
	if (sts < 0)
	    printf("ctx[1]: pmGetDerivedControl(PCP_DERIVED_DEBUG_SEMANTICS, ...): %s\n", pmErrStr(sts));
	else {
	    if (c != 0)
		printf("ctx[1]: pmGetDerivedControl(PCP_DERIVED_DEBUG_SEMANTICS, ...) -> %d not 1 as expected\n", c);
	    printf("ctx[1]: DEBUG_SEMANTICS=%d (derive=%d appl1=%d)\n", c, pmDebugOptions.derive, pmDebugOptions.appl1);
	}
    }

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
    if (Pflag)
	sts = pmAddDerivedMetric(namelist[3], "5*sample.long.ten - 8*sample.long.one", &p);
    else
	sts = pmRegisterDerivedMetric(namelist[3], "5*sample.long.ten - 8*sample.long.one", &p);
    if (sts < 0) {
	printf("%s: %s\n", namelist[3], p);
	free(p);
    }
    if (Pflag)
	sts = pmAddDerivedMetric(namelist[4], "sample.bin - sample.bin", &p);
    else
	sts = pmRegisterDerivedMetric(namelist[4], "sample.bin - sample.bin", &p);
    if (sts < 0) {
	printf("%s: %s\n", namelist[4], p);
	free(p);
    }
    if (Pflag)
	sts = pmAddDerivedMetric(namelist[5], "123456", &p);
    else
	sts = pmRegisterDerivedMetric(namelist[5], "123456", &p);
    if (sts < 0) {
	printf("%s: %s\n", namelist[5], p);
	free(p);
    }
    do_work();

    /* 7. dup ctx[1] -> ctx[0] */
    printf("\n=== State 7 ===\n");
    if (cflag) {
	sts = pmSetDerivedControl(PCP_DERIVED_DEBUG_EVAL, 1);
	if (sts < 0)
	    printf("ctx[1]: pmSetDerivedControl(PCP_DERIVED_DEBUG_EVAL, ...): %s\n", pmErrStr(sts));
	sts = pmGetDerivedControl(PCP_DERIVED_DEBUG_EVAL, &c);
	if (sts < 0)
	    printf("ctx[1]: pmGetDerivedControl(PCP_DERIVED_DEBUG_EVAL, ...): %s\n", pmErrStr(sts));
	else {
	    if (c != 1)
		printf("ctx[1]: pmGetDerivedControl(PCP_DERIVED_DEBUG_EVAL, ...) -> %d not 1 as expected\n", c);
	    printf("ctx[1]: DEBUG_EVAL=%d (derive=%d appl2=%d)\n", c, pmDebugOptions.derive, pmDebugOptions.appl2);
	}
    }
    sts = ctx[0] = pmDupContext();
    if (sts < 0) {
	printf("pmDupContext: %s\n", pmErrStr(sts));
	exit(1);
    }
    if (Mflag && Pflag)
	set_context_limit(Limit);
    do_work();
    if (cflag) {
	sts = pmSetDerivedControl(PCP_DERIVED_DEBUG_EVAL, 0);
	if (sts < 0)
	    printf("ctx[0]: pmSetDerivedControl(PCP_DERIVED_DEBUG_EVAL, ...): %s\n", pmErrStr(sts));
	sts = pmGetDerivedControl(PCP_DERIVED_DEBUG_EVAL, &c);
	if (sts < 0)
	    printf("ctx[1]: pmGetDerivedControl(PCP_DERIVED_DEBUG_EVAL, ...): %s\n", pmErrStr(sts));
	else {
	    if (c != 0)
		printf("ctx[1]: pmGetDerivedControl(PCP_DERIVED_DEBUG_EVAL, ...) -> %d not 1 as expected\n", c);
	    printf("ctx[1]: DEBUG_EVAL=%d (derive=%d appl2=%d)\n", c, pmDebugOptions.derive, pmDebugOptions.appl2);
	}
    }

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
