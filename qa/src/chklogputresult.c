/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2014,2022 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2017 Red Hat.
 *
 * Excercise __pmLogPutResult() and __pmLogPutResult[23]() and pmaPutMark().
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/archive.h>
#include <assert.h>

int
main(int argc, char **argv)
{
    int		c;
    int		i;
    int		sts;
    int		bflag = 0;
    int		mark = 0;
    char	*endnum;
    int		errflag = 0;
    int		version = PM_LOG_VERS02;
    int		indom_record_type = TYPE_INDOM_V2;
    const char	*metrics[] = {
	"sampledso.long.one",
	"sampledso.ulonglong.ten",
	"sampledso.float.hundred",
	"sampledso.double.million",
	"sampledso.string.hullo",
	"sampledso.bin",
    };
    int		nmetric = sizeof(metrics)/sizeof(metrics[0]);
    pmID	*pmids;
    pmDesc	desc;
    __pmResult	*rp;
    __pmLogCtl	logctl;
    __pmArchCtl	archctl;
    __pmPDU	*pdp;
    /*
     * epoch timestamp:
     * sec 0x0a0b0c = 658188 = 7d 14h 49m 48s (relative to UTC)
     * nsec 0x04030201 = 67305985
     */
    __pmTimestamp	stamp = { 0x0a0b0c, 0x04030201 };
    __pmLogInDom	lid;

    /* trim cmd name of leading directory components */
    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "bD:mV:?")) != EOF) {
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

	case 'm':	/* end with <mark> record ... repeat for muptiples */
	    mark++;
	    break;

	case 'V':	/* archive version */
	    version = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -V requires numeric argument\n", pmGetProgname());
		errflag++;
	    }
	    if (version != PM_LOG_VERS02 && version != PM_LOG_VERS03) {
		fprintf(stderr, "%s: illegal -V value\n", pmGetProgname());
		errflag++;
	    }
	    if (version == PM_LOG_VERS03)
		indom_record_type = TYPE_INDOM;
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
                      __pmLogPutResult[23](), the default\n\
  -D debugflag[,...]\n\
  -V archiveversion\n\
