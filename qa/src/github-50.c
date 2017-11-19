/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2015 Ken McDonell.  All Rights Reserved.
 *
 * Locking bug in libpcp ... see
 * https://github.com/performancecopilot/pcp/pull/50
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

#define BUILD_STANDALONE

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		type = 0;
    int		try = 0;
    char	*host = NULL;			/* pander to gcc */
    pmLogLabel	label;
    __pmContext	*ctxp;

    /* trim cmd name of leading directory components */
    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:h:Lx?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
#ifdef BUILD_STANDALONE
		fprintf(stderr, "%s: at most one of -a, -h, -L and -x allowed\n", pmGetProgname());
#else
		fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmGetProgname());
#endif
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

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
#ifdef BUILD_STANDALONE
		fprintf(stderr, "%s: at most one of -a, -h, -L and -x allowed\n", pmGetProgname());
#else
		fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmGetProgname());
#endif
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

#ifdef BUILD_STANDALONE
	case 'L':	/* LOCAL, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h, -L and -x allowed\n", pmGetProgname());
		errflag++;
	    }
	    host = NULL;
	    type = PM_CONTEXT_LOCAL;
	    putenv("PMDA_LOCAL_PROC=");		/* if proc PMDA needed */
	    putenv("PMDA_LOCAL_SAMPLE=");	/* if sampledso PMDA needed */
	    break;
#endif

	case 'x':	/* no current context */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h, -L and -x allowed\n", pmGetProgname());
		errflag++;
	    }
	    type = -1;
	    break;
	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options]\n\
\n\
Options:\n\
  -a archive     metrics source is a PCP log archive\n\
  -h host        metrics source is PMCD on host\n"
#ifdef BUILD_STANDALONE
"  -L             use local context instead of PMCD\n"
#endif
"  -x             no current context\n"
	    , pmGetProgname());
        exit(1);
    }

    if (type != -1) {
	if (type == 0) {
	    /* default context */
	    type = PM_CONTEXT_HOST;
	    host = "local:";
	}
	if ((sts = pmNewContext(type, host)) < 0) {
	    if (type == PM_CONTEXT_HOST)
		fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmGetProgname(), host, pmErrStr(sts));
    #ifdef BUILD_STANDALONE
	    else if (type == PM_CONTEXT_LOCAL)
		fprintf(stderr, "%s: Cannot initialize LOCAL context: %s\n",
		    pmGetProgname(), pmErrStr(sts));
    #endif
	    else
		fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), host, pmErrStr(sts));
	    exit(1);
	}
    }

    if ((sts = pmGetArchiveLabel(&label)) < 0)
	fprintf(stderr, "%s: Cannot get archive label record: %s\n",
	    pmGetProgname(), pmErrStr(sts));
    else
	fprintf(stderr, "%s: archive label record: magic=%x host=%s\n",
	    pmGetProgname(), label.ll_magic, label.ll_hostname);

    ctxp = __pmHandleToPtr(pmWhichContext());
    if (ctxp == NULL) {
	fprintf(stderr, "%s: __pmHandleToPtr botch: for context=%d\n",
	    pmGetProgname(), pmWhichContext());
	exit(1);
    }

    /*
     * Should Unlock OK just once ... from the __pmHandleToPtr() call
     * above.
     * In the bug case, when the context is valid and _not_ an archive,
     * the Unlock OK happens twice
     */
    while (try < 3 && (sts = PM_UNLOCK(ctxp->c_lock)) == 0) {
	fprintf(stderr, "Unlock OK\n");
	try++;
    }
    fprintf(stderr, "Unlock Fail: %s\n", pmErrStr(sts));

    return 0;
}
