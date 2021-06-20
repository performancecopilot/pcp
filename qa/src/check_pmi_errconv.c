/*
 * check error handling with pmiWrite after PM_ERR_CONV.
 *
 * Copyright (c) 2021 Red Hat.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/import.h>

int
main(int argc, char **argv)
{
    pmInDom indom = pmInDom_build(245, 0);
    pmID pmid = pmID_build(245, 0, 0);
    int sts;

    if (argc != 2) {
	fprintf(stderr, "Usage: %s <archive>\n", argv[0]);
	exit(2);
    }

    if ((sts = pmiStart(argv[1], 0)) < 0) {
	fprintf(stderr, "pmiStart: %s - %s\n", argv[1], pmiErrStr(sts));
	exit(1);
    }
    if ((sts = pmiSetTimezone("UTC")) < 0) {
	fprintf(stderr, "pmiSetTimezone(UTC): %s\n", pmiErrStr(sts));
	exit(1);
    }

    if ((sts = pmiAddMetric("my.metric.int", pmid, PM_TYPE_32,
			indom, PM_SEM_INSTANT, pmiUnits(0,0,0,0,0,0))) < 0) {
	fprintf(stderr, "pmiAddMetric: %s\n", pmiErrStr(sts));
	exit(1);
    }
    if ((sts = pmiAddInstance(indom, "0", 0)) < 0) {
	fprintf(stderr, "pmiAddInstance(0): %s\n", pmiErrStr(sts));
	exit(1);
    }
    if ((sts = pmiAddInstance(indom, "1", 1)) < 0) {
	fprintf(stderr, "pmiAddInstance(1): %s\n", pmiErrStr(sts));
	exit(1);
    }

    /* FALLTHROUGH on all error paths now to exercise the problem */

    if ((sts = pmiPutValue("my.metric.int", "0", "1234.5678")) < 0)
	fprintf(stderr, "pmiPutValue: inst 0: %s\n", pmiErrStr(sts));

    if ((sts = pmiPutValue("my.metric.int", "1", "123.45678")) < 0)
	fprintf(stderr, "pmiPutValue: inst 1: %s\n", pmiErrStr(sts));

    /* TZ=UTC date --date='@1547483647' */
    if ((sts = pmiWrite(1547483647, 0)) < 0)
	fprintf(stderr, "pmiWrite: %s\n", pmiErrStr(sts));

    if ((sts = pmiEnd()) < 0)
	fprintf(stderr, "pmiEnd: %s\n", pmiErrStr(sts));

    return 0;
}
