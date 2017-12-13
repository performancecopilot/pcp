/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2014 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2017 Red Hat.
 *
 * Excercise __pmLogPutResult() and __pmLogPutResult2().
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <assert.h>

int
main(int argc, char **argv)
{
    int		c;
    int		i;
    int		sts;
    int		bflag = 0;
    int		errflag = 0;
    char	*metrics[] = {
	"sampledso.long.one",
	"sampledso.ulonglong.one",
	"sampledso.float.one",
	"sampledso.double.one",
	"sampledso.string.hullo",
	"sampledso.bin",
    };
    int		nmetric = sizeof(metrics)/sizeof(metrics[0]);
    pmID	*pmids;
    pmDesc	desc;
    pmResult	*rp;
    __pmLogCtl	logctl;
    __pmArchCtl	archctl;
    __pmPDU	*pdp;
    pmTimeval	epoch = { 0, 0 };
    int		numinst;
    int		*ilist;
    char	**nlist;

    /* trim cmd name of leading directory components */
    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "bD::?")) != EOF) {
	switch (c) {

	case 'b':	/* backwards compatibility */
	    bflag++;
	    break;

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

    if (errflag || optind != argc-1) {
	fprintf(stderr,
"Usage: %s [options] archive\n\
\n\
Options:\n\
  -b                  backwards compatibility (use __pmLogPutResult() instead\n\
                      __pmLogPutResult2(), the default\n\
  -D debugflag[,...]\n\
",
                pmGetProgname());
        exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, "local:")) < 0) {
	fprintf(stderr, "%s: Cannot connect to PMCD on \"local:\": %s\n",
		pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    memset(&logctl, 0, sizeof(logctl));
    memset(&archctl, 0, sizeof(archctl));
    archctl.ac_log = &logctl;
    if ((sts = __pmLogCreate("qatest", argv[optind], LOG_PDU_VERSION, &archctl)) != 0) {
	fprintf(stderr, "%s: __pmLogCreate failed: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    logctl.l_state = PM_LOG_STATE_INIT;

    /*
     * make the archive label deterministic
     */
    logctl.l_label.ill_pid = 1234;
    logctl.l_label.ill_start.tv_sec = epoch.tv_sec;
    logctl.l_label.ill_start.tv_usec = epoch.tv_usec;
    strcpy(logctl.l_label.ill_hostname, "happycamper");
    strcpy(logctl.l_label.ill_tz, "UTC");

    logctl.l_label.ill_vol = PM_LOG_VOL_TI;
    if ((sts = __pmLogWriteLabel(logctl.l_tifp, &logctl.l_label)) != 0) {
	fprintf(stderr, "%s: __pmLogWriteLabel TI failed: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    logctl.l_label.ill_vol = PM_LOG_VOL_META;
    if ((sts = __pmLogWriteLabel(logctl.l_mdfp, &logctl.l_label)) != 0) {
	fprintf(stderr, "%s: __pmLogWriteLabel META failed: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    logctl.l_label.ill_vol = 0;
    if ((sts = __pmLogWriteLabel(archctl.ac_mfp, &logctl.l_label)) != 0) {
	fprintf(stderr, "%s: __pmLogWriteLabel VOL 0 failed: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    __pmFflush(archctl.ac_mfp);
    __pmFflush(logctl.l_mdfp);
    __pmLogPutIndex(&archctl, &epoch);

    pmids = (pmID *)malloc(nmetric*sizeof(pmID));
    assert(pmids != NULL);
    for (i = 0; i < nmetric; i++) {
	if ((sts = pmLookupName(1, &metrics[i], &pmids[i])) != 1) {
	    fprintf(stderr, "%s: pmLookupName(\"%s\") failed: %s\n", pmGetProgname(), metrics[i], pmErrStr(sts));
	    exit(1);
	}
	printf("%s -> %s\n", metrics[i], pmIDStr(pmids[i]));
	if ((sts = pmLookupDesc(pmids[i], &desc)) < 0) {
	    fprintf(stderr, "%s: pmLookupDesc(\"%s\") failed: %s\n", pmGetProgname(), pmIDStr(pmids[i]), pmErrStr(sts));
	    exit(1);
	}
	if ((sts = __pmLogPutDesc(&archctl, &desc, 1, &metrics[i])) < 0) {
	    fprintf(stderr, "%s: __pmLogPutDesc(\"%s\") failed: %s\n", pmGetProgname(), pmIDStr(pmids[i]), pmErrStr(sts));
	    exit(1);
	}
	if (desc.indom != PM_INDOM_NULL) {
	    if ((numinst = pmGetInDom(desc.indom, &ilist, &nlist)) < 0) {
		printf("pmGetInDom: %s: %s\n", pmInDomStr(desc.indom), pmErrStr(numinst));
		exit(1);
	    }
	    if ((sts = __pmLogPutInDom(&archctl, desc.indom, &epoch, numinst, ilist, nlist)) < 0) {
		fprintf(stderr, "%s: __pmLogPutInDom(...,indom=%s,numinst=%d,...) failed: %s\n", pmGetProgname(), pmInDomStr(desc.indom), numinst, pmErrStr(sts));
		exit(1);
	    }
	}
    }
    for (i = 0; i < nmetric; i++) {
	if ((sts = pmFetch(i+1, pmids, &rp)) < 0) {
	    fprintf(stderr, "%s: pmFetch(%d, ...) failed: %s\n", pmGetProgname(), i+1, pmErrStr(sts));
	    exit(1);
	}
	rp->timestamp.tv_sec = ++epoch.tv_sec;
	rp->timestamp.tv_usec = epoch.tv_usec;
	if ((sts = __pmEncodeResult(__pmFileno(archctl.ac_mfp), rp, &pdp)) < 0) {
	    fprintf(stderr, "%s: __pmEncodeResult failed: %s\n", pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}
	__pmOverrideLastFd(__pmFileno(archctl.ac_mfp));
	if (bflag) {
	    printf("__pmLogPutResult: %d metrics ...\n", i+1);
	    if ((sts = __pmLogPutResult(&archctl, pdp)) < 0) {
		fprintf(stderr, "%s: __pmLogPutResult failed: %s\n", pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }
	}
	else {
	    printf("__pmLogPutResult2: %d metrics ...\n", i+1);
	    if ((sts = __pmLogPutResult2(&archctl, pdp)) < 0) {
		fprintf(stderr, "%s: __pmLogPutResult2 failed: %s\n", pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }
	}
	__pmUnpinPDUBuf(pdp);
	pmFreeResult(rp);
    }

    __pmFflush(archctl.ac_mfp);
    __pmFflush(logctl.l_mdfp);
    __pmLogPutIndex(&archctl, &epoch);

    return 0;
}
