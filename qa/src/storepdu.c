/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <ctype.h>
#include <pcp/pmapi.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	*host = "localhost";
    char	*namespace = PM_NS_DEFAULT;
    static char	*usage = "[-D debugspec] [-h hostname] [-n namespace]";
    int		i;
    int		n;
    char	*namelist[20];
    pmID	midlist[20];
    int		numpmid;
    pmResult	*rslt;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:n:")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

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
	printf("Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), namespace, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, host)) < 0) {
	printf("%s: Cannot connect to PMCD on host \"%s\": %s\n", pmGetProgname(), host, pmErrStr(sts));
	exit(1);
    }

    i = 0;
    namelist[i++] = "sampledso.long.one";
    namelist[i++] = "sample.long.write_me";
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

    if ((n = pmFetch(1, &midlist[0], &rslt)) < 0) {
	printf("pmFetch rslt: %s\n", pmErrStr(n));
	exit(1);
    }

    rslt->vset[0]->pmid = midlist[1];
    if ((n = pmStore(rslt)) < 0) {
	printf("pmStore: %s\n", pmErrStr(n));
	exit(1);
    }
    printf("pmStore: ok\n");
    exit(0);
}
