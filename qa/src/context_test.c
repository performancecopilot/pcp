/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * context and profile exerciser
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

static int inst_bin[] = { 100, 200, 300, 400, 500, 600, 700, 800, 900 };
static int inst_colour[] = { 0, 1, 2 };

static int xpect_bin[] = { 9, 0, 1, 5, 0 };
static int xpect_colour[] = { 3, 3, 0, 1, 2 };

#define MAXC (sizeof(xpect_bin)/sizeof(xpect_bin[0]))

static char *namelist[] = {
    "sample.bin",
    "sample.colour"
};

/*
 * handle	profile				values expected
 *   0		everything			9 bins	3 colours
 *   1		no bins				0 bins	3 colours
 *   2		bin=500, no colour		1 bin	0 colour
 *   3		bin=100 .. 500, colour=0	5 bin	1 colour
 *   4          no bins, not 1			0 bins	2 colours
 */

void
_err(int handle)
{
    int		sts;

    sts = pmUseContext(handle);
    if (sts != PM_ERR_NOCONTEXT)
	printf("pmUseContext(%d): Unexpected Error: %s\n", handle, pmErrStr(sts));

    sts = pmDestroyContext(handle);
    if (sts != PM_ERR_NOCONTEXT)
	printf("pmDestroyContext(%d): Unexpected Error: %s\n", handle, pmErrStr(sts));

    sts = pmReconnectContext(handle);
    if (sts != PM_ERR_NOCONTEXT)
	printf("pmReconnectContext(%d): Unexpected Error: %s\n", handle, pmErrStr(sts));
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		i;
    int		iter = 2;
    int		fail;
    int		failiter = 0;
    int		errflag = 0;
    int		type = 0;
    char	*host = "localhost";
    char	*namespace = PM_NS_DEFAULT;
    char	*endnum;
    pmInDom	indom_bin, indom_colour;
    pmID	metrics[2];
    pmResult	*resp;
    pmDesc	desc;
    int		handle[50];		/* need 3 x MAXC */
#ifdef PCP_DEBUG
    static char	*debug = "[-D N] ";
#else
    static char	*debug = "";
#endif
    static char	*usage = "[-a archive] [-h hostname] [-i iterations] [-n namespace]";

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:i:h:n:")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

#ifdef PCP_DEBUG

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
#endif

	case 'h':	/* hostname for PMCD to contact */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'i':	/* iteration count */
	    iter = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -i requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr, "Usage: %s %s%s\n", pmProgname, debug, usage);
	exit(1);
    }

    if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmLookupName(2, namelist, metrics)) < 0) {
	fprintf(stderr, "pmLookupName: %s\n", pmErrStr(sts));
	fprintf(stderr, "pmids: 0x%x 0x%x\n", metrics[0], metrics[1]);
	exit(1);
    }

    if (type == 0)
	type = PM_CONTEXT_HOST;		/* default */

    for (i = 0; i < 3*MAXC; i++) {
	if (i & 1) {
	    /* odd ones are dup of the previous context */
	    if ((sts = pmDupContext()) < 0) {
		fprintf(stderr, "handle[%d]: pmDupContext(): %s\n", i, pmErrStr(sts));
		exit(1);
	    }
	}
	else {
	    if (type == PM_CONTEXT_HOST) {
		if ((sts = pmNewContext(PM_CONTEXT_HOST, host)) < 0) {
		    fprintf(stderr, "handle[%d]: pmNewContext(host=%s): %s\n", i, host, pmErrStr(sts));
		    exit(1);
		}
	    }
	    else {
		if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, host)) < 0) {
		    fprintf(stderr, "handle[%d]: pmNewContext(archive=%s): %s\n", i, host, pmErrStr(sts));
		    exit(1);
		}
	    }
	}
	handle[i] = sts;

	if ((sts = pmLookupDesc(metrics[0], &desc)) < 0) {
	    fprintf(stderr, "pmLookupDesc: context=%d bin: %s\n", handle[i], pmErrStr(sts));
	    exit(1);
	}
	indom_bin = desc.indom;
	if ((sts = pmLookupDesc(metrics[1], &desc)) < 0) {
	    fprintf(stderr, "pmLookupDesc: context=%d colour: %s\n", handle[i], pmErrStr(sts));
	    exit(1);
	}
	indom_colour = desc.indom;

	switch (i % MAXC) {
	    case 0:
		    break;
	    case 1:
		    pmDelProfile(indom_bin, 0, (int *)0);
		    break;
	    case 2:
		    pmDelProfile(indom_bin, 0, (int *)0);
		    pmAddProfile(indom_bin, 1, &inst_bin[4]);
		    pmDelProfile(indom_colour, 0, (int *)0);
		    break;
	    case 3:
		    pmDelProfile(indom_bin, 0, (int *)0);
		    pmAddProfile(indom_bin, 5, &inst_bin[0]);
		    pmDelProfile(indom_colour, 0, (int *)0);
		    pmAddProfile(indom_colour, 1, &inst_colour[0]);
		    break;
	    case 4:
		    pmDelProfile(indom_bin, 0, (int *)0);
		    pmAddProfile(indom_colour, 0, (int *)0);
		    pmDelProfile(indom_colour, 1, &inst_colour[1]);
		    break;
	}
    }

    for (i=0; i < iter; i++) {
	fail = 0;
	for (c = 0; c < 3*MAXC; c++) {
	    errflag = 0;
	    pmUseContext(handle[c]);
	    sts = pmFetch(2, metrics, &resp);
	    if (sts < 0) {
		fprintf(stderr, "botch @ iter=%d, context=%d: pmFetch: %s\n",
			i, handle[c], pmErrStr(sts));
		errflag = 2;
	    }
	    else {
		if (resp->numpmid != 2) {
		    fprintf(stderr, "botch @ iter=%d, context=%d: numpmid %d != 2\n",
			    i, handle[c], resp->numpmid);
		    errflag = 1;
		}
		else {
		    if (resp->vset[0]->numval != xpect_bin[c % MAXC]) {
			fprintf(stderr, "botch @ iter=%d, context=%d: [indom %s] numval got: %d expect: %d\n",
			    i, handle[c], pmInDomStr(indom_bin),
			    resp->vset[0]->numval, xpect_bin[c % MAXC]);
			errflag = 1;
		    }
		    if (resp->vset[1]->numval != xpect_colour[c % MAXC]) {
			fprintf(stderr, "botch @ iter=%d, context=%d: [indom %s] numval got: %d expect: %d\n",
			    i, handle[c], pmInDomStr(indom_colour),
			    resp->vset[1]->numval, xpect_colour[c % MAXC]);
			errflag = 1;
		    }
		}
	    }
	    if (errflag) {
		__pmDumpContext(stderr, handle[c], PM_INDOM_NULL);
		if (errflag != 2)
		    __pmDumpResult(stderr, resp);
		fail++;
	    }
	    if (errflag != 2) {
		if (type == PM_CONTEXT_ARCHIVE) {
		    resp->timestamp.tv_usec--;
		    pmSetMode(PM_MODE_FORW, &resp->timestamp, 0);
		}
		pmFreeResult(resp);
	    }
	}
	if (fail)
	    failiter++;
	else {
	    putchar('.');
	    fflush(stdout);
	}
    }
    for (c = 0; c < 3*MAXC; c++) {
	if ((sts = pmDestroyContext(handle[c])) < 0)
	    fprintf(stderr, "pmDestroyContext %d: %s\n", handle[c], pmErrStr(sts));
    }

    printf("\nPassed %d of %d iterations\n", iter-failiter, iter);

    /*
     * exercise error conditions at PMAPI relating to "handle" use
     */
    printf("Check error handling at PMAPI ...\n");
    _err(-1);		/* too small */
    _err(3*MAXC);	/* too big */
    _err(1);		/* was valid, now destroyed */

    exit(0);
}
