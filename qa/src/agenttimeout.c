/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <pcp/pmapi.h>

int
main(int argc, char *argv[])
{
    int		c;
    int		timeout;
    int		domain;
    char	*endPtr;
    time_t	tBegin, tEnd;
    char	*name;
    pmID	timeoutPmid;
    pmResult	*result;
    pmID	pmid;
    int		sts;
    unsigned	pmcdTimeout;
    unsigned	oldTimeout;
    pmValueSet	*vsp;
    int		tElapsed;
    int		tMax;
    int		errflag = 0;

    pmSetProgname(pmGetProgname());

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {

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

    /* non-flag args are argv[optind] ... argv[argc-1] */
    argc -= optind-1;
    argv += optind-1;

    if (errflag || argc != 3) {
	fprintf(stderr, "Usage: agenttimeout domain timeout\n");
	exit(1);
    }

    domain = strtol(argv[1], &endPtr, 0);
    if (*endPtr != '\0' || domain < 0 || domain >= 255) {
	fprintf(stderr, "domain '%s' is not a valid pmID domain identifier\n", argv[1]);
	exit(1);
    }

    timeout = strtol(argv[2], &endPtr, 0);
    if (*endPtr != '\0' || timeout < 0) {
	fprintf(stderr, "timeout '%s' is not valid\n", argv[2]);
	exit(1);
    }

    printf("Agent timeout test, domain = %d, timeout = %d\n", domain, timeout);
    if ((sts = pmLoadNameSpace(PM_NS_DEFAULT)) < 0) {
	fprintf(stderr, "pmLoadNameSpace: %s\n", pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	fprintf(stderr, "pmNewContext(..., \"localhost\"): %s\n",
		pmErrStr(sts));
	exit(1);
    }

    name = "pmcd.control.timeout";
    if ((sts = pmLookupName(1, &name, &timeoutPmid)) < 0) {
	fprintf(stderr, "pmLookupName(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }

    /* fetch pmcd.control.timeout and make sure the result is OK */
    if ((sts = pmFetch(1, &timeoutPmid, &result)) < 0) {
	fprintf(stderr, "pmFetch(pmcd.control.timeout): %s\n", pmErrStr(sts));
	exit(1);
    }
    if (result->numpmid != 1) {
	fprintf(stderr, "pmFetch(pmcd.control.timeout): %d values fetched!\n", result->numpmid);
	exit(1);
    }
    vsp = result->vset[0];
    if (vsp->pmid != timeoutPmid || vsp->numval != 1) {
	fprintf(stderr, "pmFetch(pmcd.control.timeout): bad pmID or numval in result\n");
	exit(1);
    }
    oldTimeout = vsp->vlist[0].value.lval;

    /* Now store the new value then fetch it back to check */
    vsp->vlist[0].value.lval = timeout;
    if ((sts = pmStore(result)) < 0) {
	fprintf(stderr, "pmStore(pmcd.control.timeout): %s\n", pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmFetch(1, &timeoutPmid, &result)) < 0) {
	fprintf(stderr, "#2 pmFetch(pmcd.control.timeout): %s\n", pmErrStr(sts));
	exit(1);
    }
    if (result->numpmid != 1) {
	fprintf(stderr, "#2 pmFetch(pmcd.control.timeout): %d values fetched!\n", result->numpmid);
	exit(1);
    }
    vsp = result->vset[0];
    if (vsp->pmid != timeoutPmid || vsp->numval != 1) {
	fprintf(stderr, "#2 pmFetch(pmcd.control.timeout): bad pmID or numval in result\n");
	exit(1);
    }
    if (vsp->vlist[0].value.lval != timeout) {
	fprintf(stderr, "pmcd timeout update failed, old = %d, new = %d, returned = %d\n",
		oldTimeout, timeout, vsp->vlist[0].value.lval);
	exit(1);
    }
    pmcdTimeout = vsp->vlist[0].value.lval;
    pmFreeResult(result);
    printf("    timeout is now %d seconds\n", pmcdTimeout);

    pmid = pmID_build(domain, 0, 0);
    tBegin = time((time_t *)0);
    if ((sts = pmFetch(1, &pmid, &result)) < 0) {
	fprintf(stderr, "fetch error = %s\n", pmErrStr(sts));
	exit(1);
    }
    vsp = result->vset[0];
    if (vsp->numval != PM_ERR_TIMEOUT)
	fprintf(stderr, "Timeout fetch didn't time out\n");
    pmFreeResult(result);
    tEnd = time((time_t *)0);
#ifdef DESPERATE
    printf("    %d seconds for fetch\n", tEnd - tBegin);
#endif
    tElapsed = tEnd - tBegin;

    /* The way pmcd timeouts work, the time before an agent is timed-out
     * varies between the timeout and 2 * timeout.  It's actually even more
     * complicated to calculate the maximum timeout interval if the timeout is
     * changed, since the timeout sproc continually reschedules itself
     * asynchronously.  There are two extrema:
     *    . If the (old) timeout had just gone off and has been requeued for
     *      the old interval just prior to the change being made.
     *	  . If the old timeout is just about to go off but the change gets
     *	    made first 
     */
    if (pmcdTimeout <= oldTimeout)
	tMax = oldTimeout + pmcdTimeout;
    else
	tMax = 2 * pmcdTimeout;

    if (tElapsed < pmcdTimeout)
	fprintf(stderr, "Fetch timed out prematurely: elapsed = %d\n", tElapsed);
    else if (tElapsed > tMax + 2)	/* Allow 2 second's delay in processing */
	fprintf(stderr, "Fetch took too long to time out: %d\n", tElapsed);
    exit(0);
}
