/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2014 Ken McDonell.  All Rights Reserved.
 *
 * Churn a context ... looking for memory leaks a la
 * http://oss.sgi.com/bugzilla/show_bug.cgi?id=1057
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

static int	nmetric;		/* for metric ... args */
static char	**name = NULL;
static pmID	*pmid = NULL;
static pmDesc	*desc = NULL;

static int	ninst;			/* for -i */
static char	**instname = NULL;
static int	*inst = NULL;
static pmInDom	indom;

void
dometric(const char *new_name)
{
    int		sts;

    nmetric++;
    if ((name = (char **)realloc(name, nmetric*sizeof(name[0]))) == NULL) {
	fprintf(stderr, "name[%d] malloc failed @ %s\n", nmetric-1, new_name);
	exit(1);
    }
    name[nmetric-1] = strdup(new_name);
    if ((pmid = (pmID *)realloc(pmid, nmetric*sizeof(pmid[0]))) == NULL) {
	fprintf(stderr, "pmid[%d] malloc failed @ %s\n", nmetric-1, new_name);
	exit(1);
    }
    if ((desc = (pmDesc *)realloc(desc, nmetric*sizeof(desc[0]))) == NULL) {
	fprintf(stderr, "desc[%d] malloc failed @ %s\n", nmetric-1, new_name);
	exit(1);
    }
    if ((sts = pmLookupName(1, &name[nmetric-1], &pmid[nmetric-1])) < 0) {
	fprintf(stderr, "Warning: pmLookupName(\"%s\",...) failed: %s\n", name[nmetric-1], pmErrStr(sts));
    }
    if ((sts = pmLookupDesc(pmid[nmetric-1], &desc[nmetric-1])) < 0) {
	fprintf(stderr, "Warning: pmLookupDesc(\"%s\",...) failed: %s\n", name[nmetric-1], pmErrStr(sts));
    }
    if (desc[nmetric-1].indom != PM_INDOM_NULL && ninst > 0 && inst == NULL) {
	/*
	 * first time through for a metric with an instance domain ...
	 * try to map the external instance names from -i to internal
	 * instance ids
	 */
	int	j;
	indom = desc[nmetric-1].indom;
	inst = (int *)malloc(ninst*sizeof(inst[0]));
	if (instname == NULL) {
	    pmNoMem("inst", ninst*sizeof(inst[0]), PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	for (j = 0; j < ninst; j++) {
	    inst[j] = pmLookupInDom(indom, instname[j]);
	    if (inst[j] < 0) {
		fprintf(stderr, "Warning: pmLookupInDom(%s, \"%s\") [j=%d] failed for metric %s: %s\n", pmInDomStr(indom), instname[j], j, new_name, pmErrStr(inst[j]));
		inst[j] = PM_IN_NULL;
	    }
	}
    }
}

int
main(int argc, char **argv)
{
    int		c;
    int		i;
    int		sts;
    int		ctx;
    int		dupctx = 0;
    int		iter;
    int		errflag = 0;
    int		type = 0;
    int		vflag = 0;
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
    int		fetch_samples = 1;
    char	*endnum;
    pmResult	*rp;
    char	*highwater = NULL;
    char	*q;
    struct timeval delta = { 15, 0 };
    struct timeval startTime;
    struct timeval endTime;
    struct timeval appStart;
    struct timeval appEnd;
    struct timeval appOffset;

    /* trim cmd name of leading directory components */
    pmSetProgname(argv[0]);

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    while ((c = getopt(argc, argv, "a:A:c:D:df:h:i:l:Ln:O:s:S:t:T:U:vzZ:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmGetProgname());
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
		fprintf(stderr, "%s: at most one -c option allowed\n", pmGetProgname());
		errflag++;
	    }
	    configfile = optarg;
	    break;	

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'd':	/* dup context rather than new context */
	    dupctx = 1;
	    break;

	case 'f':	/* pmFetch sample count */
	    fetch_samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || fetch_samples < 0) {
		fprintf(stderr, "%s: -f requires numeric argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmGetProgname());
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'i':	/* list of external instance names */
	    q = optarg;
	    while ((q = index(optarg, ',')) != NULL) {
		ninst++;
		instname = (char **)realloc(instname, (ninst+1)*sizeof(instname[0]));
		if (instname == NULL) {
		    pmNoMem("instname", (ninst+1)*sizeof(instname[0]), PM_FATAL_ERR);
		    /* NOTREACHED */
		}
		*q = '\0';
		instname[ninst-1] = strdup(optarg);
		optarg = &q[1];
	    }
	    ninst++;
	    if (instname == NULL) {
		pmNoMem("instname", ninst*sizeof(instname[0]), PM_FATAL_ERR);
		/* NOTREACHED */
	    }
	    instname[ninst-1] = strdup(optarg);
	    break;

	case 'L':	/* LOCAL, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h, -L and -U allowed\n", pmGetProgname());
		errflag++;
	    }
	    host = NULL;
	    type = PM_CONTEXT_LOCAL;
	    putenv("PMDA_LOCAL_PROC=");		/* if proc PMDA needed */
	    putenv("PMDA_LOCAL_SAMPLE=");	/* if sampledso PMDA needed */
	    break;

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
		fprintf(stderr, "%s: -s requires numeric argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 'S':	/* start time */
	    start = optarg;
	    break;

	case 't':	/* change update interval */
	    if (pmParseInterval(optarg, &delta, &endnum) < 0) {
		fprintf(stderr, "%s: illegal -t argument\n", pmGetProgname());
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
		fprintf(stderr, "%s: at most one of -a, -h, -L and -U allowed\n", pmGetProgname());
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    mode = PM_MODE_FORW;
	    host = optarg;
	    break;

	case 'v':	/* report values */
	    vflag = 1;
	    break;

	case 'z':	/* timezone from host */
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

    if (zflag && type == 0) {
	fprintf(stderr, "%s: -z requires an explicit -a, -h or -U option\n", pmGetProgname());
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
  -f samples     pmFetch samples before churning contexts [default 1]\n\
  -h host        metrics source is PMCD on host\n\
  -i instid[,instid...]\n\
                 fetch some instances (1,2,...,1,2,...) each iteration\n\
  -l logfile     redirect diagnostics and trace output\n\
  -L             use local context instead of PMCD\n\
  -n pmnsfile    use an alternative PMNS\n\
  -O offset      initial offset into the time window\n\
  -s samples     terminate after this many iterations [default 1]\n\
  -S starttime   start of the time window\n\
  -t interval    sample interval [default 15.0 seconds]\n\
  -T endtime     end of the time window\n\
  -v             report values\n\
  -z             set reporting timezone to local time of metrics source\n\
  -Z timezone    set reporting timezone\n",
                pmGetProgname());
        exit(1);
    }

    if (logfile != NULL) {
	pmOpenLog(pmGetProgname(), logfile, stderr, &sts);
	if (sts != 1) {
	    fprintf(stderr, "%s: Could not open logfile \"%s\"\n", pmGetProgname(), logfile);
	}
    }

    if (pmnsfile != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(pmnsfile, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), 
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
		pmGetProgname(), host, pmErrStr(ctx));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmGetProgname(), host, pmErrStr(ctx));
	exit(1);
    }

    if (type == PM_CONTEXT_ARCHIVE) {
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}
  	startTime = label.ll_start;
	if ((sts = pmGetArchiveEnd(&endTime)) < 0) {
	    endTime.tv_sec = INT_MAX;
	    endTime.tv_usec = 0;
	    fflush(stdout);
	    fprintf(stderr, "%s: Cannot locate end of archive: %s\n",
		pmGetProgname(), pmErrStr(sts));
	    fprintf(stderr, "\nWARNING: This archive is sufficiently damaged that it may not be possible to\n");
	    fprintf(stderr, "         produce complete information.  Continuing and hoping for the best.\n\n");
	    fflush(stderr);
	}
    }
    else {
	gettimeofday(&startTime, NULL);
	endTime.tv_sec = INT_MAX;
	endTime.tv_usec = 0;
    }

    if (zflag) {
	if ((tzh = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmGetProgname(), pmErrStr(tzh));
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
		pmGetProgname(), tz, pmErrStr(tzh));
	    exit(1);
	}
	printf("Note: timezone set to \"TZ=%s\"\n\n", tz);
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    nmetric = 0;
    while (optind < argc) {
	pmTraversePMNS(argv[optind], dometric);
	optind++;
    }

    if (align != NULL && type != PM_CONTEXT_ARCHIVE) {
	fprintf(stderr, "%s: -A option only supported for PCP archives, alignment request ignored\n",
		pmGetProgname());
	align = NULL;
    }

    sts = pmParseTimeWindow(start, finish, align, offset, &startTime,
			    &endTime, &appStart, &appEnd, &appOffset,
			    &endnum);
    if (sts < 0) {
	fprintf(stderr, "%s: illegal time window specification\n%s", pmGetProgname(), endnum);
	exit(1);
    }

    iter = 1;
    while (samples == -1 || samples-- > 0) {
	int	new_ctx;
	char	*check;

	if (iter == 1 || (iter % fetch_samples) == 0) {
	    if (dupctx) {
		if ((new_ctx = pmDupContext()) < 0) {
		    fprintf(stderr, "%s: pmDupContext failed: %s\n", pmGetProgname(), pmErrStr(new_ctx));
		    exit(1);
		}
	    }
	    else {
		if ((new_ctx = pmNewContext(type, host)) < 0) {
		    fprintf(stderr, "%s: pmNewContext failed: %s\n", pmGetProgname(), pmErrStr(new_ctx));
		    exit(1);
		}

	    }

	    if ((sts = pmDestroyContext(ctx)) < 0) {
		fprintf(stderr, "%s: pmDestroyContex failed: %s\n", pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }

	    if ((sts = pmUseContext(new_ctx)) < 0) {
		fprintf(stderr, "%s: pmUseContext(%d) failed: %s\n", pmGetProgname(), new_ctx, pmErrStr(sts));
		exit(1);
	    }
	    ctx = new_ctx;
	}

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
		    fprintf(stderr, "%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
		    exit(1);
		}
	    }
	    pmtimevalInc(&appStart, &delta);
	}

	if (nmetric > 0) {
	    if (ninst > 0) {
		if ((sts = pmAddProfile(indom, ((iter-1) % ninst)+1, inst)) < 0) {
		    fprintf(stderr, "Warning: pmAddProfile(...,%d) failed: %s\n", (iter % ninst)+1, pmErrStr(sts));
		}
	    }
	    if ((sts = pmFetch(nmetric, pmid, &rp)) < 0) {
		fprintf(stderr, "%s: pmFetch failed: %s\n", pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }
	    else {
		if (delta.tv_sec > 0 || delta.tv_usec > 0) {
		    char	now[26];
		    time_t	stamp;
		    stamp = rp->timestamp.tv_sec;
		    printf("%s", pmCtime(&stamp, now));
		}
		for (i = 0; i < nmetric; i++) {
		    printf("%s (%s):", name[i], pmIDStr(rp->vset[i]->pmid));
		    if (rp->vset[i]->numval < 0)
			printf(" Error: %s", pmErrStr(rp->vset[i]->numval));
		    else if (rp->vset[i]->numval == 0)
			printf(" No values");
		    else if (rp->vset[i]->numval == 1)
			printf(" 1 value");
		    else
			printf(" %d values", rp->vset[i]->numval);
		    if (vflag) {
			int	j;
			for (j = 0; j < rp->vset[i]->numval; j++) {
			    pmValue *vp = &rp->vset[i]->vlist[j];
			    putchar(' ');
			    pmPrintValue(stdout, rp->vset[i]->valfmt, desc[i].type, vp, 1);
			}
		    }
		    putchar('\n');
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
	fprintf(stderr, "%s: final pmDestroyContex failed: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    /* for valgrind */
    for (i = 0; i < nmetric; i++)
	free(name[i]);
    free(name);
    free(pmid);

    return 0;
}
