/*
 * Exercise libpcp_import metric name validation.
 *
 * Copyright (c) 2013 Red Hat.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/import.h>

static void
check(int sts, char *name)
{
    if (sts < 0) {
	fprintf(stderr, "%s: Error: %s\n", name, pmiErrStr(sts));
    } else {
	fprintf(stderr, "%s: OK", name);
	if (sts != 0)
	    fprintf(stderr, " -> %d", sts);
	fputc('\n', stderr);
    }
}

int
main(int argc, char **argv)
{
    int		sts;

    if (argc != 2) {
	printf("Usage: %s <name>\n", argv[0]);
	exit(1);
    }

    sts = pmiStart("tmplog", 0);
    check(sts, "pmiStart");
    sts = pmiSetHostname("tmphost.com");
    check(sts, "pmiSetHostname");
    sts = pmiSetTimezone("GMT-12");
    check(sts, "pmiSetTimezone");

    printf("Checking metric name: \"%s\" ...\n", argv[1]);
    sts = pmiAddMetric(argv[1], pmid_build(245,0,1),
				PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
				pmiUnits(1,-1,0,PM_SPACE_MBYTE,PM_TIME_SEC,0));
    check(sts, "pmiAddMetric");
    exit(0);
}
