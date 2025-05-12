/*
 * Copyright (c) 2014 Red Hat.
 *
 * Exercise the pmGetOptions() series of PMAPI interfaces
 */

#include <pcp/pmapi.h>

int
getversion(void)
{
    int version = 0;
    char *value;

    if ((value = getenv("PMAPI_VERSION")) != NULL)
	version = atoi(value);
    return version;
}

int
getflags(void)
{
    int flags = 0;

    if (getenv("PM_OPTFLAG_MULTI") != NULL)
	flags |= PM_OPTFLAG_MULTI;
    if (getenv("PM_OPTFLAG_POSIX") != NULL)
	flags |= PM_OPTFLAG_POSIX;
    if (getenv("PM_OPTFLAG_MIXED") != NULL)
	flags |= PM_OPTFLAG_MIXED;
    if (getenv("PM_OPTFLAG_ENV_ONLY") != NULL)
	flags |= PM_OPTFLAG_ENV_ONLY;
    if (getenv("PM_OPTFLAG_LONG_ONLY") != NULL)
	flags |= PM_OPTFLAG_LONG_ONLY;
    if (getenv("PM_OPTFLAG_BOUNDARIES") != NULL)
	flags |= PM_OPTFLAG_BOUNDARIES;
    if (getenv("PM_OPTFLAG_STDOUT_TZ") != NULL)
	flags |= PM_OPTFLAG_STDOUT_TZ;
    return flags;
}

char *
flagsstr(pmOptions *opts)
{
    static char flags[128];

    if (opts->flags & PM_OPTFLAG_INIT)
	strcat(flags, ",init");
    if (opts->flags & PM_OPTFLAG_DONE)
	strcat(flags, ",done");
    if (opts->flags & PM_OPTFLAG_MULTI)
	strcat(flags, ",multi");
    if (opts->flags & PM_OPTFLAG_USAGE_ERR)
	strcat(flags, ",usage_err");
    if (opts->flags & PM_OPTFLAG_RUNTIME_ERR)
	strcat(flags, ",runtime_err");
    if (opts->flags & PM_OPTFLAG_EXIT)
	strcat(flags, ",exit");
    if (opts->flags & PM_OPTFLAG_POSIX)
	strcat(flags, ",posix");
    if (opts->flags & PM_OPTFLAG_MIXED)
	strcat(flags, ",mixed");
    if (opts->flags & PM_OPTFLAG_ENV_ONLY)
	strcat(flags, ",env_only");
    if (opts->flags & PM_OPTFLAG_LONG_ONLY)
	strcat(flags, ",long_only");
    if (opts->flags & PM_OPTFLAG_BOUNDARIES)
	strcat(flags, ",boundaries");
    if (opts->flags & PM_OPTFLAG_STDOUT_TZ)
	strcat(flags, ",stdout_tz");

    if (flags[0] == '\0')	/* no flags */
	flags[1] = '-';
    return &flags[1];
}

char *
contextstr(pmOptions *opts)
{
    if (opts->context == PM_CONTEXT_HOST)
	return "host";
    if (opts->context == PM_CONTEXT_ARCHIVE)
	return "archive";
    if (opts->context == PM_CONTEXT_LOCAL)
	return "local";
    return "-";
}

#if PMAPI_VERSION == PMAPI_VERSION_2
char *
timestr(struct timeval *tp)
{
    struct tm   tmp;
    time_t      then;
    static char tv[128];

    if (tp->tv_sec == 0)
	return "-";
    if (tp->tv_sec == PM_MAX_TIME_T)
	return "PM_MAX_TIME_T";
    then = (time_t)tp->tv_sec;
    pmLocaltime(&then, &tmp);
    pmsprintf(tv, sizeof(tv), "%02d/%02d/%04d %02d:%02d:%02d.%06d",
	tmp.tm_mday, tmp.tm_mon+1, tmp.tm_year+1900,
	tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)tp->tv_usec);
    return tv;
}
#else
char *
timestr(struct timespec *tp)
{
    struct tm   tmp;
    time_t      then;
    static char tv[128];

    if (tp->tv_sec == 0)
	return "-";
    if (tp->tv_sec == PM_MAX_TIME_T)
	return "PM_MAX_TIME_T";
    memset(tv, 0, sizeof(tv));
    then = (time_t)tp->tv_sec;
    pmLocaltime(&then, &tmp);
    pmsprintf(tv, sizeof(tv), "%02d/%02d/%04d %02d:%02d:%02d.%09d",
	tmp.tm_mday, tmp.tm_mon+1, tmp.tm_year+1900,
	tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)tp->tv_nsec);
    return tv;
}
#endif

#if PMAPI_VERSION == PMAPI_VERSION_2
char *
interstr(struct timeval *tp)
{
    static char it[128];

    if (tp->tv_sec == 0)
	return "-";
    pmsprintf(it, sizeof(it), "%0d.%06d", (int)tp->tv_sec, (int)tp->tv_usec);
    return it;
}
#else
char *
interstr(struct timespec *tp)
{
    static char it[128];

    if (tp->tv_sec == 0)
	return "-";
    pmsprintf(it, sizeof(it), "%0d.%09d", (int)tp->tv_sec, (int)tp->tv_nsec);
    return it;
}
#endif

char *
isempty(char *string)
{
    if (string == NULL)
	return "-";
    return string;
}

