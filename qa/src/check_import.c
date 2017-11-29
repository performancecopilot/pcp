/*
 * Exercise libpcp_import
 *
 * Copyright (c) 2009 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/import.h>

static void
check(int sts, char *name)
{
    if (sts < 0) fprintf(stderr, "%s: Error: %s\n", name, pmiErrStr(sts));
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
    int		ctx1;
    int		ctx2;
    int		hdl1;
    int		hdl2;
    int		errflag = 0;
    int		c;
    static char	*usage = "[-D debugspec] ";

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:")) != EOF) {
	switch (c) {

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

    if (errflag) {
	printf("Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    pmiDump();

    ctx1 = pmiStart("myarchive", 0);
    check(ctx1, "pmiStart");
    pmiDump();

    sts = pmiSetHostname("somehost.com");
    check(sts, "pmiSetHostname");
    sts = pmiSetTimezone("GMT-12");
    check(sts, "pmiSetTimezone");

    sts = pmiUseContext(3);
    check(sts, "pmiUseContext");
    sts = pmiUseContext(ctx1);
    check(sts, "pmiUseContext");

    sts = pmiAddMetric("my.metric.foo", pmID_build(245,0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(1,-1,0,PM_SPACE_MBYTE,PM_TIME_SEC,0));
    check(sts, "pmiAddMetric");
    sts = pmiAddMetric("my.metric.bar", PM_ID_NULL, PM_TYPE_U64, pmInDom_build(245,1), PM_SEM_INSTANT, pmiUnits(1,-1,0,PM_SPACE_MBYTE,PM_TIME_SEC,0));
    check(sts, "pmiAddMetric");
    sts = pmiAddMetric("my.metric.foo", 1, 2, 3, 4, pmiUnits(0,0,0,0,0,0));
    check(sts, "pmiAddMetric");
    sts = pmiAddMetric("my.metric.long", PM_ID_NULL, PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(0,0,0,0,0,0));
    check(sts, "pmiAddMetric");
    sts = pmiAddMetric("my.metric.double", PM_ID_NULL, PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(0,0,0,0,0,0));
    check(sts, "pmiAddMetric");
    sts = pmiAddMetric("my.metric.string", PM_ID_NULL, PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(0,0,0,0,0,0));
    check(sts, "pmiAddMetric");
    sts = pmiAddMetric("my.dup.pmid", pmID_build(245,0,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(0,0,0,0,0,0));
    check(sts, "pmiAddMetric");
    sts = pmiAddMetric("my.metric.float", PM_ID_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_DISCRETE, pmiUnits(0,0,0,0,0,0));
    check(sts, "pmiAddMetric");

    sts = pmiAddInstance(pmInDom_build(245,1), "eek really", 1);
    check(sts, "pmiAddInstance");
    sts = pmiAddInstance(pmInDom_build(245,1), "eek", 2);
    check(sts, "pmiAddInstance");
    sts = pmiAddInstance(pmInDom_build(245,1), "blah", 3);
    check(sts, "pmiAddInstance");
    sts = pmiAddInstance(pmInDom_build(245,1), "not-blah-again", 3);
    check(sts, "pmiAddInstance");

    sts = pmiPutValue("my.metric.foo", NULL, "123");
    check(sts, "pmiPutValue");
    sts = pmiPutValue("my.metric.foo", "should be null", "1234");
    check(sts, "pmiPutValue");
    sts = pmiPutValue("my.metric.bar", "eek", "4567890123456");
    check(sts, "pmiPutValue");
    sts = pmiPutValue("my.metric.bar", "blah", "4567890123457");
    check(sts, "pmiPutValue");
    sts = pmiPutValue("my.metric.bar", "not-blah", "4567890123457");
    check(sts, "pmiPutValue");
    sts = pmiPutValue("my.metric.bar", NULL, "42");
    check(sts, "pmiPutValue");
    sts = pmiPutValue("my.metric.string", "", "a new string value");
    check(sts, "pmiPutValue");
    sts = pmiPutValue("my.metric.long", "", "123456789012345");
    check(sts, "pmiPutValue");
    sts = pmiPutValue("my.metric.double", "", "1.23456789012");
    check(sts, "pmiPutValue");
    sts = pmiPutValue("my.metric.float", "", "-1.234567");
    check(sts, "pmiPutValue");

    hdl1 = pmiGetHandle("my.metric.foo", "");
    check(hdl1, "pmiGetHandle");
    sts = pmiGetHandle("my.bad", "");
    check(sts, "pmiGetHandle");
    sts = pmiPutValueHandle(hdl1, "321");
    check(sts, "pmiPutValueHandle");
    sts = pmiPutValueHandle(0, "error");
    check(sts, "pmiPutValueHandle");

    pmiDump();

    sts = pmiWrite((int)(365.25*30*24*60*60), 0);
    check(sts, "pmiWrite");
    sts = pmiPutValueHandle(hdl1, "4321");
    check(sts, "pmiPutValueHandle");
    sts = pmiPutValue("my.metric.string", "", "a second string value");
    check(sts, "pmiPutValue");
    sts = pmiWrite(-1, -1);
    check(sts, "pmiWrite");
    sts = pmiPutValue("my.metric.string", "", "a third string value");
    check(sts, "pmiPutValue");
    sts = pmiWrite(-1, -1);
    check(sts, "pmiWrite");
    sts = pmiWrite(-1, -1);
    check(sts, "pmiWrite");

    sts = pmiPutMark();
    check(sts, "pmiPutMark");

    sts = pmiEnd();
    check(sts, "pmiEnd");

    ctx2 = pmiStart("myotherarchive", 1);
    check(ctx2, "pmiStart");
    sts = pmiAddInstance(pmInDom_build(245,1), "other", 2);
    check(sts, "pmiAddInstance");
    hdl2 = pmiGetHandle("my.metric.bar", "eek");
    check(hdl2, "pmiGetHandle");
    sts = pmiPutValueHandle(hdl2, "6543210987654");
    check(sts, "pmiPutValueHandle");
    sts = pmiPutValueHandle(3, "error");
    check(sts, "pmiPutValueHandle");

    pmiDump();

    exit(0);
}
