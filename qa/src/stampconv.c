/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017-2021 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_DEBUG,	/* -D */
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:?",
    .long_options = longopts,
    .short_usage = "[options] ...",
};

int
main(int argc, char **argv)
{
    int			c;
    char		*p;
    __pmTimestamp	input;
    __pmTimestamp	stamp;
    __int32_t		buf[3];

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    if (opts.flags & PM_OPTFLAG_EXIT || opts.optind != argc-2) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    input.sec = strtoll(argv[argc-2], &p, 10);
    if (*p != '\0') {
	fprintf(stderr, "sec (%s): bad format\n", argv[argc-2]);
	exit(1);
    }

    input.nsec = strtol(argv[argc-1], &p, 10);
    if (*p != '\0') {
	fprintf(stderr, "nsec (%s): bad format\n", argv[argc-1]);
	exit(1);
    }

    stamp = input;
    printf("__pmTimestamp input: %" FMT_INT64 ".%09d (%016llx %08x)\n", stamp.sec, stamp.nsec, (unsigned long long)stamp.sec, stamp.nsec);
    buf[0] = 0xaaaaaaaa;
    buf[1] = 0xbbbbbbbb;
    buf[2] = 0xcccccccc;
    __pmPutTimestamp(&stamp, buf);
    if (pmDebugOptions.appl0)
	printf("buf: %08x %08x %08x\n", buf[0], buf[1] ,buf[2]);
    stamp.sec = stamp.nsec = 0;
    __pmLoadTimestamp(buf, &stamp);
    printf("output: %" FMT_INT64 ".%09d (%016llx %08x)\n", stamp.sec, stamp.nsec, (unsigned long long)stamp.sec, stamp.nsec);

    printf("\n");
    stamp = input;
// check-time-formatting-ok
    printf("pmTimeval input: % " FMT_INT64 ".%06d (%016llx %08x)\n", stamp.sec, stamp.nsec / 1000, (unsigned long long)stamp.sec, stamp.nsec / 1000);
    buf[0] = 0xaaaaaaaa;
    buf[1] = 0xbbbbbbbb;
    buf[2] = 0xcccccccc;
    __pmPutTimeval(&stamp, buf);
    if (pmDebugOptions.appl0)
	printf("buf: %08x %08x %08x\n", buf[0], buf[1] ,buf[2]);
    stamp.sec = stamp.nsec = 0;
    __pmLoadTimeval(buf, &stamp);
// check-time-formatting-ok
    printf("output: % " FMT_INT64 ".%06d (%016llx %08x)\n", stamp.sec, stamp.nsec / 1000, (unsigned long long)stamp.sec, stamp.nsec / 1000);

    return 0;
}
