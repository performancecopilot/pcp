/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * chknumval - check out new encoding of errors within numval of a pmResult
 *
 * also drives pmStore, __pmConnectLogger and __pmControlLog testing
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
    char	*errmsg;
    int		type = 0;
    char	*host = "localhost";
    char	*namespace = PM_NS_DEFAULT;
    int		i;
    int		ctlport;
    int		pid = PM_LOG_PRIMARY_PID;
    int		port = PM_LOG_NO_PORT;
    const char	*namelist[20];
    pmID	pmidlist[20];
    pmDesc	desclist[20];
    int		*instlist;
    char	**inamelist;
    int		numpmid = 0;
    int		default_metrics = 0;
    __pmResult	*req;
    __pmResult	*status;
    pmResult	*result;
    pmDesc	desc;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:K:Ln:")) != EOF) {
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
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of --h and -L allowed\n", pmGetProgname());
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'K':	/* update local PMDA table */
	    if ((errmsg = pmSpecLocalPMDA(optarg)) != NULL) {
		fprintf(stderr, "%s: pmSpecLocalPMDA failed: %s\n", pmGetProgname(), errmsg);
		errflag++;
	    }
	    break;

	case 'L':	/* local PMDA connection, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -h and -L allowed\n", pmGetProgname());
		errflag++;
	    }
	    type = PM_CONTEXT_LOCAL;
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
	fprintf(stderr,
"Usage: %s [options] [metricname ...]\n\
\n\
Options\n\
  -D debug	standard PCP debug options\n\
  -h host	metrics source is PMCD on host (default is localhost)\n\
  -L            metrics source is local connection to PMDA, no PMCD\n\
  -K spec       optional additional PMDA spec for local connection\n\
                spec is of the form op,domain,dso-path,init-routine\n\
  -n namespace  use an alternative PMNS\n\
  -v            be verbose\n",
		pmGetProgname());
	exit(1);
    }

    while (optind < argc) {
	namelist[numpmid++] = argv[optind];
	optind++;
    }
    if (numpmid > 0 && numpmid < 3) {
	printf("Quitting need at least 3 metrics (not %d) for numval tests\n", numpmid);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: pmLoadASCIINameSpace: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    if (type == 0) type = PM_CONTEXT_HOST;
    if ((sts = pmNewContext(type, host)) < 0) {
	printf("%s: pmNewContext(%d, %s): %s\n", pmGetProgname(), type, host, pmErrStr(sts));
	exit(1);
    }

    /*
     * be prepared to try 10 times here ... pmlogger's  pmlc port maybe busy
     */
    for (i = 0; i < 10; i++) {
	struct timespec delay = { 0, 100000000 };	/* 0.1 sec */
	sts = __pmConnectLogger(host, &pid, &port);
	if (sts >= 0)
	    break;
	(void)nanosleep(&delay, NULL);
    }
    if (sts < 0) {
	printf("%s: Cannot connect to primary pmlogger on host \"%s\": %s\n", pmGetProgname(), host, pmErrStr(sts));
	exit(1);
    }
    ctlport = sts;

    if (numpmid == 0) {
	/* default metrics */
	default_metrics = 1;
	i = 0;
	namelist[i++] = "pmcd.control.sighup";
	namelist[i++] = "sampledso.long.write_me";
	namelist[i++] = "sample.colour";
	numpmid = i;
    }

    sts = pmLookupName(numpmid, namelist, pmidlist);
    if (sts != numpmid) {
	printf("pmLookupName: failed: %s\n", sts < 0 ? pmErrStr(sts) : "");
	for (i = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL)
		printf("	%s - not known\n", namelist[i]);
	}
	exit(1);
    }

    sts = pmLookupDescs(numpmid, pmidlist, desclist);
    if (sts < 0) {
	printf("pmLookupDescs: failed: %s\n", pmErrStr(sts));
	exit(1);
    }
    /*
     * need all metrics to have the same sort of type for the pmStore tests to work
     */
    for (i = 1; i < numpmid; i++) {
	if (desclist[i].pmid == PM_ID_NULL) {
	    printf("Warning: no pmDesc for metric[%d] %s\n", i, namelist[i]);
	    continue;
	}
	if (desclist[i].type != desclist[0].type) {
	    switch (desclist[0].type) {
		case PM_TYPE_32:
		case PM_TYPE_U32:
		case PM_TYPE_64:
		case PM_TYPE_U64:
			    if (desclist[i].type == PM_TYPE_32 ||
				desclist[i].type == PM_TYPE_U32 ||
				desclist[i].type == PM_TYPE_64 ||
				desclist[i].type == PM_TYPE_U64) continue;
			    break;
		case PM_TYPE_FLOAT:
		case PM_TYPE_DOUBLE:
			    if (desclist[i].type == PM_TYPE_FLOAT ||
				desclist[i].type == PM_TYPE_DOUBLE) continue;
			    break;
	    }
	    printf("Botch: metric[%d] %s: type %s\n", i, namelist[i], pmTypeStr(desclist[i].type));
	    printf("       metric[0] %s: type %s\n", namelist[0], pmTypeStr(desclist[0].type));
	    exit(1);
	}
    }

    if ((sts = __pmFetch(NULL, numpmid, pmidlist, &req)) < 0) {
	printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    if ((sts = __pmControlLog(ctlport, req, PM_LOG_MANDATORY, PM_LOG_OFF, 0, &status)) < 0) {
	printf("__pmControlLog: %s\n", pmErrStr(sts));
	exit(1);
    }

    __pmPrintResult(stdout, req);

    printf("\nbase store test (failures not unexpected) ...\n");
    if ((sts = pmStore(__pmOffsetResult(req))) < 0)
	printf("pmStore: %s\n", pmErrStr(sts));
    else
	printf("pmStore: OK\n");

    printf("\nnumpmid == 0 tests (failures expected) ...\n");
    req->numpmid = 0;
    __pmPrintResult(stdout, req);

    if ((sts = pmStore(__pmOffsetResult(req))) < 0)
	printf("pmStore: %s\n", pmErrStr(sts));
    else
	printf("pmStore: botch, PM_ERR_TOOSMALL expected\n");
    if ((sts = __pmControlLog(ctlport, req, PM_LOG_ENQUIRE, 0, 0, &status)) < 0)
	printf("__pmControlLog: %s\n", pmErrStr(sts));
    else {
	printf("__pmControlLog: OK\n");
	__pmPrintResult(stdout, status);
	__pmFreeResult(status);
    }

    printf("\nnumval == 0 tests (failures expected) ...\n");
    req->numpmid = numpmid;
    req->vset[1]->numval = 0;
    __pmPrintResult(stdout, req);

    if ((sts = pmStore(__pmOffsetResult(req))) < 0)
	printf("pmStore: %s\n", pmErrStr(sts));
    else
	printf("pmStore: botch, PM_ERR_VALUE expected\n");
    if ((sts = __pmControlLog(ctlport, req, PM_LOG_ENQUIRE, 0, 0, &status)) < 0)
	printf("__pmControlLog: %s\n", pmErrStr(sts));
    else {
	printf("__pmControlLog: OK\n");
	__pmPrintResult(stdout, status);
	__pmFreeResult(status);
    }

    printf("\nnumval < 0 tests (failures expected) ...\n");
    req->vset[1]->numval = 1;
    req->vset[2]->numval = PM_ERR_NOAGENT;
    __pmPrintResult(stdout, req);

    if ((sts = pmStore(__pmOffsetResult(req))) < 0)
	printf("pmStore: %s\n", pmErrStr(sts));
    else
	printf("pmStore: botch, PM_ERR_VALUE expected\n");
    if ((sts = __pmControlLog(ctlport, req, PM_LOG_ENQUIRE, 0, 0, &status)) < 0)
	printf("__pmControlLog: %s\n", pmErrStr(sts));
    else {
	printf("__pmControlLog: OK\n");
	__pmPrintResult(stdout, status);
	__pmFreeResult(status);
    }

    /* exercise *.needprofile */
    __pmFreeResult(req);

    if (default_metrics) {
	i = 0;
	namelist[i++] = "sampledso.long.hundred";
	namelist[i++] = "sampledso.needprofile";
	namelist[i++] = "sample.long.hundred";
	namelist[i++] = "sample.needprofile";
	numpmid = i;
    }
    else {
	if (numpmid < 4) {
	    printf("Quitting need at least 4 metrics for profile tests\n");
	    exit(1);
	}
	printf("Warning: running profile tests with non-default metrics ...\n");
	printf("... metric %s needs an indom with at least %d instances\n", namelist[1], 5);
	printf("... metric %s needs an indom with at least %d instances\n", namelist[3], 4);
    }

    sts = pmLookupName(numpmid, namelist, pmidlist);
    if (sts != numpmid) {
	printf("pmLookupName: failed: %s\n", sts < 0 ? pmErrStr(sts) : "");
	for (i = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL)
		printf("	%s - not known\n", namelist[i]);
	}
	exit(1);
    }

    if ((sts = pmFetch(numpmid, pmidlist, &result)) < 0) {
	printf("pmFetch: botch, %s\n", pmErrStr(sts));
	exit(1);
    }
    __pmDumpResult(stdout, result);

    if ((sts = pmLookupDesc(pmidlist[1], &desc)) < 0) {
	fprintf(stderr, "pmLookupDesc: %s\n", pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmGetInDom(desc.indom, &instlist, &inamelist)) < 0) {
	printf("pmGetInDom: %s\n", pmErrStr(sts));
    }
    pmAddProfile(desc.indom, 1, &instlist[0]);
    pmAddProfile(desc.indom, 1, &instlist[2]);
    pmAddProfile(desc.indom, 1, &instlist[4]);
    free(instlist);
    free(inamelist);

    if ((sts = pmLookupDesc(pmidlist[3], &desc)) < 0) {
	fprintf(stderr, "pmLookupDesc: %s\n", pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmGetInDom(desc.indom, &instlist, &inamelist)) < 0) {
	printf("pmGetInDom: %s\n", pmErrStr(sts));
    }
    pmAddProfile(desc.indom, 1, &instlist[1]);
    pmAddProfile(desc.indom, 1, &instlist[3]);
    free(instlist);
    free(inamelist);

    if ((sts = pmFetch(numpmid, pmidlist, &result)) < 0) {
	printf("pmFetch: botch, %s\n", pmErrStr(sts));
	exit(1);
    }
    __pmDumpResult(stdout, result);

    exit(0);
}
