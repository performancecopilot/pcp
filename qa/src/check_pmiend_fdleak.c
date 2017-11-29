/*
 * check for pmiStart/pmiEnd file descriptor leakage
 *
 * Copyright (c) 2016 Red Hat.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/import.h>

static void
check(int sts, char *name)
{
    if (sts < 0)
    	fprintf(stderr, "%s: Error: %s\n", name, pmiErrStr(sts));
}

#define NUMARCHIVES 10

int
main(int argc, char **argv)
{
    int i, c, sts;
    int before, after;
    struct timeval tv;
    char name[MAXPATHLEN];

    if ((before = open("/dev/null", O_RDONLY)) < 0)
    	perror("open");
    close(before);

    for (i=0; i < NUMARCHIVES; i++) {
	pmsprintf(name, sizeof(name), "archive%03d", i);
	c = pmiStart(name, 0);
	check(c, "pmiStart");
	sts = pmiAddMetric("my.metric.long", PM_ID_NULL, PM_TYPE_64,
	    PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(0,0,0,0,0,0));
	check(sts, "pmiAddMetric");
	sts = pmiPutValue("my.metric.long", "", "123456789012345");
	check(sts, "pmiPutValue");

	gettimeofday(&tv, NULL);
	sts = pmiWrite(tv.tv_sec, tv.tv_usec);
	check(sts, "pmiWrite");
	sts = pmiEnd();
	check(sts, "pmiEnd");
    }

    if ((after = open("/dev/null", O_RDONLY)) < 0)
    	perror("open");
    close(after);

    for (i=0; i < NUMARCHIVES; i++) {
	pmsprintf(name, sizeof(name), "archive%03d.0", i);
	unlink(name);
	pmsprintf(name, sizeof(name), "archive%03d.index", i);
	unlink(name);
	pmsprintf(name, sizeof(name), "archive%03d.meta", i);
	unlink(name);
    }

    printf("leaked %d fd\n", after - before);
    exit(after - before);
}
