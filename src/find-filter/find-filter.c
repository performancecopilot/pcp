/*
 * Copyright (c) 2020 Ken McDonell.  All Rights Reserved.
 */
#include "pmapi.h"
#include <sys/stat.h>

static void
usage(void) {
    fprintf(stderr, "Usage: find-filter [options] <predicate>\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -v                 verbose\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "<predicate> is one of the following:\n");
    fprintf(stderr, "  ctime <timespec>   filter on a file's ctime\n");
    fprintf(stderr, "  mtime <timespec>   filter on a file's mtime\n");
    fprintf(stderr, "and <timespec> is one of the following:\n");
    fprintf(stderr, "  +D[:H[:M]]         file's time is > specified days, hours and minutes ago\n");
    fprintf(stderr, "  -D[:H[:M]]         file's time is <= specified days, hours and minutes ago\n");
    exit(1);
}

#define PRED_NONE	-1
#define PRED_CTIME	1
#define PRED_MTIME	2

#define PLUS_TIME	1
#define MINUS_TIME	2

int
main(int argc, char **argv)
{
    struct timeval	now;
    double		now_f;
    struct timeval	stamp;			/* time in past */
    double		stamp_f;		/* stamp as real number */
    int			pred = PRED_NONE;
    int			sign = 0;		/* pander to gcc on NetBSD */
    int			tmp;
    int			verbose = 0;
    char		*p;
    char		*q;
    char		path[MAXPATHLEN+1];

    /* can't use getopt() because <timespec> may begin with '-' */
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
	verbose++;
	argc--;
	argv++;
    }

    if (argc < 2) {
	usage();
	/*NOTREACHED*/
    }

    /* Start with current time, then timespec refers to an offset into
     * the past.
     */
    pmtimevalNow(&now);
    now_f = pmtimevalToReal(&now);
    stamp = now;		/* struct assignment */

    if (strcmp(argv[1], "ctime") == 0)
	pred = PRED_CTIME;
    else if (strcmp(argv[1], "mtime") == 0)
	pred = PRED_MTIME;

    switch (pred) {
	case PRED_CTIME:
	case PRED_MTIME:
	    if (argc < 3) {
		usage();
		/*NOTREACHED*/
	    }
	    p = argv[2];
	    if (*p == '+')
		sign = PLUS_TIME;
	    else if (*p == '-')
		sign = MINUS_TIME;
	    else {
		fprintf(stderr, "find-filter: Error: expect '+' or '-' to start timespec, not '%c' in \"%s\"\n", *p, argv[2]);
		exit(1);
	    }
	    p++;
	    if (*p == '\0') {
		fprintf(stderr, "find-filter: Error: missing number of days in timespec \"%s\"\n", argv[2]);
		exit(1);
	    }
	    /* start with days */
	    stamp.tv_sec -= strtol(p, &q, 10)*24*3600;
	    if (p == q) {
		fprintf(stderr, "find-filter: Error: expect number of days not '%c' after %c in timespec\"%s\"\n", *p, p[-1], argv[2]);
		exit(1);
	    }
	    if (*q == '\0') {
		/* just [+|-]D */
		break;
	    }
	    p = q;
	    if (*p != ':') {
		fprintf(stderr, "find-filter: Error: expect ':' not '%c' after days in timespec \"%s\"\n", *p, argv[2]);
		exit(1);
	    }
	    /* now expect hours */
	    p++;
	    if (*p == '\0') {
		fprintf(stderr, "find-filter: Error: missing number of hours in timespec \"%s\"\n", argv[2]);
		exit(1);
	    }
	    tmp = strtol(p, &q, 10);
	    if (p == q) {
		fprintf(stderr, "find-filter: Error: expect number of hours not '%c' in timespec \"%s\"\n", *p, argv[2]);
		exit(1);
	    }
	    if (tmp < 0 || tmp > 23) {
		fprintf(stderr, "find-filter: Error: number of hours '%d' is invalid in timespec \"%s\"\n", tmp, argv[2]);
		exit(1);
	    }
	    stamp.tv_sec -= tmp*3600;
	    if (*q == '\0') {
		/* just [+|-]D:H */
		break;
	    }
	    p = q;
	    if (*p != ':') {
		fprintf(stderr, "find-filter: Error: expect ':' not '%c' after days and hours in timespec \"%s\"\n", *p, argv[2]);
		exit(1);
	    }
	    /* now expect minutes */
	    p++;
	    if (*p == '\0') {
		fprintf(stderr, "find-filter: Error: missing number of minutes in timespec \"%s\"\n", argv[2]);
		exit(1);
	    }
	    tmp = strtol(p, &q, 10);
	    if (p == q) {
		fprintf(stderr, "find-filter: Error: expect number of minutes not '%c' in timespec \"%s\"\n", *p, argv[2]);
		exit(1);
	    }
	    if (tmp < 0 || tmp > 59) {
		fprintf(stderr, "find-filter: Error: number of minutes '%d' is invalid in timespec \"%s\"\n", tmp, argv[2]);
		exit(1);
	    }
	    if (*q != '\0') {
		fprintf(stderr, "find-filter: Error: extra text after days, hours and minutes in timespec \"%s\"\n", argv[2]);
		exit(1);
	    }
	    stamp.tv_sec -= tmp*60;
	    break;

	default:
	    usage();
	    /*NOTREACHED*/
	   
    }
    stamp_f = pmtimevalToReal(&stamp);

    /*
     * Now for each pathname on stdin, stat the file and output
     * the pathname if the file's attributes pass the predicate.
     */
    while (fgets(path, MAXPATHLEN-1, stdin) != NULL) {
	struct stat	sbuf;
	int		sts;
	struct timeval	check;		/* file's time value */
	double		check_f;
	
	/* strip trailing \n */
	for (p = path; *p; p++) {
	    if (*p == '\n') {
		*p = '\0';
		break;
	    }
	}
	sts = stat(path, &sbuf);

	if (sts < 0) {
	    if (verbose)
		fprintf(stderr, "%s: stat() failed: %s\n", path, pmErrStr(-oserror()));
	    continue;
	}

	switch (pred) {
	    case PRED_CTIME:
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
		    check.tv_sec = sbuf.st_ctime;
		    check.tv_usec = 0;
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
		    check.tv_sec = sbuf.st_ctimespec.tv_sec;
		    check.tv_usec = sbuf.st_ctimespec.tv_nsec / 1000;
#elif defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESPEC_T)
		    check.tv_sec = sbuf.st_ctim.tv_sec;
		    check.tv_usec = sbuf.st_ctim.tv_nsec / 1000;
#else
!bozo!
#endif
		    break;

	    case PRED_MTIME:
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
		    check.tv_sec = sbuf.st_mtime;
		    check.tv_usec = 0;
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
		    check.tv_sec = sbuf.st_mtimespec.tv_sec;
		    check.tv_usec = sbuf.st_mtimespec.tv_nsec / 1000;
#elif defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESPEC_T)
		    check.tv_sec = sbuf.st_mtim.tv_sec;
		    check.tv_usec = sbuf.st_mtim.tv_nsec / 1000;
#else
!bozo!
#endif
		    break;

	    }

	check_f = pmtimevalToReal(&check);
	if (verbose)
	    fprintf(stderr, "%s: me: %.6f (now-%.6f) timespec: %.6f\n", path, check_f, now_f-check_f, stamp_f);
	if ((sign == PLUS_TIME && check_f < stamp_f) ||
	    (sign == MINUS_TIME && check_f >= stamp_f))
	    printf("%s\n", path);
    }

    return 0;
}
