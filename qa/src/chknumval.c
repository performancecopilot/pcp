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
#include <pcp/impl.h>

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
    char	*namelist[20];
    pmID	pmidlist[20];
    int		*instlist;
    char	**inamelist;
    int		numpmid = 0;
    int		default_metrics = 0;
    pmResult	*req;
    pmResult	*status;
    pmDesc	desc;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:K:Ln:")) != EOF) {
	switch (c) {

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

	case 'h':	/* hostname for PMCD to contact */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of --h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'K':	/* update local PMDA table */
	    if ((errmsg = __pmSpecLocalPMDA(optarg)) != NULL) {
		fprintf(stderr, "%s: __pmSpecLocalPMDA failed: %s\n", pmProgname, errmsg);
		errflag++;
	    }
	    break;

	case 'L':	/* local PMDA connection, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -h and -L allowed\n", pmProgname);
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
  -D debug	standard PCP debug flag\n\
  -h host	metrics source is PMCD on host (default is localhost)\n\
  -L            metrics source is local connection to PMDA, no PMCD\n\
  -K spec       optional additional PMDA spec for local connection\n\
                spec is of the form op,domain,dso-path,init-routine\n\
  -n namespace  use an alternative PMNS\n\
  -v            be verbose\n",
		pmProgname);
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
	printf("%s: pmLoadASCIINameSpace: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    if (type == 0) type = PM_CONTEXT_HOST;
    if ((sts = pmNewContext(type, host)) < 0) {
	printf("%s: pmNewContext(%d, %s): %s\n", pmProgname, type, host, pmErrStr(sts));
	exit(1);
    }

    if ((sts = __pmConnectLogger(host, &pid, &port)) < 0) {
	printf("%s: Cannot connect to primary pmlogger on host \"%s\": %s\n", pmProgname, host, pmErrStr(sts));
	exit(1);
    }
    ctlport = sts;

    if (numpmid == 0) {
	/* default metrics */
	default_metrics = 1;
	i = 0;
	namelist[i++] = "pmcd.control.debug";
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

    if ((sts = pmFetch(numpmid, pmidlist, &req)) < 0) {
	printf("%s: pmFetch: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    if ((sts = __pmControlLog(ctlport, req, PM_LOG_MANDATORY, PM_LOG_OFF, 0, &status)) < 0) {
	printf("__pmControlLog: %s\n", pmErrStr(sts));
	exit(1);
    }

    __pmDumpResult(stdout, req);

    printf("\nbase store test (failures not unexpected) ...\n");
    if ((sts = pmStore(req)) < 0)
	printf("pmStore: %s\n", pmErrStr(sts));
    else
	printf("pmStore: OK\n");

    printf("\nnumpmid == 0 tests (failures expected) ...\n");
    req->numpmid = 0;
    __pmDumpResult(stdout, req);

    if ((sts = pmStore(req)) < 0)
	printf("pmStore: %s\n", pmErrStr(sts));
    else
	printf("pmStore: botch, PM_ERR_TOOSMALL expected\n");
    if ((sts = __pmControlLog(ctlport, req, PM_LOG_ENQUIRE, 0, 0, &status)) < 0)
	printf("__pmControlLog: %s\n", pmErrStr(sts));
    else {
	printf("__pmControlLog: OK\n");
	__pmDumpResult(stdout, status);
	pmFreeResult(status);
    }

    printf("\nnumval == 0 tests (failures expected) ...\n");
    req->numpmid = numpmid;
    req->vset[1]->numval = 0;
    __pmDumpResult(stdout, req);

    if ((sts = pmStore(req)) < 0)
	printf("pmStore: %s\n", pmErrStr(sts));
    else
	printf("pmStore: botch, PM_ERR_VALUE expected\n");
    if ((sts = __pmControlLog(ctlport, req, PM_LOG_ENQUIRE, 0, 0, &status)) < 0)
	printf("__pmControlLog: %s\n", pmErrStr(sts));
    else {
	printf("__pmControlLog: OK\n");
	__pmDumpResult(stdout, status);
	pmFreeResult(status);
    }

    printf("\nnumval < 0 tests (failures expected) ...\n");
    req->vset[1]->numval = 1;
    req->vset[2]->numval = PM_ERR_NOAGENT;
    __pmDumpResult(stdout, req);

    if ((sts = pmStore(req)) < 0)
	printf("pmStore: %s\n", pmErrStr(sts));
    else
	printf("pmStore: botch, PM_ERR_VALUE expected\n");
    if ((sts = __pmControlLog(ctlport, req, PM_LOG_ENQUIRE, 0, 0, &status)) < 0)
	printf("__pmControlLog: %s\n", pmErrStr(sts));
    else {
	printf("__pmControlLog: OK\n");
	__pmDumpResult(stdout, status);
	pmFreeResult(status);
    }

    /* exercise *.needprofile */
    pmFreeResult(req);

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

    if ((sts = pmFetch(numpmid, pmidlist, &req)) < 0) {
	printf("pmFetch: botch, %s\n", pmErrStr(sts));
	exit(1);
    }
    __pmDumpResult(stdout, req);

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

    if ((sts = pmFetch(numpmid, pmidlist, &req)) < 0) {
	printf("pmFetch: botch, %s\n", pmErrStr(sts));
	exit(1);
    }
    __pmDumpResult(stdout, req);

    exit(0);
}
