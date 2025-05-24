/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * context and profile exerciser
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

static int inst_bin[] = { 100, 200, 300, 400, 500, 600, 700, 800, 900 };
static int inst_colour[] = { 0, 1, 2 };

static int xpect_bin[] = { 9, 0, 1, 5, 0 };
static int xpect_colour[] = { 3, 3, 0, 1, 2 };

#define MAXC (sizeof(xpect_bin)/sizeof(xpect_bin[0]))

static const char *namelist[] = {
    "sample.bin",
    "sample.colour"
};

/*
 * handle	profile				values expected
 *   0		everything			9 bins	3 colours
 *   1		no bins				0 bins	3 colours
 *   2		bin=500, no colour		1 bin	0 colour
 *   3		bin=100 .. 500, colour=0	5 bin	1 colour
 *   4          no bins, not colour=1		0 bins	2 colours
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
    int		failsetup = 0;
    int		failiter = 0;
    int		errflag = 0;
    int		type = 0;
    char	*host = "localhost";
    char	*namespace = PM_NS_DEFAULT;
    char	*endnum;
    pmInDom	indom_bin, indom_colour;
    pmID	metrics[2];
    pmDesc	descs[2];
    pmHighResResult	*resp;
    __pmContext	*ctxp;
    int		handle[50];		/* need 3 x MAXC */
    static char	*usage = "[-a archive] [-D debugspec] [-h hostname] [-i iterations] [-n namespace]";

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:i:h:n:")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmGetProgname());
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;


	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'h':	/* hostname for PMCD to contact */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmGetProgname());
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'i':	/* iteration count */
	    iter = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -i requires numeric argument\n", pmGetProgname());
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
	fprintf(stderr, "Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), namespace, pmErrStr(sts));
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
	    pmProfile		*old;
	    pmProfile		*new;
	    /* odd ones are dup of the previous context */
	    if ((sts = pmDupContext()) < 0) {
		fprintf(stderr, "handle[%d]: pmDupContext(): %s\n", i, pmErrStr(sts));
		exit(1);
	    }
	    /*
	     * integrity check ... instance profiles should be identical
	     */
	    ctxp = __pmHandleToPtr(i-1);
	    old = ctxp->c_instprof;
	    PM_UNLOCK(ctxp->c_lock);
	    ctxp = __pmHandleToPtr(i);
	    new = ctxp->c_instprof;
	    PM_UNLOCK(ctxp->c_lock);
	    /*
	     * Threadsafe note: we are single threaded, so don't worry
	     * about accessing __pmContext after c_lock released.
	     */
	    if (old == NULL && new == NULL)
		;	/* OK */
	    else if (old == NULL && new != NULL) {
		fprintf(stderr, "botch profile @ setup, orig context=%d has NO profile, dup context=%d has profile\n", i, i-1);
		fprintf(stderr, "  dup context: ");
		__pmDumpProfile(stderr, PM_INDOM_NULL, new);
		failsetup++;
	    }
	    else if (old != NULL && new == NULL) {
		fprintf(stderr, "botch profile @ setup, orig context=%d has profile, dup context=%d  has NO profile\n", i, i-1);
		fprintf(stderr, "  orig context: ");
		__pmDumpProfile(stderr, PM_INDOM_NULL, old);
		failsetup++;
	    }
	    else {
		int	bad = 0;
		if (old->state != new->state) {
		    fprintf(stderr, "botch profile @ setup, orig context=%d state=%d, dup context=%d state=%d\n", i-1, old->state, i, new->state);
		    bad = 1;
		    failsetup++;
		}
		if (old->profile_len != new->profile_len) {
		    fprintf(stderr, "botch profile @ setup, orig context=%d profile_len=%d, dup context=%d profile_len=%d\n", i-1, old->profile_len, i, new->profile_len);
		    bad = 1;
		    failsetup++;
		}
		else {
		    int		j;

		    for (j = 0; j < old->profile_len; j++) {
			if (old->profile[j].indom != new->profile[j].indom) {
			    fprintf(stderr, "botch profile @ setup, orig context=%d indom[%d=%s]", i-1, j, pmInDomStr(old->profile[j].indom));
			    fprintf(stderr, ", dup context=%d indom[%d=%s]", i, j, pmInDomStr(new->profile[j].indom));
			    bad = 1;
			    failsetup++;
			}
			if (old->profile[j].state != new->profile[j].state) {
			    fprintf(stderr, "botch profile @ setup, orig context=%d indom[%d=%s] state=%d", i-1, j, pmInDomStr(old->profile[j].indom), old->profile[j].state);
			    fprintf(stderr, ", dup context=%d indom[%d=%s] state=%d", i, j, pmInDomStr(new->profile[j].indom), new->profile[j].state);
			    bad = 1;
			    failsetup++;
			}
			if (old->profile[j].instances_len != new->profile[j].instances_len) {
			    fprintf(stderr, "botch profile @ setup, orig context=%d indom[%d=%s] instances_len=%d", i-1, j, pmInDomStr(old->profile[j].indom), old->profile[j].instances_len);
			    fprintf(stderr, ", dup context=%d indom[%d=%s] instances_len=%d", i, j, pmInDomStr(new->profile[j].indom), new->profile[j].instances_len);
			    bad = 1;
			    failsetup++;
			}
			else {
			    int		k;

			    for (k = 0; k < old->profile[j].instances_len; k++) {
				if (old->profile[j].instances[k] != new->profile[j].instances[k]) {
				    fprintf(stderr, "botch profile @ setup, orig context=%d indom[%d=%s] inst[%d]=%d", i-1, j, pmInDomStr(old->profile[j].indom), k, old->profile[j].instances[k]);
				    fprintf(stderr, ", dup context=%d indom[%d=%s] inst[%d]=%d", i, j, pmInDomStr(new->profile[j].indom), k, new->profile[j].instances[k]);
				    bad = 1;
				    failsetup++;
				}
			    }
			}
		    }
		}
		if (bad) {
		    fprintf(stderr, "  orig context: ");
		    __pmDumpProfile(stderr, PM_INDOM_NULL, old);
		    fprintf(stderr, "  dup context: ");
		    __pmDumpProfile(stderr, PM_INDOM_NULL, new);
		}
	    }
	    /*
	     * The logic below assumes the instance profile for
	     * the dup'd contexts is the same as a context from
	     * pmNewContext(), namely all instances of all indoms ...
	     * but we've inherited the instance profile from the
	     * previous context via the pmDupContext, so clear it
	     * out
	     */
	    pmDelProfile(PM_INDOM_NULL, 0, (int *)0);
	    pmAddProfile(PM_INDOM_NULL, 0, (int *)0);
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

	/*
	 * integrity assertions ...
	 */
	ctxp = __pmHandleToPtr(pmWhichContext());
	if (ctxp == NULL) {
	    fprintf(stderr, "__pmHandleToPtr: returns NULL, ctx %d, pmWhichContext() %d\n", sts, pmWhichContext());
	    exit(1);
	}
	if (ctxp->c_handle != sts) {
	    fprintf(stderr, "__pmHandleToPtr: c_handle %d != ctx %d\n", ctxp->c_handle, sts);
	    exit(1);
	}
	PM_UNLOCK(ctxp->c_lock);

	if ((sts = pmLookupDescs(2, metrics, descs)) < 0) {
	    fprintf(stderr, "pmLookupDescs: context=%d: %s\n", handle[i], pmErrStr(sts));
	    exit(1);
	}
	indom_bin = descs[0].indom;
	indom_colour = descs[1].indom;

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
	if (pmDebugOptions.context) {
	    fprintf(stderr, "After profile setup ...\n");
	    __pmDumpContext(stderr, handle[i], PM_INDOM_NULL);
	}
    }

    for (i=0; i < iter; i++) {
	fail = 0;
	for (c = 0; c < 3*MAXC; c++) {
	    errflag = 0;
	    pmUseContext(handle[c]);
	    if (pmDebugOptions.context) {
		fprintf(stderr, "Just before pmFetch ...\n");
		__pmDumpContext(stderr, handle[c], PM_INDOM_NULL);
	    }
	    sts = pmFetchHighRes(2, metrics, &resp);
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
		    __pmDumpHighResResult(stderr, resp);
		fail++;
	    }
	    if (errflag != 2) {
		if (type == PM_CONTEXT_ARCHIVE) {
		    resp->timestamp.tv_nsec--;
		    pmSetMode(PM_MODE_FORW, &resp->timestamp, NULL);
		}
		pmFreeHighResResult(resp);
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

    if (failsetup)
    printf("\nFailed %d tests during setup\n", failsetup);

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
