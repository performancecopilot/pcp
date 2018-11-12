/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * exercise multi-threaded multiple host contexts with pmLookupDesc()
 * as the simplest possible case
 */

#include <stdio.h>
#include <stdlib.h>
#include <pcp/pmapi.h>
#include <pthread.h>

#ifndef HAVE_PTHREAD_BARRIER_T
#include "pthread_barrier.h"
#endif

#define NMETRIC 5

static char	*namelist[NMETRIC] = {
    "sample.seconds",
    "sampledso.milliseconds",
    "sample.ulonglong.bin_ctr",
    "pmcd.cputime.total",
    "pmcd.buf.alloc",
};
static pmID	pmidlist[NMETRIC];

static pthread_barrier_t barrier;

static int ctx1;
static int ctx2;
static int ctx3;

static void
foo(FILE *f, char *fn, int i)
{
    pmDesc	desc;
    char	strbuf[60];
    int		sts;

    sts = pmLookupDesc(pmidlist[i], &desc);
    if (sts < 0) {
	fprintf(f, "%s: pmLookupDesc[%s] -> %s\n", fn, pmIDStr_r(pmidlist[i], strbuf, sizeof(strbuf)), pmErrStr(sts));
	pthread_exit("botch");
    }
    else if (pmidlist[i] != desc.pmid) {
	fprintf(f, "%s: pmLookupDesc: Expecting PMID: %s", fn, pmIDStr_r(pmidlist[i], strbuf, sizeof(strbuf)));
	fprintf(f, " got: %s\n", pmIDStr_r(desc.pmid, strbuf, sizeof(strbuf)));
	pthread_exit("botch");
    }
    else {
	fprintf(f, "%s: %s (%s) ->", fn, namelist[i], pmIDStr_r(pmidlist[i], strbuf, sizeof(strbuf)));
	fprintf(f, " %s", pmTypeStr_r(desc.type, strbuf, sizeof(strbuf)));
	fprintf(f, " %s", pmInDomStr_r(desc.indom, strbuf, sizeof(strbuf)));
	if (desc.sem == PM_SEM_COUNTER) fprintf(f, " counter");
	else if (desc.sem == PM_SEM_INSTANT) fprintf(f, " instant");
	else if (desc.sem == PM_SEM_DISCRETE) fprintf(f, " discrete");
	else fprintf(f, " sem-%d", desc.sem);
	fprintf(f, " %s\n", pmUnitsStr_r(&desc.units, strbuf, sizeof(strbuf)));
    }
}

static void *
func1(void *arg)
{
    char	*fn = "func1";
    int		i;
    int		j;
    FILE	*f;

    if ((f = fopen("/tmp/func1.out", "w")) == NULL) {
	perror("func1 fopen");
	pthread_exit("botch");
    }
    setlinebuf(f);

    j = pmUseContext(ctx1);
    if (j < 0) {
	fprintf(f, "Error: %s: pmUseContext(%d) -> %s\n", fn, ctx1, pmErrStr(j));
	fclose(f);
	pthread_exit("botch");
    }

    pthread_barrier_wait(&barrier);

    for (j = 0; j < 100; j++) {
	for (i = 0; i < NMETRIC; i++)
	    foo(f, fn, i);
    }

    fclose(f);
    return(NULL);	/* pthread done */
}

static void *
func2(void *arg)
{
    char	*fn = "func2";
    int		i;
    int		j;
    FILE	*f;

    if ((f = fopen("/tmp/func2.out", "w")) == NULL) {
	perror("func2 fopen");
	pthread_exit("botch");
    }
    setlinebuf(f);

    j = pmUseContext(ctx2);
    if ( j < 0) {
	fprintf(f, "Error: %s: pmUseContext(%d) -> %s\n", fn, ctx2, pmErrStr(j));
	fclose(f);
	pthread_exit("botch");
    }

    pthread_barrier_wait(&barrier);

    for (j = 0; j < 100; j++) {
	for (i = NMETRIC-1; i >= 0; i--)
	    foo(f, fn, i);
    }

    fclose(f);
    return(NULL);	/* pthread done */
}

