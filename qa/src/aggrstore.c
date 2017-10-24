/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
 *
 * Store a string value into an aggregate metric
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char **argv)
{
    int		type = PM_CONTEXT_HOST;
    int		c;
    int		sts;
    int		errflag = 0;
    char	*host = "localhost";
    static char	*usage = "[-D debugspec] [-h hostname] metric stringvalue";
    int			len;
    int			n;
    char		*namelist[1];
    pmID		pmidlist[1];
    pmResult		*res;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    break;

	case 'h':	/* hostname for PMCD to contact */
	    host = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc-2) {
	printf("Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    if ((sts = pmNewContext(type, host)) < 0) {
	printf("%s: Cannot connect to PMCD on host \"%s\": %s\n",
	    pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    namelist[0] = argv[optind];
    n = pmLookupName(1, namelist, pmidlist);
    if (n < 0 || pmidlist[0] == PM_ID_NULL) {
	printf("pmLookupName: %s\n", pmErrStr(n));
	exit(1);
    }

    if ((n = pmFetch(1, pmidlist, &res)) < 0) {
	printf("pmFetch: %s\n", pmErrStr(n));
	exit(1);
    }

    /*
     * expecting one value and a pmValueBlock with a type
     * of PM_TYPE_AGGREGATE
     */
    if (res->vset[0]->numval != 1) {
	printf("Expecting numval 1, found %d\n", res->vset[0]->numval);
	__pmDumpResult(stdout, res);
	exit(1);
    }
    if (res->vset[0]->valfmt == PM_VAL_INSITU) {
	printf("Not expecing PM_VAL_INSITU\n");
	__pmDumpResult(stdout, res);
	exit(1);
    }
    if (res->vset[0]->vlist[0].value.pval->vtype != PM_TYPE_AGGREGATE) {
	printf("Not expecing type %s\n", pmTypeStr(res->vset[0]->vlist[0].value.pval->vtype));
	__pmDumpResult(stdout, res);
	exit(1);
    }
    printf("%s old value: ", namelist[0]);
    pmPrintValue(stdout, res->vset[0]->valfmt, res->vset[0]->vlist[0].value.pval->vtype, &res->vset[0]->vlist[0], 0);

    /*
     * old value is probably from a pinned PDU buffer ... don't free
     * and accept small mem leak here
     */
    len = strlen(argv[optind+1]);
    res->vset[0]->vlist[0].value.pval = (pmValueBlock *)malloc(len + PM_VAL_HDR_SIZE);

    res->vset[0]->vlist[0].value.pval->vtype = PM_TYPE_AGGREGATE;
    res->vset[0]->vlist[0].value.pval->vlen = len + PM_VAL_HDR_SIZE;
    memcpy(res->vset[0]->vlist[0].value.pval->vbuf, argv[optind+1], len);

    if ((n = pmStore(res)) < 0) {
	printf("pmStore: %s\n", pmErrStr(n));
	exit(1);
    }
    pmFreeResult(res);

    if ((n = pmFetch(1, pmidlist, &res)) < 0) {
	printf("pmFetch again: %s\n", pmErrStr(n));
	exit(1);
    }
    printf(" new value: ");
    pmPrintValue(stdout, res->vset[0]->valfmt, res->vset[0]->vlist[0].value.pval->vtype, &res->vset[0]->vlist[0], 0);
    putchar('\n');

    pmFreeResult(res);

    exit(0);
}
