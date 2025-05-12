/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * exercise archive-based pmFetch and pmSetMode ops in interpolate mode
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

static int	vflag;
static int	tflag;
static int	numpmid;
static pmID	pmidlist[20];
static const char *namelist[20];
static struct timespec	delta = { 0, 500000 };
static struct timespec	minus_delta = { 0, -500000 };

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
    int		ctx[2];
    int		errflag = 0;
    char	*archive = "foo";
    char	*namespace = PM_NS_DEFAULT;
    static char	*usage = "[-a archive] [-n namespace] [-T] [-t delta] [-v]";
    int		i;
    int		j;
    int		k;
    int		n;
    pmLogLabel	loglabel;
    pmHighResResult	*resp;
    pmHighResResult	**resvec = (pmHighResResult **)0;
    int		resnum = 0;
    struct timespec	when;
    int		done;

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

	case 't':	/* sample interval (sec) */
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

    if ((ctx[0] = pmNewContext(PM_CONTEXT_ARCHIVE, archive)) < 0) {
	printf("%s: Cannot connect to archive \"%s\": %s\n", pmGetProgname(), archive, pmErrStr(ctx[0]));
	exit(1);
    }
    if ((sts = pmGetArchiveLabel(&loglabel)) < 0) {
	printf("%s: pmGetArchiveLabel(%d): %s\n", pmGetProgname(), ctx[0], pmErrStr(sts));
	exit(1);
    }

    when = loglabel.start;
    if ((sts = pmSetModeHighRes(PM_MODE_INTERP, &when, &delta)) < 0) {
	printf("%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    if ((ctx[1] = pmDupContext()) < 0) {
        printf("%s: Cannot dup context to archive \"%s\": %s\n", pmGetProgname(), archive, pmErrStr(ctx[0]));
        exit(1);
    }
    if ((sts = pmSetModeHighRes(PM_MODE_INTERP, &when, &delta)) < 0) {
        printf("%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
        exit(1);
    }

    pmUseContext(ctx[0]);
    if (tflag)
	pmTrimNameSpace();

    /*
     * metrics biased towards log "foo"
     */
    i = 0;
    namelist[i++] = "sample.seconds";
    namelist[i++] = "sample.colour";
    namelist[i++] = "sample.drift";
    namelist[i++] = "sample.lights";
    namelist[i++] = "sampledso.milliseconds";
    namelist[i++] = "sampledso.bin";
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
    __pmLogReads = 0;
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
    }
    if (pmDebugOptions.appl0)
	fprintf(stderr, "%d pmLogReads\n", __pmLogReads);
    fflush(stderr);
    printf("Found %d samples\n", resnum);
    fflush(stdout);

    printf("\nPass 2: backwards scan\n");
    __pmLogReads = 0;
    when = resvec[resnum - 1]->timestamp;
    if ((sts = pmSetModeHighRes(PM_MODE_INTERP, &when, &minus_delta)) < 0) {
	printf("%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    n = 0;
    for (;;) {
	if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
	    if (sts != PM_ERR_EOL)
		printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
	    break;
	}
	n++;
	cmpres(n, resvec[resnum - n], resp);
	pmFreeHighResResult(resp);
    }
    if (pmDebugOptions.appl0)
	fprintf(stderr, "%d pmLogReads\n", __pmLogReads);
    fflush(stderr);
    printf("Found %d samples\n", n);
    fflush(stdout);

    printf("\nPass 3: concurrent forwards and backwards scans\n");
    __pmLogReads = 0;
    pmUseContext(ctx[0]);
    when = loglabel.start;
    if ((sts = pmSetModeHighRes(PM_MODE_INTERP, &when, &delta)) < 0) {
	printf("%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    pmUseContext(ctx[1]);
    when = resvec[resnum - 1]->timestamp;
    if ((sts = pmSetModeHighRes(PM_MODE_INTERP, &when, &minus_delta)) < 0) {
	printf("%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    done = 0;
    n = 0;
    while (!done) {
	pmUseContext(ctx[0]);
	if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
	    if (sts != PM_ERR_EOL)
		printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
	    done = 1;
	}
	else {
	    n++;
	    cmpres(n, resvec[n/2], resp);
	    pmFreeHighResResult(resp);
	}

	pmUseContext(ctx[1]);
	if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
	    if (sts != PM_ERR_EOL)
		printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
	    done = 1;
	}
	else {
	    n++;
	    cmpres(n, resvec[resnum - n/2], resp);
	    pmFreeHighResResult(resp);
	}
    }
    if (pmDebugOptions.appl0)
	fprintf(stderr, "%d pmLogReads\n", __pmLogReads);
    fflush(stderr);
    printf("Found %d samples\n", n);
    fflush(stdout);

    printf("\nPass 4: cascading forward scan, 100%%, 75%%, 50%%, 25%%, 0%%\n");
    pmUseContext(ctx[0]);
    for (k = 0; k < 5; k++) {
	__pmLogReads = 0;
	j = 0;
	i = ( k * resnum ) / 4;
	if (i >= resnum)
	    i = resnum - 1;
	when = resvec[i]->timestamp;

	if ((sts = pmSetModeHighRes(PM_MODE_INTERP, &when, &delta)) < 0) {	
	    printf("%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}

	n = i;
	for (;;) {
	    if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
		if (sts != PM_ERR_EOL)
		    printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
		break;
	    }
	    j++;
	    n++;
	    cmpres(j, resvec[n - 1], resp);
	    pmFreeHighResResult(resp);
	}
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "%d pmLogReads\n", __pmLogReads);
	fflush(stderr);
	printf("Found %d samples\n", j);
	fflush(stdout);
    }

    printf("\nPass 5: cascading backward scan, 100%%, 75%%, 50%%, 25%%, 0%%\n");
    pmUseContext(ctx[0]);
    for (k = 0; k < 5; k++) {
	__pmLogReads = 0;
	j = 0;
	i = resnum - ( k * resnum ) / 4 - 1;
	if (i < 0)
	    i = 0;
	when = resvec[i]->timestamp;
	if ((sts = pmSetModeHighRes(PM_MODE_INTERP, &when, &minus_delta)) < 0) {
	    printf("%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}

	for (;;) {
	    if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
		if (sts != PM_ERR_EOL)
		    printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
		break;
	    }
	    cmpres(i, resvec[i], resp);
	    pmFreeHighResResult(resp);
	    i--;
	    j++;
	}
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "%d pmLogReads\n", __pmLogReads);
	fflush(stderr);
	printf("Found %d samples\n", j);
	fflush(stdout);
    }

    printf("\nPass 6: cascading forward/reverse scan, 100%%, 75%%, 50%%, 25%%, 0%%\n");
    pmUseContext(ctx[0]);
    for (k = 4; k >= 0; k--) {
	__pmLogReads = 0;
	j = 0;
	i = ( k * resnum ) / 4;
	if (i >= resnum)
	    i = resnum - 1;

	when = resvec[0]->timestamp;
	if ((sts = pmSetModeHighRes(PM_MODE_INTERP, &when, &delta)) < 0) {	
	    printf("%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}

	for (j = 0; j <= i; j++) {
	    if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
		printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }
	    cmpres(j+1, resvec[j], resp);
	    pmFreeHighResResult(resp);
	}

	when = resvec[i]->timestamp;
	if ((sts = pmSetModeHighRes(PM_MODE_INTERP, &when, &minus_delta)) < 0) {	
	    printf("%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}

	for (j = i; j >= 0; j--) {
	    if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
		printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }
	    cmpres(j+1, resvec[j], resp);
	    pmFreeHighResResult(resp);
	}
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "%d pmLogReads\n", __pmLogReads);
	fflush(stderr);
	printf("Found %d samples\n", i+1);
    }
    printf("Pass 6 done\n");
    fflush(stdout);

    if (resvec != NULL) {
	for (i = 0; i < resnum; i++)
	    pmFreeHighResResult(resvec[i]);
	free(resvec);
    }

    exit(0);
}
