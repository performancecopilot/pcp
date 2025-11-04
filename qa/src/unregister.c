/*
 * Started life as a derived metric register and unregister test
 * harness, but sort of morphed into a swiss-army-knife for testing
 * all aspects of derived metrics.
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017,2025 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

static pmLongOptions longopts[] = {
    PMOPT_ARCHIVE,	/* -a */
    { "config", 1, 'c', "FILE", "pmLoadDerivedConfig(FILE)" },
    PMOPT_DEBUG,	/* -D */
    PMOPT_HOST,		/* -h */
    { "add", 1, 'q', "NAME=EXPR", "pmAddDerived(NAME, EXPR)" },
    { "register", 1, 'r', "NAME=EXPR", "pmRegisterDerived(NAME, EXPR)" },
    { "unregister", 1, 'u', "NAME", "pmUnregisterDerived(NAME)" },
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static int overrides(int, pmOptions *);
static pmOptions opts = {
    .short_options = "a:c:D:h:q:r:u:?",
    .long_options = longopts,
    .short_usage = "[options] metric ...",
    .override = overrides,
};

int		numctx = 0;
int		*ctx;

static int
overrides(int opt, pmOptions *optsp)
{
    int		*tmp_ctx;
    int		sts;

    switch (opt) {

	case 'a':	/* archive context */
	    if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, optsp->optarg)) < 0) {
		fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
			pmGetProgname(), optsp->optarg,  pmErrStr(sts));
		break;
	    }
	    goto expand_ctx;

	case 'h':	/* host context */
	    if ((sts = pmNewContext(PM_CONTEXT_HOST, optsp->optarg)) < 0) {
		fprintf(stderr, "%s: Cannot connect to pmcd on host \"%s\": %s\n",
			pmGetProgname(), optsp->optarg,  pmErrStr(sts));
		break;
	    }
expand_ctx:
	    numctx++;
	    tmp_ctx = realloc(ctx, numctx*sizeof(ctx[0]));
	    if (tmp_ctx == NULL) {
		fprintf(stderr, "%s: realloc ctx[] failed for new context -%c %s\n",
			pmGetProgname(), opt, optsp->optarg);
		exit(1);
	    }
	    else {
		ctx = tmp_ctx;
		ctx[numctx-1] = sts;
	    }
	    return 1;

    }
    return 0;
}

int
main(int argc, char **argv)
{
    int		c;
    int		a;
    char	*p;
    int		sts;
    char	*name;
    char	*expr;
    char	*errmsg;
    pmID	pmid;
    pmID	oldpmid = PM_ID_NULL;
    pmDesc	desc;
    pmResult	*rp;

    pmSetProgname(argv[0]);

    /* no derived metrics other than the ones we're playing with */
    setenv("PCP_DERIVED_CONFIG", "", 1);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'c':	/* loadconfig */
	    sts = pmLoadDerivedConfig(opts.optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: LoadDerivedConfig failed for %s: %s\n",
		    pmGetProgname(), name, pmErrStr(sts));
	    }
	    break;	

	case 'q':	/* add (only current context) */
	case 'r':	/* register */
	    /*
	     * no real checking here ... name ends at =, expr starts after =
	     * and ends at \0, no white space allowed other than in expr
	     */
	    name = opts.optarg;
	    for (p = name+1; *p && *p != '='; p++)
		;
	    *p = '\0';
	    expr = p+1;
	    if (c == 'q')
		sts = pmAddDerivedMetric(name, expr, &errmsg);
	    else
		sts = pmRegisterDerivedMetric(name, expr, &errmsg);
	    if (sts < 0) {
		fprintf(stderr, "%s: pmRegisterDerived or pmAddDerived failed for %s=%s: %s\n%s",
		    pmGetProgname(), name, expr, pmErrStr(sts), errmsg);
		free(errmsg);
	    }
	    pmLookupName(1, (const char **)&name, &oldpmid);
	    break;	

	case 'u':	/* unregister */
	    sts = pmUnregisterDerived(opts.optarg);
	    if (sts < 0)
		fprintf(stderr, "%s: pmUnregisterDerived(%s): failed: %s\n",
		    pmGetProgname(), opts.optarg, pmErrStr(sts));
	    else
		fprintf(stderr, "%s: pmUnregisterDerived(%s) => %d\n",
		    pmGetProgname(), opts.optarg, sts);
	    break;	

	}
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
    }

    /* for each context, play with metric names from the command line ... */
    for (c = 0; c < numctx; c++) {
	if ((sts = pmUseContext(ctx[c])) < 0) {
	    printf("%s: pmUseContext(%d) failed: %s\n",
		pmGetProgname(), ctx[c], pmErrStr(sts));
	}
	printf("=== context[%d]=%d\n", c, ctx[c]);
	for (a = opts.optind; a < argc; a++) {
	    name = argv[a];
	    printf("--- %s\n", name);
	    if ((sts = pmLookupName(1, (const char **)&name, &pmid)) < 0) {
		printf("pmLookupName(%s, ...) failed: %s\n",
			name, pmErrStr(sts));
		/* punt on the pmid for the last registered derived metric */
		pmid = oldpmid;
	    }
	    if ((sts = pmLookupDesc(pmid, &desc)) < 0) {
		printf("pmLookupDesc(%s, ...) failed: %s\n",
			name, pmErrStr(sts));
	    }
	    if ((sts = pmFetch(1, &pmid, &rp)) < 0) {
		printf("pmLookupFetch(%s, ...) failed: %s\n",
			name, pmErrStr(sts));
		continue;
	    }
	    __pmDumpResult(stdout, rp);
	    pmFreeResult(rp);
	}
	if ((sts = pmDestroyContext(ctx[c])) < 0) {
	    printf("pmDestroyContext(%d) failed: %s\n",
			ctx[c], pmErrStr(sts));
	}
    }

    return 0;
}
