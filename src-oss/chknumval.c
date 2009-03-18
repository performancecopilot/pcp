/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * chknumval - check out new encoding of errors within numval of a pmResult
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
    static char	*debug = "[-D N]";
    static char	*usage = "[-D N] [-h hostname] [-n namespace] [-v]";
    int		i;
    int		ctlport;
    int		pid = PM_LOG_PRIMARY_PID;
    int		port = PM_LOG_NO_PORT;
    char	*namelist[20];
    pmID	pmidlist[20];
    int		*instlist;
    char	**inamelist;
    int		numpmid;
    pmResult	*req;
    pmResult	*status;
    pmDesc	desc;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:n:")) != EOF) {
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

    if ((sts = pmLoadNameSpace(namespace)) < 0) {
	printf("Usage: %s %s%s\n", pmProgname, debug, usage);
	exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, host)) < 0) {
	printf("%s: Cannot connect to PMCD on host \"%s\": %s\n", pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    if ((sts = __pmConnectLogger(host, &pid, &port)) < 0) {
	printf("%s: Cannot connect to primary pmlogger on host \"%s\": %s\n", pmProgname, host, pmErrStr(sts));
	exit(1);
    }
    ctlport = sts;

    i = 0;
    namelist[i++] = "pmcd.control.debug";
    namelist[i++] = "sampledso.long.write_me";
    namelist[i++] = "sample.colour";
    numpmid = i;

    sts = pmLookupName(numpmid, namelist, pmidlist);
    if (sts < 0) {
	printf("pmLookupName: %s\n", pmErrStr(sts));
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
	printf("__pmControlLog: botch, PM_ERR_VALUE expected\n");
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
	printf("__pmControlLog: botch, PM_ERR_VALUE expected\n");
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
	printf("__pmControlLog: botch, PM_ERR_VALUE expected\n");
	pmFreeResult(status);
    }

    /* exercise *.needprofile */
    pmFreeResult(req);

    i = 0;
    namelist[i++] = "sampledso.long.hundred";
    namelist[i++] = "sampledso.needprofile";
    namelist[i++] = "sample.long.hundred";
    namelist[i++] = "sample.needprofile";
    numpmid = i;

    sts = pmLookupName(numpmid, namelist, pmidlist);
    if (sts < 0) {
	printf("pmLookupName: botch, %s\n", pmErrStr(sts));
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
