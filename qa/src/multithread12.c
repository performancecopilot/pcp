/*
 * Copyright (c) 2011-2017 Ken McDonell.  All Rights Reserved.
 *
 * exercise multi-threaded multiple archive contexts
 */

#include <stdio.h>
#include <stdlib.h>
#include <pcp/pmapi.h>
#include <pthread.h>

#ifndef HAVE_PTHREAD_BARRIER_T
#include "pthread_barrier.h"
#endif

static pthread_barrier_t barrier;

static int	pick_me;

static char	*archive_A;
static char	*archive_B;
static char	*archive_C;
static char	*archive_D;

static char	*metric_B = "sample.bin";
static char	*metric_C = "sample.colour";

static int	count_A;
static int	count_B[2];
static int	count_C[2];
static int	count_D[3];

static pmInDom	*indom_D;
static int	numindom_D;

static void
dometric_A(const char *name, void *f)
{
    int		sts;
    pmID	pmid;
    pmDesc	desc;
    char	strbuf[PM_MAXERRMSGLEN];

    sts = pmLookupName(1, (char **)&name, &pmid);
    if (sts < 0) {
	fprintf((FILE *)f, "Error: thread_A: pmLookupName(%s) -> %s\n", name, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	pthread_exit("botch A.3");
    }
    sts = pmLookupDesc(pmid, &desc);
    if (sts < 0) {
	fprintf((FILE *)f, "Error: thread_A: pmLookupDesc(%s) -> %s\n", name, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	pthread_exit("botch A.4");
    }
    count_A++;

    if (pmDebugOptions.desperate) {
	fprintf((FILE *)f, "%s pmid:%s", name, pmIDStr_r(pmid, strbuf, sizeof(strbuf)));
	fprintf((FILE *)f, " indom:%s", pmInDomStr_r(desc.indom, strbuf, sizeof(strbuf)));
	fprintf((FILE *)f, " count=%d\n", count_A);
    }
}

static void *
thread_A(void *arg)
{
    int		iter = *((int *)arg);
    int		ctx;
    int		sts;
    int		i;
    FILE	*f;
    char	strbuf[PM_MAXERRMSGLEN];

    if ((f = fopen("/tmp/thread_A.out", "w")) == NULL) {
	perror("thread_A fopen");
	pthread_exit("botch A.1");
    }
    setlinebuf(f);

    if (pick_me == 0)
	pthread_barrier_wait(&barrier);

    for (i = 0; i < iter; i++) {
	ctx = pmNewContext(PM_CONTEXT_ARCHIVE, archive_A);
	if (ctx < 0) {
	    fprintf(f, "Error: thread_A: pmNewContext(%s) -> %s\n", archive_A, pmErrStr(ctx));
	    pthread_exit("botch A.2");
	}
	count_A = 0;
	sts = pmTraversePMNS_r("", dometric_A, f);
	if (sts < 0) {
	    fprintf(f, "Error: thread_A: pmTraversePMNS_r() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch A.5");
	}
	fprintf(f, "%d pmDescs found\n", count_A);
	sts = pmDestroyContext(ctx);
	if (sts < 0) {
	    fprintf(f, "Error: thread_A: pmDestroyContext(%d) -> %s\n", ctx, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch A.6");
	}
    }

    fclose(f);
    return(NULL);	/* pthread done */
}

static void *
thread_B(void *arg)
{
    int		iter = *((int *)arg);
    int		ctx;
    int		ctx2;
    pmID	pmid;
    pmResult	*rp;
    int		sts;
    int		i;
    FILE	*f;
    char	strbuf[PM_MAXERRMSGLEN];

    if ((f = fopen("/tmp/thread_B.out", "w")) == NULL) {
	perror("thread_B fopen");
	pthread_exit("botch B.1");
    }
    setlinebuf(f);

    if (pick_me == 0)
	pthread_barrier_wait(&barrier);

    for (i = 0; i < iter; i++) {
	ctx = pmNewContext(PM_CONTEXT_ARCHIVE, archive_B);
	if (ctx < 0) {
	    fprintf(f, "Error: thread_B: iter %d: pmNewContext(%s) -> %s\n", i, archive_B, pmErrStr(ctx));
	    pthread_exit("botch B.2");
	}

	ctx2 = pmDupContext();
	if (ctx2 < 0) {
	    fprintf(f, "Error: thread_B: iter %d: pmDupContext(%s) -> %s\n", i, archive_B, pmErrStr(ctx2));
	    pthread_exit("botch B.3");
	}

	sts = pmLookupName(1, (char **)&metric_B, &pmid);
	if (sts < 0) {
	    fprintf(f, "Error: thread_B: iter %d: pmLookupName(%s) -> %s\n", i, metric_B, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch B.4");
	}

	count_B[0] = count_B[1] = 0;

	while ((sts = pmFetch(1, &pmid, &rp)) >= 0) {
	    count_B[0]++;
	    count_B[1] += rp->vset[0]->numval;
	    pmFreeResult(rp);
	}
	if (sts != PM_ERR_EOL) {
	    fprintf(f, "Warning: thread_B: iter %d: pmFetch() -> %s not PM_ERR_EOL\n", i, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	}
	fprintf(f, "%d pmResults and %d values found for %s\n", count_B[0], count_B[1], metric_B);

	sts = pmDestroyContext(ctx2);
	if (sts < 0) {
	    fprintf(f, "Error: thread_B: iter %d: pmDestroyContext(%d (dup)) -> %s\n", i, ctx2, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch B.5");
	}

	sts = pmDestroyContext(ctx);
	if (sts < 0) {
	    fprintf(f, "Error: thread_B: iter %d: pmDestroyContext(%d) -> %s\n", i, ctx, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch B.6");
	}

    }

    fclose(f);
    return(NULL);	/* pthread done */
}

static void *
thread_C(void *arg)
{
    int		iter = *((int *)arg);
    int		ctx;
    struct timeval tend = {0x7fffffff, 0};
    pmID	pmid;
    pmResult	*rp;
    int		sts;
    int		i;
    FILE	*f;
    char	strbuf[PM_MAXERRMSGLEN];

    if ((f = fopen("/tmp/thread_C.out", "w")) == NULL) {
	perror("thread_C fopen");
	pthread_exit("botch C.1");
    }
    setlinebuf(f);

    if (pick_me == 0)
	pthread_barrier_wait(&barrier);

    for (i = 0; i < iter; i++) {
	ctx = pmNewContext(PM_CONTEXT_ARCHIVE, archive_C);
	if (ctx < 0) {
	    fprintf(f, "Error: thread_C: iter %d: pmNewContext(%s) -> %s\n", i, archive_C, pmErrStr(ctx));
	    pthread_exit("botch C.2");
	}

	sts = pmLookupName(1, (char **)&metric_C, &pmid);
	if (sts < 0) {
	    fprintf(f, "Error: thread_C: iter %d: pmLookupName(%s) -> %s\n", i, metric_C, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch C.3");
	}

	sts = pmSetMode(PM_MODE_BACK, &tend, 0);
	if (sts < 0) {
	    fprintf(f, "Error: thread_C: iter %d: pmSetMode(%s) -> %s\n", i, archive_C, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch C.4");
	}

	count_C[0] = count_C[1] = 0;

	while ((sts = pmFetch(1, &pmid, &rp)) >= 0) {
	    count_C[0]++;
	    count_C[1] += rp->vset[0]->numval;
	    pmFreeResult(rp);
	}
	if (sts != PM_ERR_EOL) {
	    fprintf(f, "Warning: thread_C: iter %d: pmFetch() -> %s not PM_ERR_EOL\n", i, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	}
	fprintf(f, "%d pmResults and %d values found for %s\n", count_C[0], count_C[1], metric_C);

	sts = pmDestroyContext(ctx);
	if (sts < 0) {
	    fprintf(f, "Error: thread_C: iter %d: pmDestroyContext(%d) -> %s\n", i, ctx, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch C.6");
	}

    }

    fclose(f);
    return(NULL);	/* pthread done */
}

/*
 * Note:
 * 	if PMDAs are badly behaved (like the NET_ADDR_INDOM indom for
 * 	the Linux PMDA), or instance ids recycled over a longer period
 * 	of time (like the proc PMDA), then the number of instances in
 * 	the archive's metadata file (as reported by pmdumplog -i) may
 * 	be larger than the counts returned via pmGetInDomArchive()
 * 	below because the duplicates map onto one instlist[] entry.
 */
static void
dometric_D(const char *name, void *f)
{
    int		sts;
    pmID	pmid;
    pmDesc	desc;
    int		i;
    pmInDom	*indom_D_new;
    int		*instlist;
    char	**namelist;
    char	strbuf[PM_MAXERRMSGLEN];

    sts = pmLookupName(1, (char **)&name, &pmid);
    if (sts < 0) {
	fprintf((FILE *)f, "Error: thread_D: pmLookupName(%s) -> %s\n", name, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	pthread_exit("botch D.3");
    }
    sts = pmLookupDesc(pmid, &desc);
    if (sts < 0) {
	fprintf((FILE *)f, "Error: thread_D: pmLookupDesc(%s) -> %s\n", name, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	pthread_exit("botch D.4");
    }
    if (desc.indom == PM_INDOM_NULL)
	return;
    count_D[0]++;	/* one more metric with an instance domain */

    for (i = 0; i < numindom_D; i++) {
	if (desc.indom == indom_D[i])
	    return;
    }

    /* first time for this instance domain */
    numindom_D++;
    indom_D_new = (pmInDom *)realloc(indom_D, numindom_D * sizeof(indom_D[0]));
    if (indom_D_new == NULL) {
	fprintf((FILE *)f, "Error: thread_D: realloc(%d) -> %s\n", (int)(numindom_D * sizeof(indom_D[0])), pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	pthread_exit("botch D.5");
    }
    indom_D = indom_D_new;
    indom_D[numindom_D-1] = desc.indom;
    count_D[1]++;	/* one more instance domain */

    sts = pmGetInDomArchive(desc.indom, &instlist, &namelist);
    /*
     * Note:
     * 		PM_ERR_INDOM_LOG is sort of expected if the metric is well
     * 		behaved but does not have any values at present, so no values
     * 		in the archive and there is no InDom in the archive;
     * 		e.g. pmcd.pmie.* in the archives/multi archives
     */
    if (sts < 0 && sts != PM_ERR_INDOM_LOG) {
	fprintf((FILE *)f, "Error: thread_D: pmGetInDomArchive(%s)", pmInDomStr_r(desc.indom, strbuf, sizeof(strbuf)));
	fprintf((FILE *)f, " -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	pthread_exit("botch D.6");
    }
    if (sts >= 0) {
	if (pmDebugOptions.desperate) {
	    fprintf((FILE *)f, "InDom: %s, %d instances\n", pmInDomStr_r(desc.indom, strbuf, sizeof(strbuf)), sts);
	    for (i = 0; i < sts; i++)
		fprintf((FILE *)f, "%d ", instlist[i]);
	    fputc('\n', (FILE *)f);
	}
	count_D[2] += sts;
	free(instlist);
	free(namelist);
    }
}

static void *
thread_D(void *arg)
{
    int		iter = *((int *)arg);
    int		ctx;
    int		sts;
    int		i;
    FILE	*f;
    char	strbuf[PM_MAXERRMSGLEN];

    if ((f = fopen("/tmp/thread_D.out", "w")) == NULL) {
	perror("thread_D fopen");
	pthread_exit("botch D.1");
    }
    setlinebuf(f);

    if (pick_me == 0)
	pthread_barrier_wait(&barrier);

    for (i = 0; i < iter; i++) {
	ctx = pmNewContext(PM_CONTEXT_ARCHIVE, archive_D);
	if (ctx < 0) {
	    fprintf(f, "Error: thread_D: pmNewContext(%s) -> %s\n", archive_D, pmErrStr(ctx));
	    pthread_exit("botch D.2");
	}
	if (indom_D != NULL) {
	    free(indom_D);
	    indom_D = NULL;
	    numindom_D = 0;
	}
	count_D[0] = count_D[1] = count_D[2] = 0;
	sts = pmTraversePMNS_r("", dometric_D, f);
	if (sts < 0) {
	    fprintf(f, "Error: thread_D: pmTraversePMNS_r() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch D.7");
	}

	fprintf(f, "%d non-singular metrics with %d indoms and %d instances found\n", count_D[0], count_D[1], count_D[2]);
	sts = pmDestroyContext(ctx);
	if (sts < 0) {
	    fprintf(f, "Error: thread_D: pmDestroyContext(%d) -> %s\n", ctx, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch D.8");
	}
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
    pthread_t	tid_A;
    pthread_t	tid_B;
    pthread_t	tid_C;
    pthread_t	tid_D;
    int		iter_A = 10;
    int		iter_B = 10;
    int		iter_C = 10;
    int		iter_D = 10;
    int		sts;
    char	*endnum;
    int		errflag = 0;
    int		c;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:b:B:c:C:d:D:")) != EOF) {
	switch (c) {

	case 'a':
	    iter_A = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || iter_A < 0) {
		fprintf(stderr, "%s: -a requires numeric argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 'b':
	    iter_B = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || iter_B < 0) {
		fprintf(stderr, "%s: -b requires numeric argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 'B':
	    metric_B = optarg;
	    break;

	case 'c':
	    iter_C = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || iter_C < 0) {
		fprintf(stderr, "%s: -c requires numeric argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 'C':
	    metric_C = optarg;
	    break;

	case 'd':
	    iter_D = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || iter_D < 0) {
		fprintf(stderr, "%s: -d requires numeric argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

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

    if (errflag || optind == argc || argc-optind > 4) {
	fprintf(stderr, "Usage: %s [options] archive1 [archive2 [archive3 [archive4]]]\n", pmGetProgname());
	fprintf(stderr, "options:\n");
	fprintf(stderr, "  -a iter	iteration count for thread A [default 10]\n");
	fprintf(stderr, "  -b iter	iteration count for thread B [default 10]\n");
	fprintf(stderr, "  -B metric	metric for thread B [sample.bin]\n");
	fprintf(stderr, "  -c iter	iteration count for thread C [default 10]\n");
	fprintf(stderr, "  -C metric	metric for thread C [sample.colour]\n");
	fprintf(stderr, "  -d iter	iteration count for thread D [default 10]\n");
	fprintf(stderr, "  -D debug\n");
	fprintf(stderr, "\n-D appl0 for thread A alone, -Dappl1 for thread B alone,\n");
	fprintf(stderr, "-D appl0,appl1 for thread C alone, -Dappl2 for thread D alone,\n");
	exit(1);
    }

    if (pmDebugOptions.appl0) pick_me |= 1;
    if (pmDebugOptions.appl1) pick_me |= 2;
    if (pmDebugOptions.appl2) pick_me |= 4;

    if (optind < argc) {
	archive_A = argv[optind];
	optind++;
    }
    else {
	fprintf(stderr, "Botch #1\n");
	exit(1);
    }

    if (optind < argc) {
	archive_B = argv[optind];
	optind++;
    }
    else
	archive_B = archive_A;

    if (optind < argc) {
	archive_C = argv[optind];
	optind++;
    }
    else
	archive_C = archive_B;

    if (optind < argc) {
	archive_D = argv[optind];
	optind++;
    }
    else
	archive_D = archive_C;

    if (pick_me == 0) {
	sts = pthread_barrier_init(&barrier, NULL, 4);
	if (sts != 0) {
	    printf("pthread_barrier_init: sts=%d\n", sts);
	    exit(1);
	}
    }

    if (pick_me == 0 || pick_me == 1) {
	sts = pthread_create(&tid_A, NULL, thread_A, &iter_A);
	if (sts != 0) {
	    printf("thread_create: tid_A: sts=%d\n", sts);
	    exit(1);
	}
    }
    if (pick_me == 0 || pick_me == 2) {
	sts = pthread_create(&tid_B, NULL, thread_B, &iter_B);
	if (sts != 0) {
	    printf("thread_create: tid_B: sts=%d\n", sts);
	    exit(1);
	}
    }
    if (pick_me == 0 || pick_me == 3) {
	sts = pthread_create(&tid_C, NULL, thread_C, &iter_C);
	if (sts != 0) {
	    printf("thread_create: tid_C: sts=%d\n", sts);
	    exit(1);
	}
    }
    if (pick_me == 0 || pick_me == 4) {
	sts = pthread_create(&tid_D, NULL, thread_D, &iter_D);
	if (sts != 0) {
	    printf("thread_create: tid_D: sts=%d\n", sts);
	    exit(1);
	}
    }

    if (pick_me == 0 || pick_me == 1)
	wait_for_thread("tid_A", tid_A);
    if (pick_me == 0 || pick_me == 2)
	wait_for_thread("tid_B", tid_B);
    if (pick_me == 0 || pick_me == 3)
	wait_for_thread("tid_C", tid_C);
    if (pick_me == 0 || pick_me == 4)
	wait_for_thread("tid_D", tid_D);

    exit(0);
}
