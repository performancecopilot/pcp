/*
 * chk_metric_types - check pmResult value types match pmDesc.
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <unistd.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

static void checkMetric(const char *);

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		type = 0;
    int 	verbose = 0;
    char	*host = NULL;			/* pander to gcc */
    int		mode = PM_MODE_INTERP;		/* mode for archives */
    char 	*configfile = NULL;
    char 	*logfile = NULL;
    pmLogLabel	label;				/* get hostname for archives */
    int		zflag = 0;			/* for -z */
    char 	*tz = NULL;			/* for -Z timezone */
    int		tzh;				/* initial timezone handle */
    char	local[MAXHOSTNAMELEN];
    char	*pmnsfile = PM_NS_DEFAULT;
    int		samples = -1;
    double	delta = 1.0;
    char	*endnum;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:c:D:h:l:Ln:s:t:U:VzZ:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h, -L and -U allowed\n", pmProgname);
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
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

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h, -L and -U allowed\n", pmProgname);
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

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

	case 'l':	/* logfile */
	    logfile = optarg;
	    break;

	case 'n':	/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 's':	/* sample count */
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0) {
		fprintf(stderr, "%s: -s requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case 't':	/* delta seconds (double) */
	    delta = strtod(optarg, &endnum);
	    if (*endnum != '\0' || delta <= 0.0) {
		fprintf(stderr, "%s: -t requires floating point argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case 'U':	/* uninterpolated archive log */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h, -L and -U allowed\n", pmProgname);
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    mode = PM_MODE_FORW;
	    host = optarg;
	    break;

	case 'V':	/* verbose */
	    verbose++;
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
"Usage: %s options ...\n\
\n\
Options\n\
  -a archive	metrics source is an archive log\n\
  -c configfile file to load configuration from\n\
  -D debug	standard PCP debug flag\n\
  -h host	metrics source is PMCD on host\n\
  -l logfile	redirect diagnostics and trace output\n\
  -L            use local context instead of PMCD\n\
  -n pmnsfile   use an alternative PMNS\n\
  -s samples	terminate after this many iterations\n\
  -t delta	sample interval in seconds(float) [default 1.0]\n\
  -U archive	metrics source is an uninterpolated archive log\n\
  -V            verbose/diagnostic output\n\
  -z            set reporting timezone to local time for host from -a, -h or -U\n\
  -Z timezone   set reporting timezone\n",
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
	gethostname(local, sizeof(local));
	host = local;
    }
    if ((sts = pmNewContext(type, host)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    if (type == PM_CONTEXT_ARCHIVE) {
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(1);
	}
	if (mode != PM_MODE_INTERP) {
	    if ((sts = pmSetMode(mode, &label.ll_start, 0)) < 0) {
		fprintf(stderr, "%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
		exit(1);
	    }
	}
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
    else
	tzh = pmNewContextZone();

    /* non-flag args are argv[optind] ... argv[argc-1] */
    if (optind == argc) {
	if ((sts = pmTraversePMNS(NULL, checkMetric)) < 0) {
	    fprintf(stderr, "%s: pmTraversePMNS: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
    }
    else
    while (optind < argc) {
	if ((sts = pmTraversePMNS(argv[optind], checkMetric)) < 0) {
	    fprintf(stderr, "%s: pmTraversePMNS: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
	optind++;
    }

    return 0;
}

void
checkMetric(const char *metric)
{
    int sts;
    int i;
    char *nameList[] = { NULL };
    pmID pmidList[] = { PM_IN_NULL };
    pmDesc desc;
    pmResult *result;

    /* pmLookupName will not modify this string */
    nameList[0] = (char *)metric;

    if ((sts = pmLookupName(1, nameList, pmidList)) < 0) {
	fprintf(stderr, "%s: pmLookupName: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmLookupDesc(pmidList[0], &desc)) < 0) {
	fprintf(stderr, "%s: pmLookupDesc: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    if (desc.type == PM_TYPE_NOSUPPORT)
	return;
    
    if ((sts = pmFetch(1, pmidList, &result)) < 0) {
	fprintf(stderr, "%s: pmfetch: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    if (result->numpmid > 0) {
	for (i=0; i < result->vset[0]->numval; i++) {
	    if (result->vset[0]->valfmt != PM_VAL_INSITU) {
		pmValue *val = &result->vset[0]->vlist[i];
		if (val->value.pval->vtype == PM_TYPE_UNKNOWN) {
		    printf("metric \"%s\" vtype for instance %d is PM_TYPE_UNKNOWN\n",
			metric, val->inst);
		}
		else
		if (val->value.pval->vtype != desc.type) {
		    printf("metric \"%s\" vtype for instance %d, type (%s)",
			metric, val->inst, pmTypeStr(val->value.pval->vtype));
		    printf(" does not match descriptor (%s)\n", pmTypeStr(desc.type));

		}
	    }
	}
    }


    pmFreeResult(result);
}
