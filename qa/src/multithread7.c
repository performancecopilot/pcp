/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * exercise multi-threaded multiple host contexts with profile and
 * fetch functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pthread.h>

#ifndef HAVE_PTHREAD_BARRIER_T
#include "pthread_barrier.h"
#endif

#define NMETRIC 5

static char	*namelist[NMETRIC] = {
    "sample.colour",
    "pmcd.control.register",
    "sampledso.bin",
    "sample.ulonglong.ten",
    "pmcd.buf.alloc",
};
static pmID	pmidlist[NMETRIC];
static pmDesc	desclist[NMETRIC];

static pthread_barrier_t barrier;

static int ctx1;
static int ctx2;
static int ctx3;

static pthread_mutex_t	mymutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * context use ...
 *
 * args		tid1	tid2	tid3
 *   1		  0	  0	  0
 *   2		  0	  1	  1	<- needs app-level locking
 *   3		  0	  1	  2
 */

/*
 * fetch pmidlist[i] ... pmidlist[NMETRIC-1]
 */
static void
foo(FILE *f, char *fn, int i)
{
    int		sts;
    int		j;
    pmResult	*rp;

    if ((sts = pmFetch(NMETRIC-i, &pmidlist[i], &rp)) < 0) {
	fprintf(f, "%s: %s ...: pmFetch Error: %s\n", fn, namelist[i], pmErrStr(sts));
	pthread_exit("botch");
    }
    fprintf(f, "%s:", fn);
    for (j = 0; j < rp->numpmid; j++)
	fprintf(f, " %s: %d values", namelist[i+j], rp->vset[j]->numval);
    fputc('\n', f);
    pmFreeResult(rp);
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

    j = pmUseContext(ctx1);
    if ( j < 0) {
	fprintf(f, "Error: %s: pmUseContext(%d) -> %s\n", fn, ctx1, pmErrStr(j));
	fclose(f);
	pthread_exit("botch");
    }

    pthread_barrier_wait(&barrier);

    for (j = 0; j < 100; j++) {
	for (i = 0; i < NMETRIC; i++) {
	    foo(f, fn, i);
	}
    }

    fclose(f);
    pthread_exit(NULL);
}

static void *
func2(void *arg)
{
    char	*fn = "func2";
    int		i;
    int		j;
    int		sts;
    FILE	*f;

    if ((f = fopen("/tmp/func2.out", "w")) == NULL) {
	perror("func2 fopen");
	pthread_exit("botch");
    }

    j = pmUseContext(ctx2);
    if ( j < 0) {
	fprintf(f, "Error: %s: pmUseContext(%d) -> %s\n", fn, ctx2, pmErrStr(j));
	fclose(f);
	pthread_exit("botch");
    }

    pthread_barrier_wait(&barrier);

    for (j = 0; j < 100; j++) {
	for (i = NMETRIC-1; i >= 0; i--) {
	    if (ctx2 != ctx1) {
		/*
		 * limit pmcd.control.register [1] in context 2
		 * - select 5 instances below
		 */
		int	instlist[] = { 0, 1, 2, 4, 8 };
		pthread_mutex_lock(&mymutex);
		if ((sts = pmDelProfile(desclist[1].indom, 0, NULL)) < 0) {
		    fprintf(f, "Error: pmDelProfile(%s) -> %s\n", namelist[1], pmErrStr(sts));
		    fclose(f);
		    pthread_exit("botch");
		}
		if ((sts = pmAddProfile(desclist[1].indom, sizeof(instlist)/sizeof(instlist[0]), instlist)) < 0) {
		    fprintf(f, "Error: pmAddProfile(%s) -> %s\n", namelist[1], pmErrStr(sts));
		    fclose(f);
		    pthread_exit("botch");
		}
	    }
	    foo(f, fn, i);
	    if (ctx2 != ctx1)
		pthread_mutex_unlock(&mymutex);
	}
    }

    fclose(f);
    pthread_exit(NULL);
}

