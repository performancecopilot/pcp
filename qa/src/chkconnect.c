/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <unistd.h>
#include <ctype.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char **argv)
{
    int		j;
    int		numpmid;
    int		numval;
    char	*namelist[20];
    pmID	pmidlist[20];
    pmResult	*result;
    int		c;
    int		sts;
    int		i;
    int		errflag = 0;
    char	*host = "localhost";
    char	*namespace = PM_NS_DEFAULT;
    char	*endnum;
    int		iter = 100;

#ifdef PCP_DEBUG
    static char	*debug = "[-D N]";
#else
    static char	*debug = "";
#endif
    static char	*usage = "[-h hostname] [-i iterations] [-n namespace]";

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:i:n:")) != EOF) {
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
	fprintf(stderr, "pmLoadASCIINameSpace(%s, 1): %s\n", namespace, pmErrStr(sts));
	exit(1);
    }

    i = 0;
    namelist[i++] = "sample.control";
    namelist[i++] = "sampledso.control";
    namelist[i++] = "sampledso.control";
    namelist[i++] = "pmcd.control.debug";
    namelist[i++] = "sample.control";
    namelist[i++] = "pmcd.control.debug";
    namelist[i++] = "sampledso.control";
    namelist[i++] = "pmcd.control.debug";
    namelist[i++] = "sample.control";
    namelist[i++] = "sampledso.control";
    namelist[i++] = "pmcd.control.debug";
    namelist[i++] = "sample.control";
    numpmid = i;
    sts = pmLookupName(numpmid, namelist, pmidlist);
    if (sts < 0) {
	fprintf(stderr, "pmLookupName: %s\n", pmErrStr(sts));
	for (i = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL)
		printf("	%s - not known\n", namelist[i]);
	}
	exit(1);
    }

    for (i = 0; i < iter; i++) {
	sts = pmNewContext(PM_CONTEXT_HOST, host);
	if (sts < 0) {
	    fprintf(stderr, "pmNewContext: [iteration %d] %s\n", i, pmErrStr(sts));
	    exit(1);
	}
	else if (sts != i * 2)
	    fprintf(stderr, "Error: [iteration %d] pmNewContext handle expected %d, got %d\n", i, i * 2, sts);

	numpmid = 1 + lrand48() % 12;
	if ((sts = pmFetch(numpmid, pmidlist, &result)) < 0) {
	    fprintf(stderr, "pmFetch: [iteration %d] %s\n", i, pmErrStr(sts));
	    exit(1);
	}
	numval = 0;
	for (j = 0; j < result->numpmid; j++) {
	    if (result->vset[j]->numval >= 0)
		numval += result->vset[j]->numval;
	}
	if (numval != numpmid) {
	    fprintf(stderr, "Error: [iteration %d] pmFetch numval expected %d, got %d\n", i, numpmid, numval);
	}
	pmFreeResult(result);

	sts = pmDupContext();
	if (sts < 0) {
	    fprintf(stderr, "pmDupContext: [iteration %d] %s\n", i, pmErrStr(sts));
	    exit(1);
	}
	else if (sts != i * 2 + 1)
	    fprintf(stderr, "Error: [iteration %d] pmDupContext handle expected %d, got %d\n", i, i * 2 + 1, sts);

	numpmid = 1 + lrand48() % 12;
	if ((sts = pmFetch(numpmid, pmidlist, &result)) < 0) {
	    fprintf(stderr, "pmFetch: [iteration %d, dup context] %s\n", i, pmErrStr(sts));
	    exit(1);
	}
	numval = 0;
	for (j = 0; j < result->numpmid; j++) {
	    if (result->vset[j]->numval >= 0)
		numval += result->vset[j]->numval;
	}
	if (numval != numpmid) {
	    fprintf(stderr, "Error: [iteration %d] pmFetch numval expected %d, got %d\n", i, numpmid, numval);
	}
	pmFreeResult(result);

	if ((sts = pmReconnectContext(i)) < 0) {
	    fprintf(stderr, "pmReconnectContext: [iteration %d] %s\n", i, pmErrStr(sts));
	    exit(1);
	}

	numpmid = 1 + lrand48() % 12;
	if ((sts = pmFetch(numpmid, pmidlist, &result)) < 0) {
	    fprintf(stderr, "pmFetch: [iteration %d, recon] %s\n", i, pmErrStr(sts));
	    exit(1);
	}
	numval = 0;
	for (j = 0; j < result->numpmid; j++) {
	    if (result->vset[j]->numval >= 0)
		numval += result->vset[j]->numval;
	}
	if (numval != numpmid) {
	    fprintf(stderr, "Error: [iteration %d, recon] pmFetch numval expected %d, got %d\n", i, numpmid, numval);
	}
	pmFreeResult(result);

    }

    return 0;
}
