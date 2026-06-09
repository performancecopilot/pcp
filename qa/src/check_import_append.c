/*
 * Exercise PMI_APPEND flag to pmiStart() - create an archive then
 * append additional records to it in a separate context.
 *
 * Copyright (c) 2026 Red Hat.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/import.h>

static void
check(int sts, char *name)
{
    if (sts < 0)
	fprintf(stderr, "%s: Error: %s\n", name, pmiErrStr(sts));
    else {
	fprintf(stderr, "%s: OK", name);
	if (sts != 0) fprintf(stderr, " ->%d", sts);
	fputc('\n', stderr);
    }
}

int
main(int argc, char **argv)
{
    int		sts;
    int		ctx;

    pmSetProgname(argv[0]);

    /*
     * Phase 1: create a fresh archive with two records.
     */
    fprintf(stderr, "=== Phase 1: create ===\n");

    ctx = pmiStart("appendtest", 0);
    check(ctx, "pmiStart");

    sts = pmiSetHostname("testhost.example.com");
    check(sts, "pmiSetHostname");

    sts = pmiSetTimezone("UTC");
    check(sts, "pmiSetTimezone");

    sts = pmiAddMetric("qa.append.counter",
		       PM_ID_NULL, PM_TYPE_U32, PM_INDOM_NULL,
		       PM_SEM_COUNTER, pmiUnits(0, 0, 0, 0, 0, 0));
    check(sts, "pmiAddMetric counter");

    sts = pmiAddMetric("qa.append.instant",
		       PM_ID_NULL, PM_TYPE_DOUBLE, PM_INDOM_NULL,
		       PM_SEM_INSTANT, pmiUnits(0, 0, 0, 0, 0, 0));
    check(sts, "pmiAddMetric instant");

    sts = pmiPutValue("qa.append.counter", NULL, "10");
    check(sts, "pmiPutValue counter (1)");
    sts = pmiPutValue("qa.append.instant", NULL, "1.5");
    check(sts, "pmiPutValue instant (1)");
    sts = pmiWrite(1000, 0);
    check(sts, "pmiWrite (1)");

    sts = pmiPutValue("qa.append.counter", NULL, "20");
    check(sts, "pmiPutValue counter (2)");
    sts = pmiPutValue("qa.append.instant", NULL, "2.5");
    check(sts, "pmiPutValue instant (2)");
    sts = pmiWrite(2000, 0);
    check(sts, "pmiWrite (2)");

    sts = pmiEnd();
    check(sts, "pmiEnd");

    /*
     * Phase 2: reopen with PMI_APPEND and add two more records.
     * Timestamps must be strictly after the last one written (2000s).
     */
    fprintf(stderr, "\n=== Phase 2: append ===\n");

    ctx = pmiStart("appendtest", PMI_APPEND);
    check(ctx, "pmiStart (PMI_APPEND)");

    /* Metrics must be re-registered so pmiPutValue() can look them up */
    sts = pmiAddMetric("qa.append.counter",
		       PM_ID_NULL, PM_TYPE_U32, PM_INDOM_NULL,
		       PM_SEM_COUNTER, pmiUnits(0, 0, 0, 0, 0, 0));
    check(sts, "pmiAddMetric counter (append)");

    sts = pmiAddMetric("qa.append.instant",
		       PM_ID_NULL, PM_TYPE_DOUBLE, PM_INDOM_NULL,
		       PM_SEM_INSTANT, pmiUnits(0, 0, 0, 0, 0, 0));
    check(sts, "pmiAddMetric instant (append)");

    sts = pmiPutValue("qa.append.counter", NULL, "30");
    check(sts, "pmiPutValue counter (3)");
    sts = pmiPutValue("qa.append.instant", NULL, "3.5");
    check(sts, "pmiPutValue instant (3)");
    sts = pmiWrite(3000, 0);
    check(sts, "pmiWrite (3)");

    sts = pmiPutValue("qa.append.counter", NULL, "40");
    check(sts, "pmiPutValue counter (4)");
    sts = pmiPutValue("qa.append.instant", NULL, "4.5");
    check(sts, "pmiPutValue instant (4)");
    sts = pmiWrite(4000, 0);
    check(sts, "pmiWrite (4)");

    sts = pmiEnd();
    check(sts, "pmiEnd (append)");

    /*
     * Phase 3: PMI_APPEND on a non-existent archive should silently
     * create a new one (fallback behaviour).
     */
    fprintf(stderr, "\n=== Phase 3: append-creates-new ===\n");

    ctx = pmiStart("appendnew", PMI_APPEND);
    check(ctx, "pmiStart (PMI_APPEND, new archive)");

    sts = pmiAddMetric("qa.append.counter",
		       PM_ID_NULL, PM_TYPE_U32, PM_INDOM_NULL,
		       PM_SEM_COUNTER, pmiUnits(0, 0, 0, 0, 0, 0));
    check(sts, "pmiAddMetric counter (new)");

    sts = pmiPutValue("qa.append.counter", NULL, "1");
    check(sts, "pmiPutValue counter (new)");
    sts = pmiWrite(1000, 0);
    check(sts, "pmiWrite (new)");

    sts = pmiEnd();
    check(sts, "pmiEnd (new)");

    exit(0);
}
