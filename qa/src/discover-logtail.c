/*
 * Copyright (c) 2017 Red Hat.
 *
 * Example/template program for exercising pmDiscoverRegister(3) API in libpcp_web
 */
#include "discover.h" /* See src/libpcp_web/src/discover.h */

static pmLongOptions longopts[] = {
    PMAPI_GENERAL_OPTIONS,	/* -[AaDghnOpSsTtVZz?] */
    PMOPT_DEBUG,
    PMAPI_OPTIONS_HEADER("Options"),
    { "config", 1, 'c', "CONFIGFILE", "some configuration file" },
    { "foreground", 0, 'f', "N", "do not fork, run in the foreground" },
    { "log", 1, 'l', "LOGFILE", "log file" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_BOUNDARIES | PM_OPTFLAG_STDOUT_TZ,
    .short_options = PMAPI_OPTIONS "H:K:LN:c:fi:l:",
    .long_options = longopts,
    .short_usage = "[options] [dir ...]",
};

static void
resultPrintCallBack(char *source, pmTimeval *tv, pmResult *result)
{
    printf("->  result: source \"%s\" time %d.%06d numpmid %d\n",
    	source, tv->tv_sec, tv->tv_usec, result->numpmid);
}

static void
descPrintCallBack(char *source, pmTimeval *tv, pmDesc *desc, int numnames, char **names)
{
    int i;

    printf("->  desc: source \"%s\" time %d.%06d ", source, tv->tv_sec, tv->tv_usec);
    for (i=0; i < numnames; i++)
    	printf("\"%s\"%s", names[i], i<numnames-1 ? ", " : "\n");
    pmPrintDesc(stdout, desc);
}

static void
indomPrintCallBack(char *source, pmTimeval *tv, pmInResult *inresult)
{
    printf("->  indom: source \"%s\" time %d.%06d indom %s numinst %d\n",
    	source, tv->tv_sec, tv->tv_usec, pmInDomStr(inresult->indom), inresult->numinst);
}

static void
labelPrintCallBack(char *source, pmTimeval *tv, int ident, int type, int nsets, pmLabelSet *labelset)
{
    char identbuf[64];

    __pmLabelIdentString(ident, type, identbuf, sizeof(identbuf));
    printf("->  label: source \"%s\" time %d.%06d ident:%s type:%s nsets=%d\n",
    	source, tv->tv_sec, tv->tv_usec, identbuf, __pmLabelTypeString(type), nsets);
    /*
     * TODO pmPrintLabelSets(stdout, ident, type, NULL, 0);
     * See pmDiscoverDecodeMetaLabelset(), not fully implemented yet.
     */
}

static void
textPrintCallBack(char *source, pmTimeval *tv, int type, int id, char *text)
{
    printf("->  text: source \"%s\" time %d.%06d type=0x%02x ",
    	source, tv->tv_sec, tv->tv_usec, type);
    if (type & PM_TEXT_INDOM)
    	printf("INDOM %s\n", pmInDomStr((pmInDom)id));
    else
    	printf("PMID %s\n", pmIDStr((pmID)id));
    if (text)
    	printf("%s\n", text);
}

/*
 * This example set of callbacks just prints to stdout.
 * FUTURE TODO: additional pmDiscoverCallBacks to send to redis,
 * or http post to a host:port, whatever. dlopen them as DSOs
 * named in a config file.
 */
static pmDiscoverCallBacks discoverPrintCallBacks = {
    resultPrintCallBack,
    descPrintCallBack,
    indomPrintCallBack,
    labelPrintCallBack,
    textPrintCallBack,
};

int
main(int argc, char **argv)
{
    int		i;
    int		c;
    int		sts;
    char	**dirs;
    int		ndirs;
    char	defaultdir[MAXPATHLEN];
    char	*defaultdirs[1];
    char 	*configfile = NULL;
    int		fflag = 0;
    int		lflag = 0;

    pmSetProgname(argv[0]);
    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'c':
	    if (configfile != NULL) {
		fprintf(stderr, "%s: at most one -c option allowed\n", pmGetProgname());
		exit(EXIT_FAILURE);
	    }
	    configfile = opts.optarg;
	    break;	
	case 'f':
	    fflag++;
	    break;
	case 'l':
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

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    if (opts.timezone != NULL)
	fprintf(stderr, "Got -Z \"%s\"\n", opts.timezone);

    if (opts.tzflag)
	fprintf(stderr, "Got -z\n");

    /* non-flag args are argv[optind] ... argv[argc-1] */
    dirs = argv + opts.optind;
    if ((ndirs = argc - opts.optind) == 0) {
	/*
	 * default $PCP_LOG_DIR/pmlogger
	 * TODO: probably get this out of a pmproxy config file
	 */
	snprintf(defaultdir, sizeof(defaultdir), "%s%c%s", 
	    pmGetOptionalConfig("PCP_LOG_DIR"), pmPathSeparator(), "pmlogger");
	defaultdirs[0] = defaultdir;
	ndirs = 1;
	dirs = defaultdirs;
    }

    /*
     * Register the argument archive directories and our callback functions.
     * The origin for initial history replay is not implemented yet.
     * pmDiscoverRegister() should (but doesn't yet) keep a list of registered
     * callbacks. pmDiscoverUnregister() would remove entries from that list.
     */
    for (i=0; i < ndirs; i++) {
	pmTimeval *origin = NULL; /* not implemented yet */
	pmDiscoverRegister(dirs[i], origin, &discoverPrintCallBacks);
    }

    /*
     * Enter the uv main loop.
     * This returns if/when there is nothing left to monitor
     */
    uv_run(uv_default_loop(),UV_RUN_DEFAULT);

    exit(0);
}
