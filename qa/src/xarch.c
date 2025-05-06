/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * exercise archive-based pmFetch and pmSetMode ops
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

static int	vflag;
static int	tflag;
static int	numpmid;
static pmID	pmidlist[20];
static const char *namelist[20];
static const struct timespec microsec = { 0, 1000 };

static void
cmpres(int n, pmHighResResult *e, pmHighResResult *g)
{
    int		i;
    int		j;
    int		err = 0;

    if (e->timestamp.tv_sec != g->timestamp.tv_sec ||
	e->timestamp.tv_nsec != g->timestamp.tv_nsec) {
	printf("[sample %d] pmHighResResult.timestamp: expected %lld.%09ld, got %lld.%09ld\n",
	    n, (long long)e->timestamp.tv_sec, (long)e->timestamp.tv_nsec,
	    (long long)g->timestamp.tv_sec, (long)g->timestamp.tv_nsec);
	goto FAILED;
    }
    if (e->numpmid != g->numpmid) {
	printf("[sample %d] pmHighResResult.numpmid: expected %d, got %d\n",
	    n, e->numpmid, g->numpmid);
	goto FAILED;
    }

    for (i = 0; i < e->numpmid; i++) {
	if (e->vset[i]->pmid != g->vset[i]->pmid) {
	    printf("[sample %d] pmHighResResult.vset[%d].pmid: expected %s",
			n, i, pmIDStr(e->vset[i]->pmid));
	    printf(" got %s\n", pmIDStr(g->vset[i]->pmid));
	    err++;
	}
	if (e->vset[i]->numval != g->vset[i]->numval) {
	    printf("[sample %d] pmHighResResult.vset[%d].numval: expected %d, got %d\n",
		n, i, e->vset[i]->numval, g->vset[i]->numval);
	    err++;
	    continue;
	}
	if (e->vset[i]->valfmt != g->vset[i]->valfmt) {
	    printf("[sample %d] pmHighResResult.vset[%d].valfmt: expected %d, got %d\n",
		n, i, e->vset[i]->valfmt, g->vset[i]->valfmt);
	    err++;
	    continue;
	}
	if (e->vset[i]->valfmt != PM_VAL_INSITU)
	    continue;
	for (j = 0; j < e->vset[i]->numval; j++) {
	    if (e->vset[i]->vlist[j].inst != g->vset[i]->vlist[j].inst) {
		printf("[sample %d] pmHighResResult.vset[%d].vlist[%d].inst: expected %d, got %d\n",
		n, i, j, e->vset[i]->vlist[j].inst, g->vset[i]->vlist[j].inst);
		err++;
	    }
	    if (e->vset[i]->vlist[j].value.lval != g->vset[i]->vlist[j].value.lval) {
		printf("[sample %d] pmHighResResult.vset[%d].vlist[%d].lval: expected %d, got %d\n",
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
    static char	*usage = "[-D debugspec] [-a archive] [-n namespace] [-v] [metric ...]";
    int		i;
    int		j;
    int		k;
    int		n;
    int		done;
    int		resnum = 0;
    pmLogLabel	loglabel;
    pmHighResResult	*resp;
    pmHighResResult	**resvec = NULL;
    struct timespec	when;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:n:tv")) != EOF) {
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

	case 't':	/* trim namespace */
	    tflag++;
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

    when.tv_sec = 0;
    when.tv_nsec = 0;
    if ((sts = pmSetModeHighRes(PM_MODE_FORW, &when, NULL)) < 0) {
	printf("%s: pmSetModeHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    if ((ctx[1] = pmDupContext()) < 0) {
        printf("%s: Cannot dup context to archive \"%s\": %s\n", pmGetProgname(), archive, pmErrStr(ctx[0]));
        exit(1);
    }
    if ((sts = pmSetModeHighRes(PM_MODE_FORW, &when, NULL)) < 0) {
        printf("%s: pmSetModeHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
        exit(1);
    }

    pmUseContext(ctx[0]);
    if (tflag)
	pmTrimNameSpace();

    i = 0;
    while (optind < argc - 1) {
	if (i == 20) {
	    fprintf(stderr, "%s: Too many metrics, re-build me\n", pmGetProgname());
	    exit(1);
	}
	namelist[i++] = argv[optind]++;
    }

    if (i == 0) {
	/*
	 * default metrics biased towards log "foo"
	 */
	namelist[i++] = "sample.seconds";
	namelist[i++] = "sample.colour";
	namelist[i++] = "sample.drift";
	namelist[i++] = "sample.lights";
	namelist[i++] = "sampledso.milliseconds";
	namelist[i++] = "sampledso.bin";
    }
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
    for (;;) {
	if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
	    if (sts != PM_ERR_EOL)
		printf("%s: pmFetchHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
	    break;
	}
	resnum++;
	resvec = (pmHighResResult **)realloc(resvec, resnum * sizeof(resvec[0]));
	resvec[resnum - 1] = resp;
	if (vflag)
	    __pmDumpHighResResult(stdout, resp);
    }
    fflush(stderr);
    printf("Found %d samples\n", resnum);
    fflush(stdout);

    printf("\nPass 2: backwards scan\n");
    when.tv_sec = PM_MAX_TIME_T;
    when.tv_nsec = 0;
    if ((sts = pmSetModeHighRes(PM_MODE_BACK, &when, 0)) < 0) {
	printf("%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    n = 0;
    for (;;) {
	if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
	    if (sts != PM_ERR_EOL)
		printf("%s: pmFetchHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
	    break;
	}
	n++;
	if (n > resnum) {
	    printf("%s: botch: found %d records forwards, but now have seen %d records backwards\n", pmGetProgname(), resnum, n);
	    exit(1);
	}
	cmpres(n, resvec[resnum - n], resp);
	pmFreeHighResResult(resp);
    }
    fflush(stderr);
    printf("Found %d samples\n", n);
    fflush(stdout);

    printf("\nPass 3: concurrent forwards and backwards scans\n");
    pmUseContext(ctx[0]);
    when.tv_sec = 0;
    when.tv_nsec = 0;
    if ((sts = pmSetModeHighRes(PM_MODE_FORW, &when, NULL)) < 0) {
	printf("%s: pmSetModeHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    pmUseContext(ctx[1]);
    when.tv_sec = PM_MAX_TIME_T;
    when.tv_nsec = 0;
    if ((sts = pmSetModeHighRes(PM_MODE_BACK, &when, 0)) < 0) {
	printf("%s: pmSetModeHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    done = 0;
    n = 0;
    while (!done) {
	pmUseContext(ctx[0]);
	if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
	    if (sts != PM_ERR_EOL)
		printf("%s: pmFetchHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
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
		printf("%s: pmFetchHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
	    done = 1;
	}
	else {
	    n++;
	    cmpres(n, resvec[resnum - n/2], resp);
	    pmFreeHighResResult(resp);
	}
    }
    fflush(stderr);
    printf("Found %d samples\n", n);
    fflush(stdout);

    printf("\nPass 4: cascading forward scan, 100%%, 75%%, 50%%, 25%%, 0%%\n");
    pmUseContext(ctx[0]);
    for (k = 0; k < 5; k++) {
	j = 0;
	if (resnum > 0) {
	    i = ( k * resnum ) / 4;
	    if (i <= resnum - 1) {
		when = resvec[i]->timestamp;
		when.tv_nsec -= microsec.tv_nsec;
		if (when.tv_nsec < 0) {
		    when.tv_nsec = 0;
		    when.tv_sec--;
		}
	    }
	    else {
		when = resvec[resnum - 1]->timestamp;
		pmtimespecInc(&when, &microsec);
	    }
	    if ((sts = pmSetModeHighRes(PM_MODE_FORW, &when, NULL)) < 0) {
		printf("%s: pmSetModeHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }

	    n = i;
	    for (;;) {
		if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
		    if (sts != PM_ERR_EOL)
			printf("%s: pmFetchHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
		    break;
		}
		j++;
		n++;
		cmpres(j, resvec[n - 1], resp);
		pmFreeHighResResult(resp);
	    }
	    fflush(stderr);
	}
	printf("Found %d samples\n", j);
	fflush(stdout);
    }

    printf("\nPass 5: cascading backward scan, 100%%, 75%%, 50%%, 25%%, 0%%\n");
    pmUseContext(ctx[0]);
    for (k = 0; k < 5; k++) {
	j = 0;
	if (resnum > 0) {
	    i = resnum - ( k * resnum ) / 4 - 1;
	    if (i < 0)
		i = 0;
	    if (i > 0) {
		when = resvec[i]->timestamp;
		pmtimespecInc(&when, &microsec);
	    }
	    else {
		when = resvec[0]->timestamp;
		when.tv_nsec -= microsec.tv_nsec;
		if (when.tv_nsec < 0) {
		    when.tv_nsec = 0;
		    when.tv_sec--;
		}
	    }
	    if ((sts = pmSetModeHighRes(PM_MODE_BACK, &when, NULL)) < 0) {
		printf("%s: pmSetModeHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }

	    for (;;) {
		if ((sts = pmFetchHighRes(numpmid, pmidlist, &resp)) < 0) {
		    if (sts != PM_ERR_EOL)
			printf("%s: pmFetchHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
		    break;
		}
		cmpres(i, resvec[i], resp);
		pmFreeHighResResult(resp);
		i--;
		j++;
	    }
	    fflush(stderr);
	}
	printf("Found %d samples\n", j);
	fflush(stdout);
    }

    printf("\nPass 6: EOL tests, forwards and backwards\n");
    when = loglabel.start;
    when.tv_sec--;
    if ((sts = pmSetModeHighRes(PM_MODE_BACK, &when, NULL)) < 0) {
	printf("%s: pmSetModeHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmFetchHighResArchive(&resp)) < 0) {
	if (sts != PM_ERR_EOL)
	    printf("%s: pmFetchHighResArchive: %s\n", pmGetProgname(), pmErrStr(sts));
    }
    else {
	printf("%s: pmFetchHighResArchive: unexpected success before start of log\n", pmGetProgname());
	__pmDumpHighResResult(stdout, resp);
	pmFreeHighResResult(resp);
    }

    when.tv_sec = PM_MAX_TIME_T;
    when.tv_nsec = 0;
    if ((sts = pmSetModeHighRes(PM_MODE_FORW, &when, NULL)) < 0) {
	printf("%s: pmSetModeHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmFetchHighResArchive(&resp)) < 0) {
	if (sts != PM_ERR_EOL)
	    printf("%s: pmFetchHighResArchive: %s\n", pmGetProgname(), pmErrStr(sts));
    }
    else {
	printf("%s: pmFetchHighResArchive: unexpected success after end of log\n", pmGetProgname());
	__pmDumpHighResResult(stdout, resp);
	pmFreeHighResResult(resp);
    }

    exit(0);
}
