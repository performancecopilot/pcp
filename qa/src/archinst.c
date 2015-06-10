/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <unistd.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
dometric(char *name)
{
    int		i;
    pmID	pmidlist[] = { PM_ID_NULL };
    pmDesc	desc;
    int		sts;
    char	*iname;
    int		*ilist;
    char	**nlist;

    if ((sts = pmLookupName(1, &name, pmidlist)) < 0)
	return sts;

    if ((sts = pmLookupDesc(pmidlist[0], &desc)) < 0)
	return sts;

    for (i = 0; i < 2; i++) {
	iname = "no match";
	printf("pm*InDom: inst=%d", i);
	if ((sts = pmNameInDom(desc.indom, i, &iname)) < 0)
	    printf(" %s\n", pmErrStr(sts));
	else {
	    printf(" iname=%s reverse lookup:", iname);
	    if ((sts = pmLookupInDom(desc.indom, iname)) < 0)
		printf(" %s\n", pmErrStr(sts));
	    else
		printf(" inst=%d\n", sts);
	}
	iname = "no match";
	printf("pm*InDomArchive: inst=%d", i);
	if ((sts = pmNameInDomArchive(desc.indom, i, &iname)) < 0)
	    printf(" %s\n", pmErrStr(sts));
	else {
	    printf(" iname=%s reverse lookup:", iname);
	    if ((sts = pmLookupInDomArchive(desc.indom, iname)) < 0)
		printf(" %s\n", pmErrStr(sts));
	    else
		printf(" inst=%d\n", sts);
	}
    }

    if ((sts = pmGetInDom(desc.indom, &ilist, &nlist)) < 0)
	printf("pmGetInDom: %s\n", pmErrStr(sts));
    else {
	printf("pmGetInDom:\n");
	for (i = 0; i < sts; i++) {
	    printf("   [%d] %s\n", ilist[i], nlist[i]);
	}
	free(ilist);
	free(nlist);
    }

    if ((sts = pmGetInDomArchive(desc.indom, &ilist, &nlist)) < 0)
	printf("pmGetInDomArchive: %s\n", pmErrStr(sts));
    else {
	printf("pmGetInDomArchive:\n");
	for (i = 0; i < sts; i++) {
	    printf("   [%d] %s\n", ilist[i], nlist[i]);
	}
	free(ilist);
	free(nlist);
    }
    return sts;
}

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
    pmLogLabel	label;				/* get hostname for archives */
    int		zflag = 0;			/* for -z */
    char 	*tz = (char *)0;		/* for -Z timezone */
    int		tzh;				/* initial timezone handle */
    char	local[MAXHOSTNAMELEN];
    char	*namespace = PM_NS_DEFAULT;
    int		samples = -1;
    double	delta = 1.0;
    char	*endnum;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:c:D:fh:l:n:s:t:VzZ:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

	case 'c':	/* configfile */
	    if (configfile != (char *)0) {
		fprintf(stderr, "%s: at most one -c option allowed\n", pmProgname);
		errflag++;
	    }
	    configfile = optarg;
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

	case 'f':	/* force */
	    force++; 
	    break;	

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
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

	case 'V':	/* verbose */
	    verbose++;
	    break;

	case 'z':	/* timezone from host */
	    if (tz != (char *)0) {
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
	fprintf(stderr, "%s: -z requires an explicit -a or -h option\n", pmProgname);
	errflag++;
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s options ...\n\
\n\
Options\n\
  -a   archive	  metrics source is an archive log\n\
  -c   configfile file to load configuration from\n\
  -D   debug	  standard PCP debug flag\n\
  -f		  force .. \n\
  -h   host	  metrics source is PMCD on host\n\
  -l   logfile	  redirect diagnostics and trace output\n\
  -n   namespace  use an alternative PMNS\n\
  -s   samples	  terminate after this many iterations\n\
  -t   delta	  sample interval in seconds(float) [default 1.0]\n\
  -V 	          verbose/diagnostic output\n\
  -z              set reporting timezone to local time for host from -a or -h\n\
  -Z   timezone   set reporting timezone\n",
		pmProgname);
	exit(1);
    }

    if (logfile != (char *)0) {
	__pmOpenLog(pmProgname, logfile, stderr, &sts);
	if (sts < 0) {
	    fprintf(stderr, "%s: Could not open logfile \"%s\"\n", pmProgname, logfile);
	}
    }

    if (namespace != PM_NS_DEFAULT) {
	if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	    printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	    exit(1);
	}
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
    else if (tz != (char *)0) {
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
    while (optind < argc) {
	printf("%s:\n", argv[optind]);
	sts = dometric(argv[optind]);
	if (sts < 0)
	    printf("Error: %s\n", pmErrStr(sts));
	optind++;
    }

    exit(0);
}
