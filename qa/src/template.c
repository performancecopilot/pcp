/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/archive.h>

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_ALIGN,	/* -A */
    PMOPT_ARCHIVE,	/* -a */
    PMOPT_DEBUG,	/* -D */
    PMOPT_GUIMODE,	/* -g */
    PMOPT_HOST,		/* -h */
    PMOPT_NAMESPACE,	/* -n */
    PMOPT_ORIGIN,	/* -O */
    PMOPT_GUIPORT,	/* -p */
    PMOPT_START,	/* -S */
    PMOPT_SAMPLES,	/* -s */
    PMOPT_FINISH,	/* -T */
    PMOPT_INTERVAL,	/* -t */
    PMOPT_VERSION,	/* -V */
    PMOPT_TIMEZONE,	/* -Z */
    PMOPT_HOSTZONE,	/* -z */
    PMOPT_HELP,		/* -? */
    PMOPT_HOSTSFILE,		/* -H */
    PMOPT_SPECLOCAL,		/* -K */
    PMOPT_LOCALPMDA,		/* -L */
    PMOPT_UNIQNAMES,		/* -N */
    PMOPT_CONTAINER,		/* --container=... */
    PMAPI_OPTIONS_HEADER("template options"),
    { "config", 1, 'c', "CONFIGFILE", "some configuration file" },
    { "flag", 0, 'f', "N", "some flag" },
    { "instance", 1, 'i', "INST", "some instance" },
    { "log", 1, 'l', "LOGFILE", "log file" },
    PMAPI_OPTIONS_END
};

static int overrides(int, pmOptions *);
static pmOptions opts = {
    .flags = PM_OPTFLAG_BOUNDARIES | PM_OPTFLAG_STDOUT_TZ,
    .short_options = "A:a:D:gh:n:O:p:S:s:T:t:VZ:z?" "H:K:LN:c:fi:l:",
    .long_options = longopts,
    .short_usage = "[options] ...",
    .override = overrides,
};

int	sflag;			/* ==1 if -s seen on command line */

static int
overrides(int opt, pmOptions *optsp)
{
    if (opt == 's')
	sflag++;
    return 0;
}

int
main(int argc, char **argv)
{
    int		c;
    char	*p;
    int		ctx;
    int		sts;
    char 	*configfile = NULL;
    int		fflag = 0;
    int		iflag = 0;
    int		lflag = 0;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'c':	/* my configfile */
	    if (configfile != NULL) {
		fprintf(stderr, "%s: at most one -c option allowed\n", pmGetProgname());
		exit(EXIT_FAILURE);
	    }
	    configfile = opts.optarg;
	    break;	

	case 'f':	/* my flag */
	    fflag++;
	    break;

	case 'i':	/* my instances */
	    iflag++;
	    /* TODO extract instances from opts.optarg */
	    break;

	case 'l':	/* my logfile */
	    if (lflag) {
		fprintf(stderr, "%s: at most one -l option allowed\n", pmGetProgname());
		exit(EXIT_FAILURE);
	    }
	    pmOpenLog(pmGetProgname(), opts.optarg, stderr, &sts);
	    if (sts != 1) {
		fprintf(stderr, "%s: Could not open logfile \"%s\"\n", pmGetProgname(), opts.optarg);
		exit(EXIT_FAILURE);
	    }
	    lflag++;
	    break;	

	}
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.narchives == 1) {
	if ((ctx = pmNewContext(PM_CONTEXT_ARCHIVE, opts.archives[0])) < 0) {
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), opts.archives[0],  pmErrStr(ctx));
	    exit(EXIT_FAILURE);
	}
	if ((sts = pmGetContextOptions(ctx, &opts)) < 0) {
	    pmflush();
	    fprintf(stderr, "%s: pmGetContextOptions(%d, ...) failed: %s\n",
			pmGetProgname(), pmWhichContext(), pmErrStr(sts));
		exit(EXIT_FAILURE);
	}
    }
    else if (opts.narchives > 0) {
	fprintf(stderr, "%s: at most one archive allowed\n", pmGetProgname());
	exit(EXIT_FAILURE);
    }

    if (opts.nhosts == 1) {
	if ((ctx = pmNewContext(PM_CONTEXT_HOST, opts.hosts[0])) < 0) {
	    fprintf(stderr, "%s: Cannot connect to pmcd on host \"%s\": %s\n",
		    pmGetProgname(), opts.hosts[0],  pmErrStr(ctx));
	    exit(EXIT_FAILURE);
	}
    }
    else if (opts.nhosts > 0) {
	fprintf(stderr, "%s: at most one host allowed\n", pmGetProgname());
	exit(EXIT_FAILURE);
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    if (opts.align_optarg != NULL)
	printf("Got -A \"%s\"\n", opts.align_optarg);

    if (opts.guiflag)
	printf("Got -g\n");

    if (opts.nsflag)
	printf("Loaded PMNS\n");

    if (opts.guiport)
	printf("Got -p \"%s\"\n", opts.guiport_optarg);

    if (opts.align_optarg != NULL || opts.start_optarg != NULL ||
        opts.finish_optarg != NULL || opts.origin_optarg != NULL) {
	printf("Start time: ");
	pmPrintStamp(stdout, &opts.start);
	putchar('\n');
	printf("Origin time: ");
	pmPrintStamp(stdout, &opts.origin);
	putchar('\n');
	printf("Finish time: ");
	pmPrintStamp(stdout, &opts.finish);
	putchar('\n');
    }

    if (sflag)
	printf("Got -s \"%d\"\n", opts.samples);

    if (opts.interval.tv_sec > 0 || opts.interval.tv_usec > 0)
	printf("Got -t %d.%06d (secs)\n",
		(int)opts.interval.tv_sec, (int)opts.interval.tv_usec);

    p = getenv("PCP_CONTAINER");
    if (p != NULL)
	printf("Got --container=\"%s\"\n", p);

    if (opts.timezone != NULL)
	printf("Got -Z \"%s\"\n", opts.timezone);

    if (opts.tzflag)
	printf("Got -z\n");

    if (opts.Lflag)
	printf("Got -L\n");

    /* non-flag args are argv[opts.optind] ... argv[argc-1] */
    while (opts.optind < argc) {
	printf("extra argument[%d]: %s\n", opts.optind, argv[opts.optind]);
	opts.optind++;
    }

    while (!sflag || opts.samples-- > 0) {
	/* put real stuff here */
	break;
    }

    return 0;
}
