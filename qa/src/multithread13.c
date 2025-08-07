/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017-2019 Ken McDonell.  All Rights Reserved.
 *
 * Exercise multi-threaded operation with multiple contexts ...
 * aimed mostly at PM_CONTEXT_LOCAL, but this code should work with
 * any/all context types.
 *
 * We want to fetch sampledso.bin instances with the profile changing
 * across threads and fetches, concurrent with the odd context destroy
 * and create operations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pcp/pmapi.h>
#include <pthread.h>

#ifndef HAVE_PTHREAD_BARRIER_T
#include "pthread_barrier.h"
#endif

static pmLongOptions longopts[] = {
    PMOPT_ARCHIVE,		/* -a */
    PMOPT_DEBUG,		/* -D */
    PMOPT_HOST,			/* -h */
    PMOPT_SPECLOCAL,		/* -K */
    PMOPT_LOCALPMDA,		/* -L */
    PMOPT_SAMPLES,		/* -s */
    PMOPT_HOSTZONE,		/* -z */
    PMOPT_HELP,			/* -? */
    PMAPI_OPTIONS_HEADER("multithread13 options"),
    { "isolate", 0, 'i', "", "isolate output to /tmp/mt13out.N for each thread" },
    { "verbose", 0, 'v', "", "verbose" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "a:D:h:iK:Ls:vz?",
    .long_options = longopts,
    .short_usage = "[options] nthread",
};

int		verbose;	/* number of times -v seen on command line */
int		isolate;	/* 1 to isolate output fom work() */
int		*ctx;		/* one PMAPI context per worker thread */
pthread_t	*tid;		/* thread id for each thread */

/* target metric */
const char	*metric = "sampledso.bin";
pmID		pmid;
pmDesc		desc;

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

static void *
work(void *arg)
{
    int		lctx = *((int *)arg);
    int		sts;
    int		j;
    int		samples = opts.samples;
    int		state;			/* 0 to include instances, 1 to exclude */
    int		inst[3];		/* instances */
    pmResult	*resp;
    int		fopened = 0;
    int		ok;
    FILE	*f;

    if (isolate) {
	char	path[MAXPATHLEN+1];
	snprintf(path, sizeof(path), "/tmp/mt13out.%03d", lctx);
	if ((f = fopen(path, "w")) == NULL) {
	    fprintf(stderr, "Error [%d] fopen(%s) failed: %s\n", lctx, path, pmErrStr(-errno));
	    pthread_exit("botch fopen");
	}
	fopened = 1;
    }
    else
	f = stderr;

    srandom(lctx);	/* different per thread, but deterministic */

#define INST_INCL	0
#define INST_EXCL	1

    if ((sts = pmUseContext(lctx)) < 0) {
	fprintf(f, "Error [%d] Cannot use context: %s\n", lctx, pmErrStr(sts));
	if (fopened) fclose(f);
	pthread_exit("botch pmUseContext");
    }

    if (opts.tzflag) {
	if ((sts = pmNewContextZone()) < 0) {
	    fprintf(f, "Error [%d] Cannot set context timezone: %s\n", lctx, pmErrStr(sts));
	    if (fopened) fclose(f);
	    pthread_exit("botch pmNewContextZone");
	}
    }

    while (samples >= 0) {
	state = random() % 2;
	/*
	 * 9 instances for sampledso.bin, instance ids are 100, 200, ... 900
	 * Pick 3 unique instances.  For INST_INCL, include the first two.
	 * For INST_EXCL, exclude all three.
	 */
	inst[0] = ((random() % 9) + 1) * 100;
	do {
	    inst[1] = ((random() % 9) + 1) * 100;
	} while (inst[0] == inst[1]);
	do {
	    inst[2] = ((random() % 9) + 1) * 100;
	} while (inst[0] == inst[2] || inst[1] == inst[2]);

	if (state == INST_INCL) {
	    if (verbose)
		fprintf(f, "[%d] include: %d %d\n", lctx, inst[0], inst[1]);
	    if ((sts = pmDelProfile(desc.indom, 0, NULL)) < 0) {
		fprintf(f, "Error [%d] pmDelProfile all: %s\n", lctx, pmErrStr(sts));
		if (fopened) fclose(f);
		pthread_exit("botch pmDelProfile all");
	    }
	    if ((sts = pmAddProfile(desc.indom, 2, inst)) < 0) {
		fprintf(f, "Error [%d] pmAddProfile: %s\n", lctx, pmErrStr(sts));
		if (fopened) fclose(f);
		pthread_exit("botch pmAddProfile");
	    }
	}
	else {
	    if (verbose)
		fprintf(f, "[%d] exclude: %d %d %d\n", lctx, inst[0], inst[1], inst[2]);
	    if ((sts = pmAddProfile(desc.indom, 0, NULL)) < 0) {
		fprintf(f, "Error [%d] pmAddProfile all: %s\n", lctx, pmErrStr(sts));
		if (fopened) fclose(f);
		pthread_exit("botch pmAddProfile all");
	    }
	    if ((sts = pmDelProfile(desc.indom, 3, inst)) < 0) {
		fprintf(f, "Error [%d] pmDelProfile: %s\n", lctx, pmErrStr(sts));
		if (fopened) fclose(f);
		pthread_exit("botch pmDelProfile");
	    }
	}
	if ((sts = pmFetch(1, &pmid, &resp)) < 0) {
	    fprintf(f, "Error [%d] pmFetch: %s\n", lctx, pmErrStr(sts));
	    if (fopened) fclose(f);
	    pthread_exit("botch pmFetch");
	}
	if (verbose) {
	    fprintf(f, "[%d] numpmid=%d numval[0]=%d:", lctx, resp->numpmid, resp->vset[0]->numval);
	    for (j = 0; j < resp->vset[0]->numval; j++) {
		fprintf(f, " %d", resp->vset[0]->vlist[j].inst);
	    }
	    fputc('\n', f);
	}
	ok = 1;
	if (resp->numpmid != 1) {
	    fprintf(f, "Error [%d] numpmid=%d not 1 as expected\n", lctx, resp->numpmid);
	    ok = 0;
	}
	else {
	    if (state == INST_INCL && resp->vset[0]->numval != 2) {
		fprintf(f, "Error [%d] include numval=%d not 2 as expected\n", lctx, resp->vset[0]->numval);
		ok = 0;
	    }
	    if (state == INST_EXCL && resp->vset[0]->numval != 6) {
		fprintf(f, "Error [%d] exclude numval=%d not 6 as expected\n", lctx, resp->vset[0]->numval);
		ok = 0;
	    }
	    for (j = 0; j < resp->vset[0]->numval; j++) {
		int	i = resp->vset[0]->vlist[j].inst;
		switch (i) {
		    case 100:
		    case 200:
		    case 300:
		    case 400:
		    case 500:
		    case 600:
		    case 700:
		    case 800:
		    case 900:
			if (state == INST_INCL &&
			    (i != inst[0] && i != inst[1])) {
			    fprintf(f, "Error [%d] include inst=%d not expected\n", lctx, i);
			    ok = 0;
			}
			if (state == INST_EXCL &&
			    (i == inst[0] || i == inst[1] || i == inst[2])) {
			    fprintf(f, "Error [%d] exclude inst=%d not expected\n", lctx, i);
			    ok = 0;
			}
			break;
		    default:
			fprintf(f, "Error [%d] inst=%d is bogus\n", lctx, i);
			ok = 0;
			break;
		}
	    }
	}
	if (ok && verbose)
	    fprintf(f, "[%d] ok\n", lctx);

	if (samples > 0) {
	    samples--;
	    if (samples == 0)
		break;
	}
    }

    if (fopened) fclose(f);
    return(NULL);	/* pthread done */
}

static void
wait_for_thread(int i)
{
    int		sts;
    char	*msg;

    sts = pthread_join(tid[i], (void *)&msg);
    if (sts == 0) {
	if (msg == PTHREAD_CANCELED)
	    printf("thread %d: pthread_join: cancelled?\n", i);
	else if (msg != NULL)
	    printf("thread %d: pthread_join: %s\n", i, msg);
    }
    else
	printf("thread %d: pthread_join: error: %s\n", i, strerror(sts));
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		nthread;
    int		i;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	    case 'i':		/* isolate output */
		isolate = 1;
		break;
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
    nthread = atoi(argv[opts.optind]);
    if (nthread < 1) {
	fprintf(stderr, "%s: nthread (%d) is not valid\n", pmGetProgname(), nthread);
	exit(EXIT_FAILURE);
    }
    if (verbose)
	printf("%s: Running with %d threads ...\n", pmGetProgname(), nthread);

    tid = (pthread_t *)calloc(nthread, sizeof(tid[0]));
    if (tid == NULL) {
	fprintf(stderr, "%s: tid[%d] calloc failed!\n", pmGetProgname(), nthread);
	exit(EXIT_FAILURE);
    }

    ctx = (int *)calloc(nthread, sizeof(ctx[0]));
    if (ctx == NULL) {
	fprintf(stderr, "%s: ctx[%d] calloc failed!\n", pmGetProgname(), nthread);
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

    for (i = 0; i < nthread; i++) {
	ctx[i] = newcontext();
	if ((sts = pmGetContextOptions(ctx[i], &opts)) < 0) {
	    fprintf(stderr, "%s: pmGetContextOptions(%d, ...) failed: %s\n",
		    pmGetProgname(), pmWhichContext(), pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
    }

    if ((sts = pmLookupName(1, &metric, &pmid)) < 0) {
	fprintf(stderr, "%s: pmLookupName: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }

    if ((sts = pmLookupDesc(pmid, &desc)) < 0) {
	fprintf(stderr, "%s: pmLookupDesc: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }

    for (i = 0; i < nthread; i++) {
	sts = pthread_create(&tid[i], NULL, work, &ctx[i]);
	if (sts != 0) {
	    printf("thread_create: %d: sts=%d\n", i, sts);
	    exit(EXIT_FAILURE);
	}
    }

    ;	/* stuff happens */

    for (i = 0; i < nthread; i++) {
	wait_for_thread(i);
    }

    exit(EXIT_SUCCESS);
}
