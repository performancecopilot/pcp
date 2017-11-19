/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2015 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		type = 0;
    char	*archive = NULL;		/* pander to gcc */
    int		mode = PM_MODE_FORW;		/* mode for archives */
    pmLogLabel	label;				/* get hostname for archives */
    int		zflag = 0;			/* for -z */
    char 	*tz = NULL;			/* for -Z timezone */
    int		tzh;				/* initial timezone handle */
    int		samples = -1;
    int		save_samples;
    char	*endnum;
    int		numpmid = 0;
    pmID	*pmidlist = NULL;
    pmResult	*rp;
    char	timebuf[26];

    /* trim cmd name of leading directory components */
    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:O:s:zZ:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
#ifdef BUILD_STANDALONE
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmGetProgname());
#else
		fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmGetProgname());
#endif
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    archive = optarg;
	    break;

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 's':	/* sample count */
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0) {
		fprintf(stderr, "%s: -s requires numeric argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 'z':	/* timezone from archive */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmGetProgname());
		errflag++;
	    }
	    zflag++;
	    break;

	case 'Z':	/* $TZ timezone */
	    if (zflag) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmGetProgname());
		errflag++;
	    }
	    tz = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options] metric ...\n\
\n\
Options:\n\
  -a archive     metrics source is a PCP log archive\n\
  -s samples     terminate after this many samples\n\
  -z             set reporting timezone to local time of metrics source\n\
  -Z timezone    set reporting timezone\n",
                pmGetProgname());
        exit(1);
    }

    if (type == 0) {
	fprintf(stderr, "%s: -a is not optional\n", pmGetProgname());
	exit(1);
    }

    if ((sts = pmNewContext(type, archive)) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmGetProgname(), archive, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmGetArchiveLabel(&label)) < 0) {
	fprintf(stderr, "%s: Cannot get archive label record: %s\n",
	    pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmSetMode(mode, &label.ll_start, 0)) < 0) {
	fprintf(stderr, "%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    if (zflag) {
	if ((tzh = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmGetProgname(), pmErrStr(tzh));
	    exit(1);
	}
	printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n",
	    label.ll_hostname);
    }
    else if (tz != NULL) {
	if ((tzh = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		pmGetProgname(), tz, pmErrStr(tzh));
	    exit(1);
	}
	printf("Note: timezone set to \"TZ=%s\"\n\n", tz);
    }

    while (optind < argc) {
	pmidlist = (pmID *)realloc(pmidlist, (numpmid+1)*sizeof(pmidlist[0]));
	if (pmidlist == NULL) {
	    pmNoMem("pmidlist[]", (numpmid+1)*sizeof(pmidlist[0]), PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	sts = pmLookupName(1, &argv[optind], &pmidlist[numpmid]);
	if (sts < 0) {
	    fprintf(stderr, "%s: pmLookupName(..., %s, ...): %s\n", pmGetProgname(), argv[optind], pmErrStr(sts));
	    exit(1);
	}
	numpmid++;
	optind++;
    }

    printf("pmFetch ...\n");
    save_samples = samples;
    while (samples == -1 || samples-- > 0) {
	sts = pmFetch(numpmid, pmidlist, &rp);
	if (sts >= 0) {
	    pmCtime(&rp->timestamp.tv_sec, timebuf);
	    printf("numpmid=%2d %.19s.%08d\n", rp->numpmid, timebuf, (int)rp->timestamp.tv_usec);
	    pmFreeResult(rp);
	}
	else {
	    printf("-> %s\n", pmErrStr(sts));
	    break;
	}
    }

    if ((sts = pmSetMode(mode, &label.ll_start, 0)) < 0) {
	fprintf(stderr, "%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    printf("\npmFetchArchive ...\n");
    samples = save_samples;
    while (samples == -1 || samples-- > 0) {
	sts = pmFetchArchive(&rp);
	if (sts >= 0) {
	    pmCtime(&rp->timestamp.tv_sec, timebuf);
	    printf("numpmid=%2d %.19s.%08d", rp->numpmid, timebuf, (int)rp->timestamp.tv_usec);
	    if (rp->numpmid == 0)
		printf(" <mark>");
	    putchar('\n');
	    pmFreeResult(rp);
	}
	else {
	    printf("-> %s\n", pmErrStr(sts));
	    break;
	}
    }

    return 0;
}
