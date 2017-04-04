/*
 * Copyright (c) 2005 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * torture pmGetArchiveEnd
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

#ifdef IS_MINGW
int
truncate(const char *fname, off_t offset)
{
    int fd;

    if ((fd = open(fname, O_WRONLY)) < 0)
	return -1;
    if (ftruncate(fd, offset) < 0)
	return -1;
    close(fd);
    return 0;
}
#endif

static void
printstamp(struct timeval *tp)
{
    static struct tm	*tmp;

    tmp = localtime(&tp->tv_sec);
    printf("%02d:%02d:%02d.%03d", (int)tmp->tm_hour, (int)tmp->tm_min, (int)tmp->tm_sec, (int)(tp->tv_usec/1000));
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		e_sts = 0;
    int		errflag = 0;
    int		ahtype = 0;
    int		verbose = 0;
    int		quick = 0;
    char	*host = NULL;			/* pander to gcc */
    pmResult	*result;
    pmResult	*prev = NULL;
    struct timeval	start = { 0,0 };
    struct timeval	end;
    int		tzh;
    off_t	trunc_size = 0;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:t:qv?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (ahtype != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
		errflag++;
	    }
	    ahtype = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

#ifdef PCP_DEBUG

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;
#endif

	case 't':	/* truncate */
	    trunc_size = atol(optarg);
	    break;

	case 'q':	/* quick */
	    quick = 1;
	    break;

	case 'v':	/* verbose */
	    verbose++;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind < argc) {
	fprintf(stderr,
"Usage: %s options ...\n\
\n\
Options\n\
  -a   archive	  metrics source is an archive log\n\
  -t   size       truncate archive to size bytes\n\
  -q              quick (read last 3 records, not the whole archive)\n\
  -v              verbose\n",
		pmProgname);
	exit(1);
    }

    if (ahtype != PM_CONTEXT_ARCHIVE) {
	fprintf(stderr, "%s: -a is not optional!\n", pmProgname);
	exit(1);
    }

    /* truncate if -t specified, _before_ opening archive */
    if (trunc_size != 0) {
	if (access(host, W_OK) == 0) {
	    if (truncate(host, trunc_size) != 0) {
		fprintf(stderr, "%s: file %s exists, but cannot truncate\n",
		    pmProgname, host);
		exit(1);
	    }
	}
	else {
	    char	path[MAXPATHLEN];

	    sprintf(path, "%s.0", host);
	    if (access(path, W_OK) == 0) {
		if (truncate(path, trunc_size) != 0) {
		    fprintf(stderr, "%s: file %s exists, but cannot truncate\n",
			pmProgname, path);
		    exit(1);
		}
	    }
	    else {
		fprintf(stderr, "%s: cannot find writeable %s or %s\n",
			pmProgname, host, path);
		exit(1);
	    }
	}
    }

    if ((sts = pmNewContext(ahtype, host)) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
	    pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    /* force -z (timezone of archive */
    if ((tzh = pmNewContextZone()) < 0) {
	fprintf(stderr, "%s: Cannot set context timezone: %s\n",
	    pmProgname, pmErrStr(tzh));
	exit(1);
    }

    sts = pmGetArchiveEnd(&end);
    if (sts < 0) {
	printf("pmGetArchiveEnd: %s\n", pmErrStr(sts));
    }
    else {
	if (verbose) {
	    printf("pmGetArchiveEnd time: ");
	    printstamp(&end);
	    printf("\n");
	}
    }

    sts = pmSetMode(PM_MODE_BACK, &end, 0);
    if (sts < 0) {
	printf("pmSetMode PM_MODE_BACK: %s\n", pmErrStr(sts));
	exit(1);
    }
    sts = pmFetchArchive(&result);
    if (sts < 0) {
	printf("pmFetchArchive: %s\n", pmErrStr(sts));
	e_sts = 1;
    }
    else {
	if (verbose) {
	    printf("last result time (direct): ");
	    printstamp(&result->timestamp);
	    printf("\n");
	}
	if (result->timestamp.tv_sec != end.tv_sec ||
	    result->timestamp.tv_usec != end.tv_usec) {
	    printf("Mismatch: end=");
	    printstamp(&end);
	    printf(" direct=");
	    printstamp(&result->timestamp);
	    printf("\n");
	    e_sts = 1;
	}
	start.tv_sec = result->timestamp.tv_sec;
	start.tv_usec = result->timestamp.tv_usec;
	pmFreeResult(result);
    }

    if (quick && e_sts == 0) {
	int	i;
	for (i = 0; i < 2; i++) {
	    sts = pmFetchArchive(&result);
	    if (sts >= 0) {
		start.tv_sec = result->timestamp.tv_sec;
		start.tv_usec = result->timestamp.tv_usec;
		pmFreeResult(result);
	    }
	}
    }
    else {
	/* start from the epoch and move forward */
	start.tv_sec = 0;
	start.tv_usec = 0;
    }
    sts = pmSetMode(PM_MODE_FORW, &start, 0);
    if (sts < 0) {
	printf("pmSetMode PM_MODE_FORW: %s\n", pmErrStr(sts));
	exit(1);
    }
    while ((sts = pmFetchArchive(&result)) >= 0) {
	if (prev != NULL)
	    pmFreeResult(prev);
	prev = result;
    }
    if (verbose) printf("pmFetchArchive: %s\n", pmErrStr(sts));
    if (prev == NULL) {
	printf("no results!\n");
    }
    else {
	if (verbose) {
	    printf("last result time (serial): ");
	    printstamp(&prev->timestamp);
	    printf("\n");
	}
	if (prev->timestamp.tv_sec != end.tv_sec ||
	    prev->timestamp.tv_usec != end.tv_usec) {
	    printf("Mismatch: end=");
	    printstamp(&end);
	    printf(" serial=");
	    printstamp(&prev->timestamp);
	    printf("\n");
	    e_sts = 1;
	}
	pmFreeResult(prev);
    }

    exit(e_sts);
}
