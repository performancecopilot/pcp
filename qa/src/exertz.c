/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		type = 0;
    int		force = 0;
    int 	verbose = 0;
    char	*host = NULL;			/* pander to gcc */
    char 	*configfile = (char *)0;
    char 	*logfile = (char *)0;
    char 	*tz = (char *)0;
    char 	*tzhost = (char *)0;
    int		tzh = -1;			/* pander to gcc */
    char	local[MAXHOSTNAMELEN];
    char	*namespace = PM_NS_DEFAULT;
    int		samples = -1;
    double	delta = 1.0;
    char	*endnum;
    time_t	now;
    int		tzh2;
    char	s[28];

    pmSetProgname(argv[0]);
    setlinebuf(stdout);

    now = 24 * 60 * 60;		/* epoch + 1 day */
    putenv("TZ=UTC");		/* sane starting point */

    printf("UTC pmCtime(): %s", pmCtime(&now, s));
    printf("UTC ctime(): %s", ctime(&now));

    while ((c = getopt(argc, argv, "a:c:D:f:h:ln:s:t:Vz:Z:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmGetProgname());
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

	case 'c':	/* configfile */
	    if (configfile != (char *)0) {
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

	case 'f':	/* force */
	    force++; 
	    break;	

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmGetProgname());
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'l':	/* logfile */
	    logfile = optarg;
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case 's':	/* sample count */
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples <= 0.0) {
		fprintf(stderr, "%s: -s requires numeric argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 't':	/* delta seconds (double) */
	    delta = strtod(optarg, &endnum);
	    if (*endnum != '\0' || delta <= 0.0) {
		fprintf(stderr, "%s: -t requires floating point argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 'V':	/* verbose */
	    verbose++;
	    break;

	case 'z':	/* timezone from host */
	    if (tz != (char *)0) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmGetProgname());
		errflag++;
	    }
	    tzhost = optarg;
	    break;

	case 'Z':	/* $TZ timezone */
	    if (tzhost != (char *)0) {
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
"Usage: %s options ...\n\
\n\
Options\n\
  -a   archive	  metrics source is an archive\n\
  -c   configfile file to load configuration from\n\
  -D   debugspec  standard PCP debugging options\n\
  -f		  force .. \n\
  -h   host	  metrics source is PMCD on host\n\
  -l   logfile	  redirect diagnostics and trace output\n\
  -n   namespace  use an alternative PMNS\n\
  -s   samples	  terminate after this many iterations\n\
  -t   delta	  sample interval in seconds(float) [default 1.0]\n\
  -V 	          verbose/diagnostic output\n\
  -z   host       set reporting timezone to local time for host\n\
  -Z   timezone   set reporting timezone\n",
		pmGetProgname());
	exit(1);
    }

    if (logfile != (char *)0) {
	pmOpenLog(pmGetProgname(), logfile, stderr, &sts);
	if (sts != 1) {
	    fprintf(stderr, "%s: Could not open logfile \"%s\"\n", pmGetProgname(), logfile);
	}
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), namespace, pmErrStr(sts));
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
		pmGetProgname(), host, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmGetProgname(), host, pmErrStr(sts));
	exit(1);
    }
    if (tzhost == (char *)0 && tz == (char *)0)
	tzh = pmNewContextZone();

    if (tzhost != (char *)0) {
	if (type == PM_CONTEXT_ARCHIVE) {
	    pmHighResLogLabel	label;
	    if ((sts = pmGetHighResArchiveLabel(&label)) < 0) {
		fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		    pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }
	    if (strcmp(tzhost, label.hostname) != 0) {
		fprintf(stderr, "%s: mismatched host name between -z (%s) and archive (%s)\n",
		    pmGetProgname(), tzhost, label.hostname);
		exit(1);
	    }
	}
	else if (strcmp(tzhost, host) != 0) {
	    fprintf(stderr, "%s: -z and -h must agree on the name of the host\n", pmGetProgname());
	    exit(1);
	}
	if ((tzh = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmGetProgname(), pmErrStr(tzh));
	    exit(1);
	}
    }

    if (tz != (char *)0) {
	if ((tzh = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		pmGetProgname(), tz, pmErrStr(tzh));
	    exit(1);
	}
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    while (optind < argc) {
	printf("extra argument[%d]: %s\n", optind, argv[optind]);
	optind++;
    }

    while (samples == -1 || samples-- > 0) {
	/* put real stuff here */
	break;
    }

    printf("PMAPI context pmCtime(): %s", pmCtime(&now, s));
    printf("UTC ctime(): %s", ctime(&now));

    tzh2 = pmNewZone("PST8PDT");
    printf("and in California: %s", pmCtime(&now, s));
    printf("UTC ctime(): %s", ctime(&now));
    pmNewZone("EST-11EST-10,86/2:00,303/2:00");
    printf("and in Melbourne: %s", pmCtime(&now, s));
    printf("UTC ctime(): %s", ctime(&now));
    pmUseZone(tzh2);
    printf("back to California: %s", pmCtime(&now, s));
    printf("UTC ctime(): %s", ctime(&now));

    pmUseZone(tzh);
    printf("back to the PMAPI context pmCtime(): %s", pmCtime(&now, s));
    printf("UTC ctime(): %s", ctime(&now));

    exit(0);
}