",
                pmGetProgname());
        exit(1);
    }

    putenv("TZ=UTC");
    printf("Expect timestamps to start @");
    __pmPrintTimestamp(stdout, &stamp);
    putchar('\n');

    if ((sts = pmNewContext(PM_CONTEXT_HOST, "local:")) < 0) {
	fprintf(stderr, "%s: Cannot connect to PMCD on \"local:\": %s\n",
		pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    memset(&logctl, 0, sizeof(logctl));
    memset(&archctl, 0, sizeof(archctl));
    archctl.ac_log = &logctl;
    if ((sts = __pmLogCreate("qatest", argv[optind], version, &archctl, 0)) != 0) {
	fprintf(stderr, "%s: __pmLogCreate failed: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    logctl.state = PM_LOG_STATE_INIT;

    /*
     * make the archive label deterministic
     */
    logctl.label.pid = 1234;
    logctl.label.start.sec = stamp.sec;
    logctl.label.start.nsec = stamp.nsec;
    if (logctl.label.hostname)
	free(logctl.label.hostname);
    logctl.label.hostname = strdup("happycamper");
    if (logctl.label.timezone)
	free(logctl.label.timezone);
    logctl.label.timezone = strdup("UTC");
    if (logctl.label.zoneinfo)
	free(logctl.label.zoneinfo);
    logctl.label.zoneinfo = NULL;

    logctl.label.vol = PM_LOG_VOL_TI;
    if ((sts = __pmLogWriteLabel(logctl.tifp, &logctl.label)) != 0) {
	fprintf(stderr, "%s: __pmLogWriteLabel TI failed: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    logctl.label.vol = PM_LOG_VOL_META;
    if ((sts = __pmLogWriteLabel(logctl.mdfp, &logctl.label)) != 0) {
	fprintf(stderr, "%s: __pmLogWriteLabel META failed: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    logctl.label.vol = 0;
    if ((sts = __pmLogWriteLabel(archctl.ac_mfp, &logctl.label)) != 0) {
	fprintf(stderr, "%s: __pmLogWriteLabel VOL 0 failed: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    __pmFflush(archctl.ac_mfp);
    __pmFflush(logctl.mdfp);
    __pmLogPutIndex(&archctl, &stamp);

    lid.next = lid.prior = NULL;
    lid.buf = NULL;
    lid.isdelta = 0;

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
	if ((sts = __pmLogPutDesc(&archctl, &desc, 1, (char **)&metrics[i])) < 0) {
	    fprintf(stderr, "%s: __pmLogPutDesc(\"%s\") failed: %s\n", pmGetProgname(), pmIDStr(pmids[i]), pmErrStr(sts));
	    exit(1);
	}
	if (desc.indom != PM_INDOM_NULL) {
	    if ((lid.numinst = pmGetInDom(desc.indom, &lid.instlist, &lid.namelist)) < 0) {
		printf("pmGetInDom: %s: %s\n", pmInDomStr(desc.indom), pmErrStr(lid.numinst));
		exit(1);
	    }
	    lid.alloc = (PMLID_INSTLIST | PMLID_NAMELIST);
	    lid.indom = desc.indom;
	    lid.stamp = stamp;
	    if ((sts = __pmLogPutInDom(&archctl, indom_record_type, &lid)) < 0) {
		fprintf(stderr, "%s: __pmLogPutInDom(...,indom=%s,numinst=%d,...) failed: %s\n", pmGetProgname(), pmInDomStr(desc.indom), lid.numinst, pmErrStr(sts));
		exit(1);
	    }
	    __pmFreeLogInDom(&lid);
	}
    }
    for (i = 0; i < nmetric; i++) {
	if ((sts = __pmFetch(NULL, i+1, pmids, &rp)) < 0) {
	    fprintf(stderr, "%s: __pmFetch(%d, ...) failed: %s\n", pmGetProgname(), i+1, pmErrStr(sts));
	    exit(1);
	}
	rp->timestamp.sec = ++stamp.sec;
	rp->timestamp.nsec = stamp.nsec;
	if ((sts = __pmEncodeResult(&logctl, rp, &pdp)) < 0) {
	    fprintf(stderr, "%s: __pmEncodeResult failed: %s\n", pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}
	__pmOverrideLastFd(__pmFileno(archctl.ac_mfp));
	if (bflag) {
	    printf("__pmLogPutResult: %d metrics @", i+1);
	    __pmPrintTimestamp(stdout, &rp->timestamp);
	    printf(" ...\n");
	    if ((sts = __pmLogPutResult(&archctl, pdp)) < 0) {
		fprintf(stderr, "%s: __pmLogPutResult failed: %s\n", pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }
	}
	else {
	    if (version == PM_LOG_VERS03) {
		printf("__pmLogPutResult3: %d metrics @", i+1);
		__pmPrintTimestamp(stdout, &rp->timestamp);
		printf(" ...\n");
		if ((sts = __pmLogPutResult3(&archctl, pdp)) < 0) {
		    fprintf(stderr, "%s: __pmLogPutResult3 failed: %s\n", pmGetProgname(), pmErrStr(sts));
		    exit(1);
		}
	    }
	    else {
		printf("__pmLogPutResult2: %d metrics @", i+1);
		__pmPrintTimestamp(stdout, &rp->timestamp);
		printf(" ...\n");
		if ((sts = __pmLogPutResult2(&archctl, pdp)) < 0) {
		    fprintf(stderr, "%s: __pmLogPutResult2 failed: %s\n", pmGetProgname(), pmErrStr(sts));
		    exit(1);
		}
	    }
	}
	__pmUnpinPDUBuf(pdp);
	__pmFreeResult(rp);
    }

    /* optionally add <mark> records at 500msec intervals */
    while (mark > 0) {
	__pmTimestamp	inc = { 0, 500000000 };

	__pmTimestampInc(&stamp, &inc);
	printf("pmaPutMark: @");
	__pmPrintTimestamp(stdout, &stamp);
	putchar('\n');
	if ((sts = __pmLogWriteMark(&archctl, &stamp, NULL)) < 0) {
	    fprintf(stderr, "%s: -maPutMark failed: %s\n", pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}

	mark--;
    }

    __pmFflush(archctl.ac_mfp);
    __pmFflush(logctl.mdfp);
    __pmLogPutIndex(&archctl, &stamp);

    exit(0);
}
