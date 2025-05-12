/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * EOL and clock advancing in interpolate mode
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

static int	vflag;
static int	tflag;
static int	numpmid;
static pmID	pmidlist[20];
static const char *namelist[20];
static struct timespec	delta = { 0, 500000000 };
static struct timespec	minus_delta = { 0, -500000000 };

static void
cmpres(int n, pmHighResResult *e, pmHighResResult *g)
{
    int		i;
    int		j;
    int		err = 0;

    if (e->timestamp.tv_sec != g->timestamp.tv_sec ||
	e->timestamp.tv_nsec != g->timestamp.tv_nsec) {
	printf("[sample %d] pmResult.timestamp: expected %ld.%09ld, got %ld.%09ld\n",
	    n, (long)e->timestamp.tv_sec, (long)e->timestamp.tv_nsec,
	    (long)g->timestamp.tv_sec, (long)g->timestamp.tv_nsec);
	goto FAILED;
    }
    if (e->numpmid != g->numpmid) {
	printf("[sample %d] pmResult.numpmid: expected %d, got %d\n",
	    n, e->numpmid, g->numpmid);
	goto FAILED;
    }

    for (i = 0; i < e->numpmid; i++) {
	if (e->vset[i]->pmid != g->vset[i]->pmid) {
	    printf("[sample %d] pmResult.vset[%d].pmid: expected %s",
	    n, i, pmIDStr(e->vset[i]->pmid));
	    printf(" got %s\n", pmIDStr(g->vset[i]->pmid));
	    err++;
	}
	if (e->vset[i]->numval != g->vset[i]->numval) {
	    printf("[sample %d] pmResult.vset[%d].numval: expected %d, got %d\n",
		n, i, e->vset[i]->numval, g->vset[i]->numval);
	    err++;
	    continue;
	}
	if (e->vset[i]->valfmt != g->vset[i]->valfmt) {
	    printf("[sample %d] pmResult.vset[%d].valfmt: expected %d, got %d\n",
		n, i, e->vset[i]->valfmt, g->vset[i]->valfmt);
	    err++;
	    continue;
	}
	if (e->vset[i]->valfmt != PM_VAL_INSITU)
	    continue;
	for (j = 0; j < e->vset[i]->numval; j++) {
	    if (e->vset[i]->vlist[j].inst != g->vset[i]->vlist[j].inst) {
		printf("[sample %d] pmResult.vset[%d].vlist[%d].inst: expected %d, got %d\n",
		n, i, j, e->vset[i]->vlist[j].inst, g->vset[i]->vlist[j].inst);
		err++;
	    }
	    if (e->vset[i]->vlist[j].value.lval != g->vset[i]->vlist[j].value.lval) {
		printf("[sample %d] pmResult.vset[%d].vlist[%d].lval: expected %d, got %d\n",
		n, i, j, e->vset[i]->vlist[j].value.lval, g->vset[i]->vlist[j].value.lval);
		err++;
	    }
	}
    }

    if (err == 0)
	return;

FAILED:
    printf("Expected ...\n");
    __pmDumpHighResResult(stdout, e);
    printf("Got ...\n");
    __pmDumpHighResResult(stdout, g);
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		ctx;
    int		errflag = 0;
    char	*archive = "foo";
    char	*namespace = PM_NS_DEFAULT;
    static char	*usage = "[-a archive] [-n namespace] [-T] [-t delta] [-v]";
    int		i;
    int		n;
    pmLogLabel	loglabel;
    pmHighResResult	*resp;
    pmHighResResult	**resvec = malloc(0);
    int		resnum = 0;
    struct timespec	when;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:n:Tt:v")) != EOF) {
	switch (c) {

	case 'a':	/* archive */
	    archive = optarg;
	    break;

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case 'v':	/* verbose output */
	    vflag++;
	    break;

	case 'T':	/* trim namespace */
	    tflag++;
	    break;

	case 't':	/* sample interval */
	    pmtimespecFromReal(atof(optarg), &delta);
	    if (delta.tv_sec > 0) {
		minus_delta.tv_sec = -delta.tv_sec;
		minus_delta.tv_nsec = delta.tv_nsec;
	    }
	    else
		minus_delta.tv_nsec = -delta.tv_nsec;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	printf("Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), namespace, pmErrStr(sts));
	exit(1);
    }

    if ((ctx = pmNewContext(PM_CONTEXT_ARCHIVE, archive)) < 0) {
	printf("%s: Cannot connect to archive \"%s\": %s\n", pmGetProgname(), archive, pmErrStr(ctx));
	exit(1);
    }
    if ((sts = pmGetArchiveLabel(&loglabel)) < 0) {
	printf("%s: pmGetArchiveLabel(%d): %s\n", pmGetProgname(), ctx, pmErrStr(sts));
	exit(1);
    }

    when = loglabel.start;
    if ((sts = pmSetModeHighRes(PM_MODE_INTERP, &when, &delta)) < 0) {
	printf("%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    if (tflag)
	pmTrimNameSpace();

    /*
     * metrics biased towards log "foo"
     */
    i = 0;
    namelist[i++] = "sample.seconds";
    numpmid = i;

    sts = pmLookupName(numpmid, namelist, pmidlist);
    if (sts < 0) {
	printf("pmLookupName: %s\n", pmErrStr(sts));
	for (i = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL)
		printf("	%s - not known\n", namelist[i]);
	}
    }

    printf("\nPass 1: forward scan\n");
    fflush(stdout);
    for (;;) {
	if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
	    if (sts != PM_ERR_EOL)
		printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
	    break;
	}
	resnum++;
	resvec = (pmHighResResult **)realloc(resvec, resnum * sizeof(resvec[0]));
	resvec[resnum - 1] = resp;
	if (vflag)
	    __pmDumpHighResResult(stdout, resp);
	when = resp->timestamp;
    }
    fflush(stderr);
    printf("Found %d samples\n", resnum);

    printf("\nPass 1.1: forwards past EOL\n");
    fflush(stdout);
    for (i = 0; i < 10; i++) {
	if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
	    if (sts != PM_ERR_EOL)
		printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
	}
	pmtimespecInc(&when, &delta);
    }
    fflush(stderr);

    printf("\nPass 1.2: backwards past EOL\n");
    fflush(stdout);
    if ((sts = pmSetModeHighRes(PM_MODE_INTERP, &when, &minus_delta)) < 0) {
        printf("%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
        exit(1);
    }
    for (i = 0; i < 10; i++) {
	if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
	    if (sts != PM_ERR_EOL)
		printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
	}
    }
    fflush(stderr);

    printf("\nPass 2: backwards scan\n");
    fflush(stdout);
    n = 0;
    for (;;) {
	if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
	    if (sts != PM_ERR_EOL)
		printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
	    break;
	}
	n++;
	cmpres(n, resvec[resnum - n], resp);
	when = resp->timestamp;
	pmFreeHighResResult(resp);
    }
    fflush(stderr);
    printf("Found %d samples\n", n);

    printf("\nPass 2.1: backwards prior to SOL\n");
    fflush(stdout);
    for (i = 0; i < 10; i++) {
	if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
	    if (sts != PM_ERR_EOL)
		printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
	} else {
	    pmFreeHighResResult(resp);
	}
	pmtimespecDec(&when, &delta);
    }
    fflush(stderr);

    printf("\nPass 2.2: forwards prior to SOL\n");
    fflush(stdout);
    if ((sts = pmSetModeHighRes(PM_MODE_INTERP, &when, &delta)) < 0) {
        printf("%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
        exit(1);
    }
    for (i = 0; i < 10; i++) {
	if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
	    if (sts != PM_ERR_EOL)
		printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
	}
    }
    fflush(stderr);

    printf("\nPass 3: forwards scan\n");
    fflush(stdout);
    for (n = 0;; n++) {
	if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
	    if (sts != PM_ERR_EOL)
		printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
	    break;
	}
	cmpres(n, resvec[n], resp);
	pmFreeHighResResult(resvec[n]);
	resvec[n] = NULL;
	pmFreeHighResResult(resp);
    }
    fflush(stderr);
    printf("Found %d samples\n", n);
    fflush(stdout);
    if (resvec != NULL) {
	for (i = 0; i < resnum; i++) {
	    if (resvec[i] != NULL) 
		pmFreeHighResResult(resvec[i]);
	}
	free(resvec);
    }

    exit(0);
}
