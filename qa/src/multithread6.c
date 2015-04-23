/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * exercise multi-threaded multiple host contexts with instance domain
 * functions
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

static void
foo(FILE *f, char *fn, int i)
{
    int		sts;
    int		j;
    int		numinst;
    int		*inst;
    char	**name;
    char	*tmp;


    if (desclist[i].indom == PM_INDOM_NULL) {
	fprintf(f, "%s: %s: singular\n", fn, namelist[i]);
	return;
    }
    if ((sts = pmGetInDom(desclist[i].indom, &inst, &name)) < 0) {
	fprintf(f, "%s: %s: pmGetInDom Error: %s\n", fn, namelist[i], pmErrStr(sts));
	pthread_exit("botch");
    }
    numinst = sts;
    fprintf(f, "%s: %s: indom contains %d instances: ", fn, namelist[i], numinst);
    for (j = 0; j < numinst; j++) {
	if ((sts = pmNameInDom(desclist[i].indom, inst[j], &tmp)) < 0) {
	    fprintf(f, "\n%s: inst %d: pmNameInDom Error: %s\n", fn, inst[j], pmErrStr(sts));
	    pthread_exit("botch");
	}
	if (strcmp(tmp, name[j]) != 0) {
	    fprintf(f, "\n%s: inst %d: expecting \"%s\", got \"%s\"\n",  fn, inst[j], name[j], tmp);
	    pthread_exit("botch");
	}
	fputc('i', f);
	free(tmp);
    }
    fputc(' ', f);
    for (j = 0; j < numinst; j++) {
	if ((sts = pmLookupInDom(desclist[i].indom, name[j])) < 0) {
	    fprintf(f, "\n%s: inst \"%s\": pmLookupInDom Error: %s\n", fn, name[j], pmErrStr(sts));
	    pthread_exit("botch");
	}
	if (sts != inst[j]) {
	    fprintf(f, "\n%s: inst \"%s\": expecting %d, got %d\n",  fn, name[j], inst[j], sts);
	    pthread_exit("botch");
	}
	fputc('n', f);
    }
    fputc('\n', f);

    free(inst);
    free(name);
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
	    foo(f, fn, i);
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
	for (i = 0; i < NMETRIC; i += 2)
	    foo(f, fn, i);
	for (i = 1; i < NMETRIC; i += 2)
	    foo(f, fn, i);
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
