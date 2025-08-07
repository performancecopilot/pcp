/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017-2019 Ken McDonell.  All Rights Reserved.
 *
 * Try to reproduce the cockpit error documented as:
 * https://github.com/cockpit-project/cockpit/issues/6108 and
 * https://bugzilla.redhat.com/show_bug.cgi?id=1235962
 * and most recently associated with
 * https://github.com/performancecopilot/pcp/pull/693
 *
 * Truncated stack trace at failure looks like this for F22:
 *   Thread no. 1 (10 frames)
 *   #0 __pmFindProfile at profile.c:144
 *   #1 __pmInProfile at profile.c:163
 *   #2 __pmdaNextInst at callback.c:148
 *   #3 pmdaFetch at callback.c:514
 *   #4 linux_fetch at pmda.c:5714
 *   #5 __pmFetchLocal at fetchlocal.c:131
 *   #6 pmFetch at fetch.c:147
 *   #7 cockpit_pcp_metrics_tick at src/bridge/cockpitpcpmetrics.c:349
 *   #8 on_timeout_tick at src/bridge/cockpitmetrics.c:178
 *   #13 g_main_context_iteration at gmain.c:3869
 *
 * This code is based on multithread13.c ... an earlier unsuccessful
 * attempt to reproduce the fault.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pcp/pmapi.h>

static pmLongOptions longopts[] = {
    PMOPT_ARCHIVE,		/* -a */
    PMOPT_DEBUG,		/* -D */
    PMOPT_HOST,			/* -h */
    PMOPT_SPECLOCAL,		/* -K */
    PMOPT_LOCALPMDA,		/* -L */
    PMOPT_SAMPLES,		/* -s */
    PMOPT_HOSTZONE,		/* -z */
    PMOPT_HELP,			/* -? */
    PMAPI_OPTIONS_HEADER("profilecrash options"),
    { "verbose", 0, 'v', "", "verbose" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "a:D:h:K:Ls:vz?",
    .long_options = longopts,
    .short_usage = "[options] metric",
};

int		verbose;	/* number of times -v seen on command line */
				/* target metric */
const char	*metric;
pmID		pmid;
pmDesc		desc;
				/* target instance domain */
int		*instlist;
char		**namelist;
int		ninst;

int		ctx1;		/* one PMAPI context with fixed profile */
int		ctx2;		/* one PMAPI context with dynamic profile */

static int
newcontext(void)
{
    int		lctx = -1;
    int		sts;

    if (opts.narchives == 1) {
	if ((lctx = pmNewContext(PM_CONTEXT_ARCHIVE, opts.archives[0])) < 0) {
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), opts.archives[0],  pmErrStr(lctx));
	    exit(EXIT_FAILURE);
	}
	if ((sts = pmSetMode(PM_MODE_FORW, NULL, NULL)) < 0) {
	    fprintf(stderr, "%s: pmSetMode failed for context %d: %s\n",
			pmGetProgname(), pmWhichContext(), pmErrStr(sts));
		exit(EXIT_FAILURE);
	}
    }
    else if (opts.nhosts == 1) {
	if ((lctx = pmNewContext(PM_CONTEXT_HOST, opts.hosts[0])) < 0) {
	    fprintf(stderr, "%s: Cannot connect to pmcd on host \"%s\": %s\n",
		    pmGetProgname(), opts.hosts[0],  pmErrStr(lctx));
	    exit(EXIT_FAILURE);
	}
    }
    else {
	/* local context is the only other option after cmd arg parsing */
	if ((lctx = pmNewContext(PM_CONTEXT_LOCAL, NULL)) < 0) {
	    fprintf(stderr, "%s: Cannot create local context: %s\n",
		    pmGetProgname(), pmErrStr(lctx));
	    exit(EXIT_FAILURE);
	}
    }

    return lctx;
}

/*
 * Check if "i" is a valid internal instance identifier for our
 * instance domain
 */
static int
isvalid(int i)
{
    int		j;
    for (j = 0; j < ninst; j++) {
	if (i == instlist[j])
	    return 1;
    }
    return 0;
}

#define INST_INCL	0
#define INST_EXCL	1

/*
 * the pmFetch, with instances being changed
 */
