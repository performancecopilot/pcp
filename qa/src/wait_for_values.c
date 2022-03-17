/*
 * wait_for_values - wait a defined time to receive at least one non-zero value
 * based on indom.c
 */

#include <time.h>
#include <pcp/pmapi.h>

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_ARCHIVE,
    PMOPT_DEBUG,
    PMOPT_HOST,
    { "wait", 1, 'w', "MSEC", "wait MSEC milliseconds for a non-zero value" },
    PMOPT_LOCALPMDA,
    PMOPT_SPECLOCAL,
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_STDOUT_TZ,
    .short_options = "a:D:h:i:K:Lw:zZ:?",
    .long_options = longopts,
    .short_usage = "[options] [-w SEC] metricname ...",
};

/**
 * try to fetch at least one non-zero value
 * returns 1 on success, 0 if no values were found and negative values for errors
 */
int
dometric(const char *name)
{
    pmID	pmid;
    pmDesc	desc;
    int		i, sts;
    pmResult	*rp;
    pmValueSet	*valset;
    int		found_value = 0;

    if ((sts = pmLookupName(1, &name, &pmid)) < 0) {
	fprintf(stderr, "pmFetch: %s\n", pmErrStr(sts));
	return sts;
    }
    if ((sts = pmLookupDesc(pmid, &desc)) < 0) {
	fprintf(stderr, "pmFetch: %s\n", pmErrStr(sts));
	return sts;
    }
    if ((sts = pmFetch(1, &desc.pmid, &rp)) < 0) {
	fprintf(stderr, "pmFetch: %s\n", pmErrStr(sts));
	return sts;
    }

    valset = rp->vset[0];
    for (i = 0; i < valset->numval; i++) {
	if (valset->vlist[i].value.lval != 0) {
	    found_value = 1;
	    break;
	}
    }

    pmFreeResult(rp);
    return found_value;
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		exitsts = 0;
    int		contextid;
    char	*source;
    char	*endnum;
    int		wait_ms = 0;
    int		total_waited;
    struct timespec delay = { 0, 100000000 };	/* 0.1 sec */

    pmSetProgname(argv[0]);
    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'w':	/* wait time */
	    wait_ms = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || wait_ms < 0) {
		pmprintf("%s: -w requires numeric argument\n", pmGetProgname());
		opts.errors++;
	    }
	    break;
	}
    }

    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT)) {
	exitsts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(exitsts);
    }

    if (opts.context == PM_CONTEXT_ARCHIVE)
	source = opts.archives[0];
    else if (opts.context == PM_CONTEXT_HOST)
	source = opts.hosts[0];
    else if (opts.context == PM_CONTEXT_LOCAL)
	source = NULL;
    else {
	opts.context = PM_CONTEXT_HOST;
	source = "local:";
    }
    if ((sts = pmNewContext(opts.context, source)) < 0) {
	if (opts.context == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmGetProgname(), source, pmErrStr(sts));
	else if (opts.context == PM_CONTEXT_LOCAL)
	    fprintf(stderr, "%s: Cannot make local standalone connection: %s\n",
		pmGetProgname(), pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmGetProgname(), source, pmErrStr(sts));
	exit(1);
    }
    contextid = sts;

    if (opts.context == PM_CONTEXT_ARCHIVE) {
	if (pmGetContextOptions(contextid, &opts)) {
	    pmflush();
	    exit(1);
	}
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    while (opts.optind < argc) {
	sts = dometric(argv[opts.optind]);
	for (total_waited = 0; sts <= 0 && total_waited + 100 <= wait_ms; total_waited += 100) {
	    nanosleep(&delay, NULL);
	    sts = dometric(argv[opts.optind]);
	}

	if (sts < 0) {
	    printf("Error fetching metrics for %s\n", argv[opts.optind]);
	    return 1;
	}
	else if (sts == 0) {
	    printf("Error: Did not receive a single value for %s\n", argv[opts.optind]);
	    return 1;
	}
	else {
	    printf("Found a value for %s\n", argv[opts.optind]);
	}
	opts.optind++;
    }

    return 0;
}