static void *
func3(void *arg)
{
    char	*fn = "func3";
    int		i;
    int		j;
    FILE	*f;

    if ((f = fopen("/tmp/func3.out", "w")) == NULL) {
	perror("func3 fopen");
	pthread_exit("botch");
    }
    setlinebuf(f);

    j = pmUseContext(ctx3);
    if ( j < 0) {
	fprintf(f, "Error: %s: pmUseContext(%d) -> %s\n", fn, ctx3, pmErrStr(j));
	fclose(f);
	pthread_exit("botch");
    }

    pthread_barrier_wait(&barrier);

    for (j = 0; j < 100; j++) {
	for (i = 0; i < NMETRIC; i += 2)
	    foo(f, fn, i);
	for (i = 1; i < NMETRIC; i += 2)
	    foo(f, fn, i);
    }

    fclose(f);
    return(NULL);	/* pthread done */
}

static void
wait_for_thread(char *name, pthread_t tid)
{
    int		sts;
    char	*msg;

    sts = pthread_join(tid, (void *)&msg);
    if (sts == 0) {
	if (msg == PTHREAD_CANCELED)
	    printf("thread %s: pthread_join: cancelled?\n", name);
	else if (msg != NULL)
	    printf("thread %s: pthread_join: %s\n", name, msg);
    }
    else
	printf("thread %s: pthread_join: error: %s\n", name, strerror(sts));
}

int
main(int argc, char **argv)
{
    pthread_t	tid1;
    pthread_t	tid2;
    pthread_t	tid3;
    int		sts;
    int		errflag = 0;
    int		c;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind == argc || argc-optind > 3) {
	fprintf(stderr, "Usage: %s [-D...] host1 [host2 [host3]]\n", pmGetProgname());
	exit(1);
    }

    ctx1 = pmNewContext(PM_CONTEXT_HOST, argv[optind]);
    if (ctx1 < 0) {
	printf("Error: pmNewContext(%s) -> %s\n", argv[optind], pmErrStr(ctx1));
	exit(1);
    }
    optind++;

    if (optind < argc) {
	ctx2 = pmNewContext(PM_CONTEXT_HOST, argv[optind]);
	if (ctx2 < 0) {
	    printf("Error: pmNewContext(%s) -> %s\n", argv[optind], pmErrStr(ctx2));
	    exit(1);
	}
	optind++;
    }
    else
	ctx2 = ctx1;

    if (optind < argc) {
	ctx3 = pmNewContext(PM_CONTEXT_HOST, argv[optind]);
	if (ctx3 < 0) {
	    printf("Error: pmNewContext(%s) -> %s\n", argv[optind], pmErrStr(ctx2));
	    exit(1);
	}
	optind++;
    }
    else
	ctx3 = ctx2;

    sts = pmLookupName(NMETRIC, namelist, pmidlist);
    if (sts != NMETRIC) {
	int	i;
	if (sts < 0)
	    printf("Error: pmLookupName -> %s\n", pmErrStr(sts));
	else
	    printf("Error: pmLookupName returned %d, expected %d\n", sts, NMETRIC);
	for (i = 0; i < NMETRIC; i++) {
	    printf("    %s -> %s\n", namelist[i], pmIDStr(pmidlist[i]));
	}
	exit(1);
    }

    sts = pthread_barrier_init(&barrier, NULL, 3);
    if (sts != 0) {
	printf("pthread_barrier_init: sts=%d\n", sts);
	exit(1);
    }

    sts = pthread_create(&tid1, NULL, func1, NULL);
    if (sts != 0) {
	printf("thread_create: tid1: sts=%d\n", sts);
	exit(1);
    }
    sts = pthread_create(&tid2, NULL, func2, NULL);
    if (sts != 0) {
	printf("thread_create: tid2: sts=%d\n", sts);
	exit(1);
    }
    sts = pthread_create(&tid3, NULL, func3, NULL);
    if (sts != 0) {
	printf("thread_create: tid3: sts=%d\n", sts);
	exit(1);
    }

    wait_for_thread("tid1", tid1);
    wait_for_thread("tid2", tid2);
    wait_for_thread("tid3", tid3);

    exit(0);
}