void
dumpall(pmOptions *opts)
{
    int i;

    printf("Options structure dump:\n");
    printf("    version: %d\n", opts->version);
    printf("    flags: 0x%x (%s)\n", opts->flags, flagsstr(opts));
    printf("    errors: %d\n", opts->errors);
    printf("    context: 0x%x (%s)\n", opts->context, contextstr(opts));
    printf("    nhosts: %d\n", opts->nhosts);
    if (opts->nhosts > 0) {
	printf("    hosts: %s", opts->hosts[0]);
	for (i = 1; i < opts->nhosts; i++)
	    printf(",%s", opts->hosts[i]);
	printf("\n");
    }
    printf("    narchives: %d\n", opts->narchives);
    if (opts->narchives > 0) {
	printf("    archives: %s", opts->archives[0]);
	for (i = 1; i < opts->narchives; i++)
	    printf(",%s", opts->archives[i]);
	printf("\n");
    }
    printf("    start: %s\n", timestr(&opts->start));
    printf("    finish: %s\n", timestr(&opts->finish));
    printf("    origin: %s\n", timestr(&opts->origin));
    printf("    interval: %s\n", interstr(&opts->interval));
    printf("    align_optarg: %s\n", isempty(opts->align_optarg));
    printf("    start_optarg: %s\n", isempty(opts->start_optarg));
    printf("    finish_optarg: %s\n", isempty(opts->finish_optarg));
    printf("    origin_optarg: %s\n", isempty(opts->origin_optarg));
    printf("    guiport_optarg: %s\n", isempty(opts->guiport_optarg));
    printf("    timezone: %s\n", isempty(opts->timezone));
    printf("    samples: %d\n", opts->samples);
    printf("    guiport: %d\n", opts->guiport);
    printf("    guiflag: %d\n", opts->guiflag);
    printf("    tzflag: %d\n", opts->tzflag);
    printf("    Lflag: %d\n", opts->Lflag);
}

int
main(int argc, char *argv[])
{
    pmLongOptions longopts[] = {
	PMAPI_GENERAL_OPTIONS,
	PMAPI_OPTIONS_HEADER("Context options"),
	PMOPT_SPECLOCAL,
	PMOPT_LOCALPMDA,
	PMOPT_HOSTSFILE,
	PMOPT_HOST_LIST,
	PMOPT_ARCHIVE_LIST,
	PMOPT_ARCHIVE_FOLIO,
	PMAPI_OPTIONS_HEADER("Testing options"),
	{ "window", 0, 'w', 0, "do time window parsing" },
	{ "extra", 0, 'x', 0, "an extra option, for testing" },
	{ "eXtra", 1, 'X', "ARG", "an extra option with an argument" },
	{ "", 0, 'y', 0, "a short-option-only without argument" },
	{ "", 1, 'Y', "N", "a short-option-only with an argument" },
	{ "fubar", 0, 0, 0, "a long-option-only without argument" },
	{ "foobar", 1, 0, "N", "a long-option-only with an argument" },
	PMAPI_OPTIONS_END
    };
    pmOptions opts = {
	.flags = PM_OPTFLAG_BOUNDARIES | PM_OPTFLAG_STDOUT_TZ,
	.short_options = PMAPI_OPTIONS "H:K:L" "wXxYy",
	.long_options = longopts,
    };
    int sts;
    char *tz;
    int ctx = -1;
    int c;
    int wflag = 0;

    opts.version = getversion();
    opts.flags = getflags();
    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	printf("Got option: %c index=%d [errors=%d]\n", c? c: '-', opts.index, opts.errors);
	switch(c) {
	case 'w':
	    wflag++;
	    break;
	case 'x':
	    printf(" -> x option has no argument\n");
	    break;
	case 'X':
	    printf(" -> X option argument was: '%s'\n", opts.optarg);
	    break;
	case 'y':
	    printf(" -> y option has no argument\n");
	    break;
	case 'Y':
	    printf(" -> Y option argument was: '%s'\n", opts.optarg);
	    break;
	case 0:	/* long-only options */
	    printf(" -> long-only option index=%d\n", opts.index);
	    break;
	default:
	    printf(" -> unexpected option: '%c'\n", c);
	    pmUsageMessage(&opts);
	    exit(1);
	}
    }
    printf("End of option processing\n");

    if (opts.flags & PM_OPTFLAG_USAGE_ERR)
	fprintf(stderr, "Warning: usage error detected\n");
    if (opts.flags & PM_OPTFLAG_RUNTIME_ERR)
	fprintf(stderr, "Warning: runtime error detected\n");
    if (opts.padding != 0)
	fprintf(stderr, "Warning: non-zero padding\n");
    if (opts.zeroes != 0)
        fprintf(stderr, "Warning: unexpected non-zeroes\n");

    if (opts.flags & PM_OPTFLAG_USAGE_ERR)
	pmUsageMessage(&opts);
    if (opts.flags & PM_OPTFLAG_RUNTIME_ERR)
	pmflush();

    if (opts.timezone)	/* ensure we have deterministic output */
	tz = opts.timezone;
    else
	tz = "UTC";

    if ((c = pmNewZone(tz)) < 0)
	fprintf(stderr, "Warning: TZ failure - %s: %s\n",
			opts.timezone, pmErrStr(c));

    if (wflag && opts.errors == 0) {
	if (opts.nhosts == 0 && opts.narchives == 0)
	    ctx = pmNewContext(PM_CONTEXT_HOST, "local:");
	else if (opts.nhosts > 0)
	    ctx = pmNewContext(PM_CONTEXT_HOST, opts.hosts[0]);
	else if (opts.narchives > 0)
	    ctx = pmNewContext(PM_CONTEXT_ARCHIVE, opts.archives[0]);
	if (ctx >= 0) {
	    if ((sts = pmGetContextOptions(ctx, &opts)) < 0)
		fprintf(stderr, "Warning: pmGetContextOptions failed: %s\n", pmErrStr(sts));
	}
	else {
	    fprintf(stderr, "Error: failed to establish a PMAPI context: %s\n", pmErrStr(ctx));
	}
    }

    dumpall(&opts);

    pmFreeOptions(&opts);
    exit(0);
}