static void *
func3(void *arg)
{
    char	*fn = "func3";
    int		i;
    int		j;
    int		sts;
    FILE	*f;

    if ((f = fopen("/tmp/func3.out", "w")) == NULL) {
	perror("func3 fopen");
	pthread_exit("botch");
    }

    j = pmUseContext(ctx3);
    if ( j < 0) {
	fprintf(f, "Error: %s: pmUseContext(%d) -> %s\n", fn, ctx3, pmErrStr(j));
	fclose(f);
	pthread_exit("botch");
    }

    pthread_barrier_wait(&barrier);

    for (j = 0; j < 100; j++) {
	for (i = 0; i < NMETRIC; i += 2) {
	    if (ctx3 != ctx2) {
		/*
		 * limit sampledso.bin [2] in context 3
		 * - exclude instances below, leaving 7 instances 200, ... 800
		 */
		int	instlist[] = { 100, 900 };
		if ((sts = pmAddProfile(desclist[2].indom, 0, NULL)) < 0) {
		    fprintf(f, "Error: pmAddProfile(%s) -> %s\n", namelist[2], pmErrStr(sts));
		    fclose(f);
		    pthread_exit("botch");
		}
		if ((sts = pmDelProfile(desclist[2].indom, sizeof(instlist)/sizeof(instlist[0]), instlist)) < 0) {
		    fprintf(f, "Error: pmDelProfile(%s) -> %s\n", namelist[2], pmErrStr(sts));
		    fclose(f);
		    pthread_exit("botch");
		}
	    }
	    else {
		pthread_mutex_lock(&mymutex);
		if ((sts = pmAddProfile(desclist[1].indom, 0, NULL)) < 0) {
		    fprintf(f, "Error: pmAddProfile(%s) -> %s\n", namelist[1], pmErrStr(sts));
		    fclose(f);
		    pthread_exit("botch");
		}
	    }
	    foo(f, fn, i);
	    if (ctx3 == ctx2)
		pthread_mutex_unlock(&mymutex);
	}
	for (i = 1; i < NMETRIC; i += 2) {
	    /* inherit instance profile from loop above */
	    if (ctx3 == ctx2) {
		pthread_mutex_lock(&mymutex);
		if ((sts = pmAddProfile(desclist[1].indom, 0, NULL)) < 0) {
		    fprintf(f, "Error: pmAddProfile(%s) -> %s\n", namelist[1], pmErrStr(sts));
		    fclose(f);
		    pthread_exit("botch");
		}
	    }
	    foo(f, fn, i);
	    if (ctx3 == ctx2)
		pthread_mutex_unlock(&mymutex);
	}
    }

    fclose(f);
    pthread_exit(NULL);
}

int
main(int argc, char **argv)
{
    pthread_t	tid1;
    pthread_t	tid2;
    pthread_t	tid3;
    int		sts;
    char	*msg;
    int		errflag = 0;
    int		c;
    int		i;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:")) != EOF) {
	switch (c) {

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind == argc || argc-optind > 3) {
	fprintf(stderr, "Usage: %s [-D...] host1 [host2 [host3]]\n", pmProgname);
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
	if (sts < 0)
	    printf("Error: pmLookupName -> %s\n", pmErrStr(sts));
	else
	    printf("Error: pmLookupName returned %d, expected %d\n", sts, NMETRIC);
	for (i = 0; i < NMETRIC; i++) {
	    printf("    %s -> %s\n", namelist[i], pmIDStr(pmidlist[i]));
	}
	exit(1);
    }

    for (i = 0; i < NMETRIC; i++) {
	if ((sts = pmLookupDesc(pmidlist[i], &desclist[i])) < 0) {
	    printf("Error: pmLookupDesc(%s) -> %s\n", namelist[i], pmErrStr(sts));
	    exit(1);
	}
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

    pthread_join(tid1, (void *)&msg);
    if (msg != NULL) printf("tid1: %s\n", msg);
    pthread_join(tid2, (void *)&msg); 
    if (msg != NULL) printf("tid2: %s\n", msg);
    pthread_join(tid3, (void *)&msg); 
    if (msg != NULL) printf("tid3: %s\n", msg);

    exit(0);
}
