/*
 * checkout localtime() and pmlocaltime()
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017,2025 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/archive.h>

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,	/* -D */
    PMOPT_TIMEZONE,	/* -Z */
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_STDOUT_TZ,
    .short_options = "D:Z:?",
    .long_options = longopts,
    .short_usage = "[options]",
};

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    time_t	now = time(NULL);
    struct tm	platform;
    struct tm	pcp;
    int		zone;
    time_t	check;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, "local:")) < 0) {
	printf("Arrgh: pmNewContext: %s\n", pmErrStr(sts));
	exit(EXIT_FAILURE);
    }

    if (opts.timezone != NULL) {
	printf("Got -Z \"%s\"\n", opts.timezone);
	if ((sts = setenv("TZ", opts.timezone, 1)) < 0) {
	    printf("Arrgh: setenv: %s\n", pmErrStr(-oserror()));
	    exit(EXIT_FAILURE);
	}
	if ((zone = pmNewZone(opts.timezone)) < 0) {
	    printf("Arrgh: pmNewZone: %s\n", pmErrStr(zone));
	    exit(EXIT_FAILURE);
	}
    }

    localtime_r(&now, &platform);
    printf("platform localtime: %02d:%02d:%02d", platform.tm_hour, platform.tm_min, platform.tm_sec);
    if (platform.tm_isdst) printf("+DST");
    putchar('\n');
    check = mktime(&platform);
    if (check != now)
	printf("Botch: time_t mismatch %ld vs %ld\n", (long)now, (long)check);

    pmLocaltime(&now, &pcp);
    printf("libpcp localtime: %02d:%02d:%02d", pcp.tm_hour, pcp.tm_min, pcp.tm_sec);
    if (pcp.tm_isdst) printf("+DST");
    putchar('\n');
    if (platform.tm_hour != pcp.tm_hour)
	printf("Botch: hour mismatch %d vs %d\n", platform.tm_hour, pcp.tm_hour);
    if (platform.tm_min != pcp.tm_min)
	printf("Botch: min mismatch %d vs %d\n", platform.tm_min, pcp.tm_min);
    if (platform.tm_sec != pcp.tm_sec)
	printf("Botch: sec mismatch %d vs %d\n", platform.tm_sec, pcp.tm_sec);
    check = mktime(&pcp);
    if (check != now)
	printf("Botch: time_t mismatch %ld vs %ld\n", (long)now, (long)check);

    pmLocaltime(&now, &pcp);
    printf("context localtime: %02d:%02d:%02d", pcp.tm_hour, pcp.tm_min, pcp.tm_sec);
    if (pcp.tm_isdst) printf("+DST");
    putchar('\n');
    if (platform.tm_hour != pcp.tm_hour)
	printf("Botch: hour mismatch %d vs %d\n", platform.tm_hour, pcp.tm_hour);
    if (platform.tm_min != pcp.tm_min)
	printf("Botch: min mismatch %d vs %d\n", platform.tm_min, pcp.tm_min);
    if (platform.tm_sec != pcp.tm_sec)
	printf("Botch: sec mismatch %d vs %d\n", platform.tm_sec, pcp.tm_sec);
    check = mktime(&pcp);
    if (check != now)
	printf("Botch: time_t mismatch %ld vs %ld\n", (long)now, (long)check);

    return 0;
}
