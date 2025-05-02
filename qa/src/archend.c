/*
 * endarchive - exercise __pmEndArchive
 *
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2022 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

static void
printstamp(struct timespec *tp)
{
    static struct tm	*tmp;
    time_t		clock = (time_t)tp->tv_sec;

    tmp = localtime(&clock);
    printf("%02d:%02d:%02d.%09d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)tp->tv_nsec);
}

int
main(int argc, char **argv)
{
    int			c;
    int			sts;
    int			errflag = 0;
    struct timespec	end;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
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

    /* non-flag args are argv[optind] ... argv[argc-1] */
    if (optind != argc-1)
	errflag++;

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options] archive\n\
\n\
Options\n\
  -D   debug flags\n",
		pmGetProgname());
	exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, argv[optind])) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
	    pmGetProgname(), argv[optind], pmErrStr(sts));
	exit(1);
    }

    sts = pmGetArchiveEnd(&end);
    if (sts < 0) {
	printf("pmGetArchiveEnd: %s\n", pmErrStr(sts));
    }
    else {
	printf("pmGetArchiveEnd time: ");
	printstamp(&end);
	printf("\n");
    }

    exit(0);
}
