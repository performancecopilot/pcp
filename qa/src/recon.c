/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * ping pmcd ... when the connection is lost, note time.
 * keep pinging ... when connection is resumed, report time delta.
 *
 * Has to be run as root to control pmcd
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

#define MAX_LOOP 1000

int
main(int argc, char **argv)
{
    struct timeval	now;
    struct timeval	then;
    pmResult	*rp;
    int		i;
    char	*namelist[4];
    pmID	pmidlist[4];
    int		numpmid;
    int		ctx;
    int		c;
    int		sts;
    int		errflag = 0;
    int		loop;

    static const struct timeval delay = { 0, 10000 };
    /*
     * was sginap(10) ... I think this dated from the sgi days when this
     * was, 10 * 1/100 (1/HZ) seconds, so 100,000 usec or 100 msec ...
     * but this is not reliable for the QA test (200), so changed to
     * 10 msec
     */

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:")) != EOF) {
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

    if (errflag) {
	fprintf(stderr,
"Usage: %s options ...\n\
\n\
Options:\n\
  -D debugspec		set PCP debugging options\n",
		pmGetProgname());
	exit(1);
    }

    if ((sts = pmLoadNameSpace(PM_NS_DEFAULT)) < 0) {
	fprintf(stderr, "pmLoadNameSpace: %s\n", pmErrStr(sts));
	exit(1);
    }

    if ((ctx = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	fprintf(stderr, "pmNewContext: %s\n", pmErrStr(ctx));
	exit(1);
    }

    i = 0;
    namelist[i++] = "sample.long.write_me";
    numpmid = i;
    sts = pmLookupName(numpmid, namelist, pmidlist);
    if (sts < 0) {
	fprintf(stderr, "pmLookupName: %s\n", pmErrStr(sts));
	for (i = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL)
		fprintf(stderr, "	%s - not known\n", namelist[i]);
	}
	exit(1);
    }

    for (loop = 0; loop < MAX_LOOP; loop++) {
	if ((sts = pmFetch(1, pmidlist, &rp)) < 0) {
	    fprintf(stderr, "pmFetch failed: %s\n", pmErrStr(sts));
	    if (sts != PM_ERR_IPC && sts != -ECONNRESET) {
		/* unexpected */
		fprintf(stderr, "Bogus error?\n");
		exit(1);
	    }
	    gettimeofday(&then, (struct timezone *)0);
	    break;
	}
	pmFreeResult(rp);
	__pmtimevalSleep(delay);
    }
    if (loop == MAX_LOOP) {
	fprintf(stderr, "Arrgh: pmFetch() failed %d times ... giving up!\n", loop);
	exit(1);
    }

    for (loop = 0; loop < MAX_LOOP; loop++) {
	if ((sts = pmReconnectContext(ctx)) >= 0) {
	    fprintf(stderr, "pmReconnectContext: success\n");
	    gettimeofday(&now, (struct timezone *)0);
	    /* roundup to the nearest second, now that pmcd stop is much quicker */
	    fprintf(stderr, "delay: %.0f secs\n", pmtimevalSub(&now, &then) + 0.5);
	    break;
	}
	__pmtimevalSleep(delay);
    }
    if (loop == MAX_LOOP) {
	fprintf(stderr, "Arrgh: pmReconnectContext() failed %d times ... giving up!\n", loop);
	exit(1);
    }


    exit(0);
}
