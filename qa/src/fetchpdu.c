/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include "libpcp.h"


int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		highres = 0;
    int		timestamp = 0;
    char	*hostspec = "local:";
    char	*namespace = PM_NS_DEFAULT;
    static char	*usage = "[-D debugspec] [-H] [-h hostspec] [-n namespace] [-T]";
    int		i;
    int		n;
    const char	*namelist[20];
    pmID	midlist[20];
    int		numpmid;
    pmResult_v2	*rslt;
    pmResult	*hrslt;
    pmValueSet	**vset;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:Hn:T")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'h':	/* hostspec for PMCD to contact */
	    hostspec = optarg;
	    break;

	case 'H':	/* use high resolution fetching */
	    highres = 1;
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case 'T':	/* report fetch timestamp also */
	    timestamp = 1;
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

    if ((namespace != PM_NS_DEFAULT) &&
	(sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	fprintf(stderr, "%s: Cannot load namespace from \"%s\": %s\n",
			pmGetProgname(), namespace, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, hostspec)) < 0) {
	fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
			pmGetProgname(), hostspec, pmErrStr(sts));
	exit(1);
    }

    i = 0;
    namelist[i++] = "sample.long.one";
    namelist[i++] = "sampledso.long.one";
    numpmid = i;
    n = pmLookupName(numpmid, namelist, midlist);
    if (n < 0)
	fprintf(stderr, "pmLookupName: %s\n", pmErrStr(n));
    if (n != numpmid) {
	for (i = 0; i < numpmid; i++) {
	    if (midlist[i] == PM_ID_NULL)
		fprintf(stderr, "   %s is bad\n", namelist[i]);
	}
	exit(1);
    }

    if (highres) {
	if ((n = pmFetch(numpmid, midlist, &hrslt)) < 0) {
	    fprintf(stderr, "pmFetch: %s\n", pmErrStr(n));
	    exit(1);
	}
	if (timestamp)
	    pmPrintHighResStamp(stdout, &hrslt->timestamp);
	vset = hrslt->vset;
    } else {
	if ((n = pmFetch_v2(numpmid, midlist, &rslt)) < 0) {
	    fprintf(stderr, "pmFetch_v2: %s\n", pmErrStr(n));
	    exit(1);
	}
	if (timestamp)
	    pmPrintStamp(stdout, &rslt->timestamp);
	vset = rslt->vset;
    }

    if (timestamp)
	putchar('\n');

    for (i = 0; i < numpmid; i++) {
	printf("%s: ", namelist[i]);
        if (vset[i]->numval < 0)
	    printf("%s\n", pmErrStr(vset[i]->numval));
	else
	    printf("%d\n", vset[i]->vlist[0].value.lval);
    }

    if (pmDebugOptions.appl0) {
	if (highres)
	    __pmDumpResult(stdout, hrslt);
	else
	    __pmDumpResult_v2(stdout, rslt);
    }

    exit(0);
}
