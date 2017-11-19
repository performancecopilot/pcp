/*
 * exercise multi-threaded concurrent context error paths
 */

#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include <pcp/pmapi.h>

/*
 * do iter host cycles and 10*iter archive cycles
 */
static int iter = 500;

/*
 * for each cycle, call pmNewContext() for NUM_CTX contexts and then
 * call pmDestroyContext() to destroy them in the opposite order to
 * the order of creation.
 */
#define NUM_CTX 10

static void *
liveworker(void *x)
{
    int         i, c, ctx[NUM_CTX], sts;
    char        *host = x;
    char	errmsg[PM_MAXERRMSGLEN];

    for (i=0; i < iter; i++) {
	for (c = 0; c < NUM_CTX; c++) {
	    if ((ctx[c] = pmNewContext(PM_CONTEXT_HOST, host)) < 0) {

		fprintf(stderr, "liveworker: iter %d ctx[%d] create: %s\n", i, c, pmErrStr_r(ctx[c], errmsg, sizeof(errmsg)));
	    }
	}
	for (c = NUM_CTX-1; c >= 0; c--) {
	    if (ctx[c] >= 0) {
		sts = pmDestroyContext(ctx[c]);
		if (sts < 0)
		    fprintf(stderr, "liveworker: iter %d ctx[%d] destroy: %s\n", i, c, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    }
	}
    }
    return NULL;
}

static void *
archworker(void *x)
{
    int         i, c, ctx[NUM_CTX], sts;
    char        *archive = x;
    char	errmsg[PM_MAXERRMSGLEN];

    for (i=0; i < 10*iter; i++) {
	for (c = 0; c < NUM_CTX; c++) {
	    if ((ctx[c] = pmNewContext(PM_CONTEXT_ARCHIVE, archive)) < 0) {
		/*
		 * qa/1096 uses this code with a bad archive, so be silently
		 * tolerant of some failures
		 */
		if (ctx[c] == PM_ERR_NODATA)
		    continue;
		fprintf(stderr, "archworker: iter %d ctx[%d] create: %s\n", i, c, pmErrStr_r(ctx[c], errmsg, sizeof(errmsg)));
	    }
	}
	for (c = NUM_CTX-1; c >= 0; c--) {
	    if (ctx[c] >= 0) {
		sts = pmDestroyContext(ctx[c]);
		if (sts < 0)
		    fprintf(stderr, "archworker: iter %d ctx[%d] destroy: %s\n", i, c, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    }
	}
    }
    return NULL;
}

int
main(int argc, char *argv[])
{
    int sts;
    int errflag = 0;
    int c;
    pthread_t p1, p2;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:i:?")) != EOF) {
	switch (c) {
	case 'D':
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'i':
	    iter = atoi(optarg);
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc-2) {
    	fprintf(stderr, "Usage: %s [-D...] [-i iter] hostname archivename\n", pmGetProgname());
	exit(1);
    }

    pthread_create(&p1, NULL, &liveworker, argv[optind]);
    pthread_create(&p2, NULL, &archworker, argv[optind+1]);
    pthread_join(p2, NULL);
    pthread_join(p1, NULL);

    /* success, if we don't abort first! */
    printf("SUCCESS\n");
    exit(0);
}