static void
do_inner(void)
{
    int		sts;
    int		i;
    int		j;
    int		inst[5];	/* instances */
    pmResult	*resp;
    int		ok;
    int		state;		/* 0 to include instances, 1 to exclude */
    static int	ff = 0;		/* flip-flop */

    if (ff) {
	/*
	 * Every second time we're called ...
	 *
	 * First, fetch fixed instances using ctx1
	 */
	if ((sts = pmUseContext(ctx1)) < 0) {
	    fprintf(stderr, "[%d] pmUseContext: %s\n", ctx1, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	if ((sts = pmFetch(1, &pmid, &resp)) < 0) {
	    fprintf(stderr, "[%d] pmFetch: %s\n", ctx1, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	if (verbose) {
	    fprintf(stderr, "[%d] numpmid=%d numval[0]=%d:", ctx1, resp->numpmid, resp->vset[0]->numval);
	    for (j = 0; j < resp->vset[0]->numval; j++) {
		fprintf(stderr, " %d", resp->vset[0]->vlist[j].inst);
	    }
	    fputc('\n', stderr);
	}
	ok = 1;
	if (resp->numpmid != 1) {
	    fprintf(stderr, "Error [%d] numpmid=%d not 1 as expected\n", ctx1, resp->numpmid);
	    ok = 0;
	}
	else {
	    if (resp->vset[0]->numval != 2) {
		fprintf(stderr, "Error [%d] numval=%d not 2 as expected\n", ctx1, resp->vset[0]->numval);
		ok = 0;
	    }
	    for (j = 0; j < resp->vset[0]->numval; j++) {
		i = resp->vset[0]->vlist[j].inst;
		if (i == instlist[0] || i == instlist[ninst-1]) {
		    ;
		}
		else {
		    fprintf(stderr, "Error [%d] inst=%d is bogus\n", ctx1, i);
		    ok = 0;
		}
	    }
	}

	if (ok && verbose)
	    fprintf(stderr, "[%d] ok\n", ctx1);
	pmFreeResult(resp);
    }
    ff = 1 - ff;	/* update flip-flop ready for next call */

    /*
     * Second, fetch some instances using ctx2 and a dynamic (changing)
     * profile.
     */
    if ((sts = pmUseContext(ctx2)) < 0) {
	fprintf(stderr, "[%d] pmUseContext: %s\n", ctx2, pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
    state = random() % 2;
    /*
     * Pick 5 unique instances.  For INST_INCL, include the first two.
     * For INST_EXCL, exclude all five.
     * Note: inst[i] is an index into instlist[], not an instance id.
     */
    for (i = 0; i < 5; i++) {
	while (1) {
	    inst[i] = random() % ninst;
	    for (j = 0; j < i; j++) {
		if (inst[i] == inst[j])
		    goto again;
	    }
	    break;	/* unique */
again:
	    ;
	}
    }
    /*
     * Now make inst[] actual instance ids
     */
    for (i = 0; i < 5; i++) {
	inst[i] = instlist[inst[i]];
    }

    if (state == INST_INCL) {
	if (verbose)
	    fprintf(stderr, "[%d] include: %d %d\n", ctx2, inst[0], inst[1]);
	if ((sts = pmDelProfile(desc.indom, 0, NULL)) < 0) {
	    fprintf(stderr, "[%d] pmDelProfile all: %s\n", ctx2, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	if ((sts = pmAddProfile(desc.indom, 2, inst)) < 0) {
	    fprintf(stderr, "[%d] pmAddProfile: %s\n", ctx2, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
    }
    else {
	if (verbose)
	    fprintf(stderr, "[%d] exclude: %d %d %d %d %d\n", ctx2, inst[0], inst[1], inst[2], inst[3], inst[4]);
	if ((sts = pmAddProfile(desc.indom, 0, NULL)) < 0) {
	    fprintf(stderr, "[%d] pmAddProfile all: %s\n", ctx2, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	if ((sts = pmDelProfile(desc.indom, 5, inst)) < 0) {
	    fprintf(stderr, "[%d] pmDelProfile: %s\n", ctx2, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
    }
    if ((sts = pmFetch(1, &pmid, &resp)) < 0) {
	fprintf(stderr, "[%d] pmFetch: %s\n", ctx2, pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
    if (verbose) {
	fprintf(stderr, "[%d] numpmid=%d numval[0]=%d:", ctx2, resp->numpmid, resp->vset[0]->numval);
	for (j = 0; j < resp->vset[0]->numval; j++) {
	    fprintf(stderr, " %d", resp->vset[0]->vlist[j].inst);
	}
	fputc('\n', stderr);
    }
    ok = 1;
    if (resp->numpmid != 1) {
	fprintf(stderr, "Error [%d] numpmid=%d not 1 as expected\n", ctx2, resp->numpmid);
	ok = 0;
    }
    else {
	if (state == INST_INCL && resp->vset[0]->numval != 2) {
	    fprintf(stderr, "Error [%d] include numval=%d not 2 as expected\n", ctx2, resp->vset[0]->numval);
	    ok = 0;
	}
	if (state == INST_EXCL && resp->vset[0]->numval != ninst-5) {
	    fprintf(stderr, "Error [%d] exclude numval=%d not %d as expected\n", ctx2, resp->vset[0]->numval, ninst-5);
	    ok = 0;
	}
	for (j = 0; j < resp->vset[0]->numval; j++) {
	    i = resp->vset[0]->vlist[j].inst;
	    if (isvalid(i)) {
		if (state == INST_INCL &&
		    (i != inst[0] && i != inst[1])) {
		    fprintf(stderr, "Error [%d] include inst=%d not expected\n", ctx2, i);
		    ok = 0;
		}
		if (state == INST_EXCL &&
		    (i == inst[0] || i == inst[1] || i == inst[2] || i == inst[3] || i == inst[4])) {
		    fprintf(stderr, "Error [%d] exclude inst=%d not expected\n", ctx2, i);
		    ok = 0;
		}
	    }
	    else {
		fprintf(stderr, "Error [%d] inst=%d is bogus\n", ctx2, i);
		ok = 0;
	    }
	}
    }

    if (ok && verbose)
	fprintf(stderr, "[%d] ok\n", ctx2);
    pmFreeResult(resp);
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		outer;		/* outer loop iterations */
    int		inner;		/* inner loop iterations */
    int		fixinst[2];	/* fixed instances */
    char	*slop = NULL;	/* for random malloc/realloc */
    int		sloplen = 0;
    char	*slop_tmp;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	    case 'v':		/* verbosity */
		verbose++;
		break;
	}
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.optind != argc-1) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    if (opts.narchives > 1) {
	fprintf(stderr, "%s: at most one archive allowed\n", pmGetProgname());
	exit(EXIT_FAILURE);
    }
    if (opts.nhosts > 1) {
	fprintf(stderr, "%s: at most one host allowed\n", pmGetProgname());
	exit(EXIT_FAILURE);
    }
    if (opts.narchives == 0 && opts.nhosts == 0 && opts.Lflag == 0) {
	fprintf(stderr, "%s: need one of -a, -h or -L\n", pmGetProgname());
	exit(EXIT_FAILURE);
    }

    metric = argv[argc-1];		/* target metric from command line */

    ctx1 = newcontext();

    if (opts.tzflag) {
	if ((sts = pmNewContextZone()) < 0) {
	    fprintf(stderr, "[%d] Cannot set context timezone: %s\n", ctx2, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
    }

    if ((sts = pmLookupName(1, &metric, &pmid)) < 0) {
	fprintf(stderr, "%s: pmLookupName(%s): %s\n", pmGetProgname(), metric, pmErrStr(sts));
	exit(EXIT_FAILURE);
    }

    if ((sts = pmLookupDesc(pmid, &desc)) < 0) {
	fprintf(stderr, "%s: pmLookupDesc: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
    if (desc.indom == PM_INDOM_NULL) {
	fprintf(stderr, "%s: metric %s: must have an instance domain\n", pmGetProgname(), metric);
	exit(EXIT_FAILURE);
    }
    if (opts.narchives == 1)
	sts = pmGetInDomArchive(desc.indom, &instlist, &namelist);
    else
	sts = pmGetInDom(desc.indom, &instlist, &namelist);
    if (sts < 0) {
	fprintf(stderr, "%s: pmGetInDom: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
    ninst = sts;
    if (ninst < 6) {
	fprintf(stderr, "%s: metric %s: must have at least 6 instances (not %d)\n", pmGetProgname(), metric, ninst);
	exit(EXIT_FAILURE);
    }

    if ((sts = pmDelProfile(desc.indom, 0, NULL)) < 0) {
	fprintf(stderr, "%s: pmDelProfile all: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
    fixinst[0] = instlist[0];
    fixinst[1] = instlist[ninst-1];
    if ((sts = pmAddProfile(desc.indom, 2, fixinst)) < 0) {
	fprintf(stderr, "%s: pmAddProfile: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }

    ctx2 = newcontext();

    outer = opts.samples;

    while (outer >= 0) {
	for (inner = 0; inner < 3; inner++)
	    do_inner();

	if (outer > 0) {
	    outer--;
	    if (outer == 0)
		break;
	}

	/*
	 * now the critical part ..
	 * - destroy the context with the dynamic profile
	 * - muck with malloc and free a bit
	 * - create the context again
	 */
	if ((sts = pmDestroyContext(ctx2)) < 0) {
	    fprintf(stderr, "%s: pmDestroyContext: %s\n", pmGetProgname(), pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	sloplen += random() % 40;
	slop_tmp = realloc(slop, sloplen);
	if (slop_tmp == NULL) {
	    fprintf(stderr, "%s: realloc: %s\n", pmGetProgname(), pmErrStr(-errno));
	    exit(EXIT_FAILURE);
	}
	slop = slop_tmp;

	ctx2 = newcontext();
    }

    free(slop);

    exit(EXIT_SUCCESS);
}
