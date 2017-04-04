/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2014 Ken McDonell.  All Rights Reserved.
 *
 * Churn a context ... looking for memory leaks a la
 * http://oss.sgi.com/bugzilla/show_bug.cgi?id=1057
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

#define BUILD_STANDALONE 1

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		ctx;
    int		dupctx = 0;
    int		nmetric;
    int		iter;
    int		errflag = 0;
    int		type = 0;
    char	*host = NULL;			/* pander to gcc */
    int		mode = PM_MODE_INTERP;		/* mode for archives */
    char 	*configfile = NULL;
    char	*start = NULL;
    char	*finish = NULL;
    char	*align = NULL;
    char	*offset = NULL;
    char 	*logfile = NULL;
    pmLogLabel	label;				/* get hostname for archives */
    int		zflag = 0;			/* for -z */
    char 	*tz = NULL;			/* for -Z timezone */
    int		tzh;				/* initial timezone handle */
    char	local[MAXHOSTNAMELEN];
    char	*pmnsfile = PM_NS_DEFAULT;
    int		samples = 1;
    char	*endnum;
    char	**name = NULL;
    pmID	*pmid = NULL;
    pmResult	*rp;
    char	*highwater = NULL;
    struct timeval delta = { 15, 0 };
    struct timeval startTime;
    struct timeval endTime;
    struct timeval appStart;
    struct timeval appEnd;
    struct timeval appOffset;

    /* trim cmd name of leading directory components */
    __pmSetProgname(argv[0]);

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    while ((c = getopt(argc, argv, "a:A:c:D:dh:l:Ln:O:s:S:t:T:U:zZ:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
#ifdef BUILD_STANDALONE
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
#else
		fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmProgname);
#endif
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

	case 'A':	/* time alignment */
	    align = optarg;
	    break;

	case 'c':	/* configfile */
	    if (configfile != NULL) {
		fprintf(stderr, "%s: at most one -c option allowed\n", pmProgname);
		errflag++;
	    }
	    configfile = optarg;
	    break;	

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

	case 'd':	/* dup context rather than new context */
	    dupctx = 1;
	    break;

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
#ifdef BUILD_STANDALONE
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
#else
		fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmProgname);
#endif
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

#ifdef BUILD_STANDALONE
	case 'L':	/* LOCAL, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h, -L and -U allowed\n", pmProgname);
		errflag++;
	    }
	    host = NULL;
	    type = PM_CONTEXT_LOCAL;
	    putenv("PMDA_LOCAL_PROC=");		/* if proc PMDA needed */
	    putenv("PMDA_LOCAL_SAMPLE=");	/* if sampledso PMDA needed */
	    break;
#endif

	case 'l':	/* logfile */
	    logfile = optarg;
	    break;

	case 'n':	/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 'O':	/* sample offset time */
	    offset = optarg;
	    break;

	case 's':	/* sample count */
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0) {
		fprintf(stderr, "%s: -s requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case 'S':	/* start time */
	    start = optarg;
	    break;

	case 't':	/* change update interval */
	    if (pmParseInterval(optarg, &delta, &endnum) < 0) {
		fprintf(stderr, "%s: illegal -t argument\n", pmProgname);
		fputs(endnum, stderr);
		free(endnum);
		errflag++;
	    }
	    break;

	case 'T':	/* terminate time */
	    finish = optarg;
	    break;

	case 'U':	/* uninterpolated archive log */
	    if (type != 0) {
#ifdef BUILD_STANDALONE
		fprintf(stderr, "%s: at most one of -a, -h, -L and -U allowed\n", pmProgname);
#else
		fprintf(stderr, "%s: at most one of -a, -h and -U allowed\n", pmProgname);
#endif
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    mode = PM_MODE_FORW;
	    host = optarg;
	    break;

	case 'z':	/* timezone from host */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    zflag++;
	    break;

	case 'Z':	/* $TZ timezone */
	    if (zflag) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
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

    if (zflag && type == 0) {
	fprintf(stderr, "%s: -z requires an explicit -a, -h or -U option\n", pmProgname);
	errflag++;
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options] [metrics ...]\n\
\n\
Options:\n\
  -a archive     metrics source is a PCP log archive\n\
  -A align       align sample times on natural boundaries\n\
  -c configfile  file to load configuration from\n\
  -d             use pmDupContext [default: pmNewContext]\n\
  -h host        metrics source is PMCD on host\n\
  -l logfile     redirect diagnostics and trace output\n"
#ifdef BUILD_STANDALONE
"  -L             use local context instead of PMCD\n"
#endif
"  -n pmnsfile    use an alternative PMNS\n\
  -O offset      initial offset into the time window\n\
  -s samples     terminate after this many iterations [default 1]\n\
  -S starttime   start of the time window\n\
  -t interval    sample interval [default 15.0 seconds]\n\
  -T endtime     end of the time window\n\
  -z             set reporting timezone to local time of metrics source\n\
  -Z timezone    set reporting timezone\n",
                pmProgname);
        exit(1);
    }

    if (logfile != NULL) {
	__pmOpenLog(pmProgname, logfile, stderr, &sts);
	if (sts < 0) {
	    fprintf(stderr, "%s: Could not open logfile \"%s\"\n", pmProgname, logfile);
	}
    }

    if (pmnsfile != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(pmnsfile, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, 
	       pmnsfile, pmErrStr(sts));
	exit(1);
    }

    if (type == 0) {
	type = PM_CONTEXT_HOST;
	(void)gethostname(local, MAXHOSTNAMELEN);
	local[MAXHOSTNAMELEN-1] = '\0';
	host = local;
    }
    if ((ctx = pmNewContext(type, host)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmProgname, host, pmErrStr(ctx));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, host, pmErrStr(ctx));
	exit(1);
    }

    if (type == PM_CONTEXT_ARCHIVE) {
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(1);
	}
  	startTime = label.ll_start;
	if ((sts = pmGetArchiveEnd(&endTime)) < 0) {
	    endTime.tv_sec = INT_MAX;
	    endTime.tv_usec = 0;
	    fflush(stdout);
	    fprintf(stderr, "%s: Cannot locate end of archive: %s\n",
		pmProgname, pmErrStr(sts));
	    fprintf(stderr, "\nWARNING: This archive is sufficiently damaged that it may not be possible to\n");
	    fprintf(stderr, "         produce complete information.  Continuing and hoping for the best.\n\n");
	    fflush(stderr);
	}
    }
    else {
	gettimeofday(&startTime, NULL);
	endTime.tv_sec = INT_MAX;
    }

    if (zflag) {
	if ((tzh = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmProgname, pmErrStr(tzh));
	    exit(1);
	}
	if (type == PM_CONTEXT_ARCHIVE)
	    printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n",
		label.ll_hostname);
	else
	    printf("Note: timezone set to local timezone of host \"%s\"\n\n", host);
    }
    else if (tz != NULL) {
	if ((tzh = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		pmProgname, tz, pmErrStr(tzh));
	    exit(1);
	}
	printf("Note: timezone set to \"TZ=%s\"\n\n", tz);
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    nmetric = 0;
    while (optind < argc) {
	nmetric++;
	if ((name = (char **)realloc(name, nmetric*sizeof(name[0]))) == NULL) {
	    fprintf(stderr, "name[%d] malloc failed @ %s\n", nmetric-1, argv[optind]);
	    exit(1);
	}
	name[nmetric-1] = argv[optind];
	if ((pmid = (pmID *)realloc(pmid, nmetric*sizeof(pmid[0]))) == NULL) {
	    fprintf(stderr, "pmid[%d] malloc failed @ %s\n", nmetric-1, argv[optind]);
	    exit(1);
	}
	if ((sts = pmLookupName(1, &name[nmetric-1], &pmid[nmetric-1])) < 0) {
	    fprintf(stderr, "Warning: pmLookupName(\"%s\",...) failed: %s\n", name[nmetric-1], pmErrStr(sts));
	}
	optind++;
    }

    if (align != NULL && type != PM_CONTEXT_ARCHIVE) {
	fprintf(stderr, "%s: -A option only supported for PCP archives, alignment request ignored\n",
		pmProgname);
	align = NULL;
    }

    sts = pmParseTimeWindow(start, finish, align, offset, &startTime,
			    &endTime, &appStart, &appEnd, &appOffset,
			    &endnum);
    if (sts < 0) {
	fprintf(stderr, "%s: illegal time window specification\n%s", pmProgname, endnum);
	exit(1);
    }

    iter = 1;
    while (samples == -1 || samples-- > 0) {
	int	new_ctx;
	char	*check;

	if (dupctx) {
	    if ((new_ctx = pmDupContext()) < 0) {
		fprintf(stderr, "%s: pmDupContext failed: %s\n", pmProgname, pmErrStr(new_ctx));
		exit(1);
	    }
	}
	else {
	    if ((new_ctx = pmNewContext(type, host)) < 0) {
		fprintf(stderr, "%s: pmNewContext failed: %s\n", pmProgname, pmErrStr(new_ctx));
		exit(1);
	    }

	}

	if ((sts = pmDestroyContext(ctx)) < 0) {
	    fprintf(stderr, "%s: pmDestroyContex failed: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}

	if ((sts = pmUseContext(new_ctx)) < 0) {
	    fprintf(stderr, "%s: pmUseContext(%d) failed: %s\n", pmProgname, new_ctx, pmErrStr(sts));
	    exit(1);
	}
	ctx = new_ctx;
	/* dump out PDU buffer pool state */
	__pmFindPDUBuf(-1);
	/* check for outrageous memory leaks */
	check = (char *)sbrk(0);
	if (highwater != NULL) {
	    if (check - highwater > 512*1024) {
		/* use first 2 iterations to get stable */
		if (iter > 2)
		    printf("Memory growth (iteration %d): %ld\n", iter, (long)(check - highwater));
		highwater = check;
	    }
	}
	else
	    highwater = check;

	if (type == PM_CONTEXT_ARCHIVE) {
	    if (mode == PM_MODE_INTERP) {
		int		delta_msec;
		delta_msec = delta.tv_sec*1000 + delta.tv_usec/1000;
		if ((sts = pmSetMode(mode, &appStart, delta_msec)) < 0) {
		    fprintf(stderr, "%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
		    exit(1);
		}
	    }
	    __pmtimevalInc(&appStart, &delta);
	}

	if (nmetric > 0) {
	    if ((sts = pmFetch(nmetric, pmid, &rp)) < 0) {
		fprintf(stderr, "%s: pmFetch failed: %s\n", pmProgname, pmErrStr(sts));
		exit(1);
	    }
	    else {
		int		i;
		char	now[26];
		time_t	stamp;
		stamp = rp->timestamp.tv_sec;
		printf("%s", pmCtime(&stamp, now));
		for (i = 0; i < nmetric; i++) {
		    printf("%s: %d values\n", pmIDStr(rp->vset[i]->pmid), rp->vset[i]->numval);
		}
		pmFreeResult(rp);
	    }

	    if (type != PM_CONTEXT_ARCHIVE) {
		__pmtimevalSleep(delta);
	    }
	}
	else
	    printf("Nothing to be fetched\n");
	iter++;
    }

    if ((sts = pmDestroyContext(ctx)) < 0) {
	fprintf(stderr, "%s: final pmDestroyContex failed: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    return 0;
}
