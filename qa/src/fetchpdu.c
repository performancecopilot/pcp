/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>


int
main(int argc, char **argv)
{

    int		c;
    int		sts;
    int		errflag = 0;
    char	*host = "localhost";
    char	*namespace = PM_NS_DEFAULT;
#ifdef PCP_DEBUG
    static char	*debug = "[-D N] ";
#else
    static char	*debug = "";
#endif
    static char	*usage = "[-h hostname] [-n namespace]";
    int		i;
    int		n;
    char	*namelist[20];
    pmID	midlist[20];
    int		numpmid;
    pmResult	*rslt;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:n:")) != EOF) {
	switch (c) {
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
	    host = optarg;
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
	printf("Usage: %s %s%s\n", pmProgname, debug, usage);
	exit(1);
    }

    if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, host)) < 0) {
	printf("%s: Cannot connect to PMCD on host \"%s\": %s\n", pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    i = 0;
    namelist[i++] = "sample.long.one";
    namelist[i++] = "sampledso.long.one";
    numpmid = i;
    n = pmLookupName(numpmid, namelist, midlist);
    if (n < 0) {
	printf("pmLookupName: %s\n", pmErrStr(n));
	for (i = 0; i < numpmid; i++) {
	    if (midlist[i] == PM_ID_NULL)
		printf("   %s is bad\n", namelist[i]);
	}
	exit(1);
    }

    if ((n = pmFetch(numpmid, midlist, &rslt)) < 0) {
	printf("pmFetch rslt: %s\n", pmErrStr(n));
	exit(1);
    }

    for (i = 0; i < numpmid; i++) {
	printf("%s: ", namelist[i]);
        if (rslt->vset[i]->numval < 0)
	    printf("%s\n", pmErrStr(rslt->vset[i]->numval));
	else
	    printf("%d\n", rslt->vset[i]->vlist[0].value.lval);
    }

/*
    __pmDumpResult(stdout, rslt);
*/

    exit(0);
}
