/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 2017-2025 Ken McDonell.  All Rights Reserved.
 *
 * Broad-shallow PMAPI coverage for PMAPI_VERSION and compatibility
 * testing.
 *
 * Based on bits and pieces of ...
 * - getoptions.c
 * - template.c
 * - churnctx.c
 *
 */

/*
 * PMAPI_VERSION_2 interfaces ... ones marked [y] are tested explicity by
 * this QA application, ones marked [i] are tested implicity because they
 * are called unconditionally from at least one of the interfaces marked
 * [y], ones marked [x] are excluded (mostly the "HighRes" variants, and
 * ones marked [ ] are TBD
 *
 * [ ] pmAddDerived
 * [ ] pmAddDerivedMetric
 * [ ] pmAddDerivedText
 * [ ] pmAddProfile
 * [ ] pmAtomStr
 * [ ] pmAtomStr_r
 * [ ] pmClearDebug
 * [ ] pmConvScale
 * [ ] pmCreateFetchGroup
 * [ ] pmCtime
 * [ ] pmDelProfile
 * [ ] pmDerivedErrStr
 * [ ] pmDestroyContext
 * [ ] pmDestroyFetchGroup
 * [ ] pmDiscoverServices
 * [ ] pmDupContext
 * [ ] pmErrStr
 * [ ] pmErrStr_r
 * [ ] pmEventFlagsStr
 * [ ] pmEventFlagsStr_r
 * [ ] pmExtendFetchGroup_event
 * [ ] pmExtendFetchGroup_indom
 * [ ] pmExtendFetchGroup_item
 * [ ] pmExtendFetchGroup_timestamp
 * [ ] pmExtractValue
 * [y] pmFetch
 * [ ] pmFetchArchive
 * [ ] pmFetchGroup
 * [ ] pmFetchHighRes
 * [ ] pmFetchHighResArchive
 * [y] pmflush
 * [ ] pmFreeEventResult
 * [ ] pmFreeHighResEventResult
 * [ ] pmFreeHighResResult
 * [ ] pmFreeLabelSets
 * [ ] pmFreeMetricSpec
 * [y] pmFreeOptions
 * [y] pmFreeResult
 * [ ] pmfstring
 * [ ] pmGetAPIConfig
 * [y] pmGetArchiveEnd
 * [y] pmGetArchiveLabel
 * [ ] pmGetChildren
 * [ ] pmGetChildrenStatus
 * [ ] pmGetClusterLabels
 * [ ] pmGetConfig
 * [ ] pmGetContextHostName
 * [ ] pmGetContextHostName_r
 * [ ] pmGetContextLabels
 * [y] pmGetContextOptions
 * [ ] pmGetDerivedControl
 * [ ] pmGetDomainLabels
 * [ ] pmGetFetchGroupContext
 * [x] pmGetHighResArchiveEnd
 * [x] pmGetHighResArchiveLabel
 * [ ] pmGetHostName
 * [y] pmGetInDom
 * [y] pmGetInDomArchive
 * [ ] pmGetInDomLabels
 * [ ] pmGetInstancesLabels
 * [ ] pmGetItemLabels
 * [ ] pmGetOptionalConfig
 * [y] pmGetOptions
 * [ ] pmgetopt_r
 * [ ] pmGetPMNSLocation
 * [ ] pmGetProgname
 * [ ] pmGetUsername
 * [ ] pmGetVersion
 * [x] pmHighResFetch
 * [y] pmID_build
 * [y] pmID_cluster
 * [y] pmID_domain
 * [y] pmID_item
 * [y] pmIDStr
 * [y] pmIDStr_r
 * [y] pmInDom_build
 * [y] pmInDom_domain
 * [y] pmInDom_serial
 * [y] pmInDomStr
 * [y] pmInDomStr_r
 * [ ] pmLoadASCIINameSpace
 * [ ] pmLoadDerivedConfig
 * [ ] pmLoadNameSpace
 * [y] pmLocaltime
 * [y] pmLookupDesc
 * [y] pmLookupDescs
 * [y] pmLookupInDom
 * [y] pmLookupInDomArchive
 * [ ] pmLookupInDomText
 * [ ] pmLookupLabels
 * [y] pmLookupName
 * [ ] pmLookupText
 * [ ] pmMergeLabels
 * [ ] pmMergeLabelSets
 * [ ] pmNameAll
 * [ ] pmNameID
 * [y] pmNameInDom
 * [y] pmNameInDomArchive
 * [y] pmNewContext
 * [ ] pmNewContextZone
 * [y] pmNewZone
 * [ ] pmNoMem
 * [ ] pmNotifyErr
 * [ ] pmNumberStr
 * [ ] pmNumberStr_r
 * [ ] pmOpenLog
 * [x] pmParseHighResInterval
 * [ ] pmParseHighResTimeWindow
 * [y] pmParseInterval
 * [ ] pmParseMetricSpec
 * [ ] pmParseTimeWindow
 * [ ] pmParseUnitsStr
 * [ ] pmPathSeparator
 * [ ] pmPrintDesc
 * [ ] pmprintf
 * [ ] pmPrintHighResStamp
 * [ ] pmPrintLabelSets
 * [ ] pmPrintStamp
 * [ ] pmPrintValue
 * [ ] pmReconnectContext
 * [ ] pmRegisterDerived
 * [ ] pmRegisterDerivedMetric
 * [ ] pmSemStr
 * [ ] pmSemStr_r
 * [y] pmSetDebug
 * [ ] pmSetDerivedControl
 * [y] pmSetMode
 * [x] pmSetModeHighRes
 * [ ] pmSetProcessIdentity
 * [ ] pmSetProgname
 * [ ] pmSortHighResInstances
 * [ ] pmSortInstances
 * [ ] pmSpecLocalPMDA
 * [y] pmsprintf
 * [ ] pmStore
 * [ ] pmStoreHighRes
 * [ ] pmstrlen
 * [ ] pmstrncat
 * [ ] pmstrncpy
 * [ ] pmSyslog
 * [y] pmtimespecAdd
 * [y] pmtimespecDec
 * [y] pmtimespecFromReal
 * [y] pmtimespecInc
 * [y] pmtimespecNow
 * [y] pmtimespecSub
 * [y] pmtimespecToReal
 * [y] pmtimevalAdd
 * [y] pmtimevalDec
 * [y] pmtimevalFromReal
 * [y] pmtimevalInc
 * [y] pmtimevalNow
 * [y] pmtimevalSub
 * [y] pmtimevalToReal
 * [ ] pmTraversePMNS
 * [ ] pmTraversePMNS_r
 * [ ] pmTrimNameSpace
 * [ ] pmTypeStr
 * [ ] pmTypeStr_r
 * [ ] pmUnitsStr
 * [ ] pmUnitsStr_r
 * [ ] pmUnloadNameSpace
 * [ ] pmUnpackEventRecords
 * [ ] pmUnpackHighResEventRecords
 * [y] pmUsageMessage
 * [ ] pmUseContext
 * [ ] pmUseZone
 * [ ] pmWhichContext
 * [ ] pmWhichZone
 */


#include "pcp/pmapi.h"
#include "pcp/libpcp.h"

static int
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

static char *
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

static char *
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

#if PMAPI_VERSION < 4
static char *
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
static char *
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
    pmsprintf(tv, sizeof(tv), "%02d/%02d/%04d %02d:%02d:%02d.%06d",
	tmp.tm_mday, tmp.tm_mon+1, tmp.tm_year+1900,
	tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)tp->tv_nsec / 1000);
    return tv;
}
#endif

#if PMAPI_VERSION < 4
static char *
interstr(struct timeval *tp)
{
    static char it[128];

    if (tp->tv_sec == 0)
	return "-";
    pmsprintf(it, sizeof(it), "%0d.%06d", (int)tp->tv_sec, (int)tp->tv_usec);
    return it;
}
#else
static char *
interstr(struct timespec *tp)
{
    static char it[128];

    if (tp->tv_sec == 0)
	return "-";
    pmsprintf(it, sizeof(it), "%0d.%06d", (int)tp->tv_sec, (int)tp->tv_nsec / 1000);
    return it;
}
#endif

static char *
isempty(char *string)
{
    if (string == NULL)
	return "-";
    return string;
}

static void
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
	PMAPI_GENERAL_OPTIONS,		/* -A, -a, -D, -g, -h, -n, -O, -p, -S, -s, -T, -t, -Z, -z, -V, -? */
	PMOPT_HOSTSFILE,		/* -H */
	PMOPT_SPECLOCAL,		/* -K */
	PMOPT_LOCALPMDA,		/* -L */
	PMOPT_UNIQNAMES,		/* -N */
	PMOPT_CONTAINER,		/* --container=NAME */
	PMOPT_HOST_LIST,		/* --host-list=HOST,HOST,... */
	PMOPT_ARCHIVE_LIST,		/* --archive-list=ARCHIVE,ARCHIVE,... */
	PMOPT_ARCHIVE_FOLIO,		/* --archive-folio=NAME */
	PMOPT_DERIVED,			/* --derived=FILE */
	{ "interval", 1, 'i', "DELTA", "a time interval" },
	{ "metric", 1, 'm', "METRIC", "the metric [default: sample.colour]" },
	PMAPI_OPTIONS_END
    };
    pmOptions opts = {
	.flags = PM_OPTFLAG_BOUNDARIES | PM_OPTFLAG_STDOUT_TZ,
	.short_options = PMAPI_OPTIONS "H:K:LN:" "i:m:" ,
	.long_options = longopts,
	.short_usage = "[options] ...",
    };
    int sts;
    char *tz;
    int ctx = -1;
    int c;
#if PMAPI_VERSION < 4
    struct timeval	start;
    struct timeval	end;
    struct timeval	delta = { 1, 0 };
    int			msec;
#else
    struct timespec	start;
    struct timespec	end;
    struct timespec	delta = { 1, 0 };
#endif
    char	*name = "sample.colour";
    pmID	pmid;
    pmDesc	desc;
    pmInDom	indom;
    pmLogLabel	label;
    char	*errmsg;

    /*
     * options tests ...
     */
    opts.flags = getflags();
    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	    case 'm':
		name = opts.optarg;
		break;

	    case 'i':
		if ((sts = pmParseInterval(opts.optarg, &delta, &errmsg)) < 0) {
		    printf("pmParseInterval: Fail: %s (%s)\n", pmErrStr(sts), errmsg);
		    free(errmsg);
		}
		else {
		    printf("-i interval: %s", interstr(&delta));
#if PMAPI_VERSION >= 4
		    printf(" (");
		    pmPrintInterval(stdout, &delta);
		    putchar(')');
#endif
		putchar('\n');
		}
	}
    }

    printf("End of option processing\n");
    if (opts.flags & PM_OPTFLAG_USAGE_ERR) {
	pmUsageMessage(&opts);
	exit(1);
    }
    if (opts.flags & PM_OPTFLAG_RUNTIME_ERR) {
	pmflush();
	exit(1);
    }

    if (opts.timezone)	/* ensure we have deterministic output */
	tz = opts.timezone;
    else
	tz = "UTC";
    if ((c = pmNewZone(tz)) < 0)
	printf("pmNewZone: Fail: %s\n", pmErrStr(c));
    else if ((sts = pmUseZone(c)) < 0)
	printf("pmUseZone: Fail: %s\n", pmErrStr(sts));
    if (opts.errors == 0) {
	char *err;
	if (opts.nhosts == 0 && opts.narchives == 0)
	    ctx = pmNewContext(PM_CONTEXT_HOST, "local:");
	else if (opts.nhosts > 0)
	    ctx = pmNewContext(PM_CONTEXT_HOST, opts.hosts[0]);
	else if (opts.narchives > 0)
	    ctx = pmNewContext(PM_CONTEXT_ARCHIVE, opts.archives[0]);
	if (ctx >= 0) {
	    if ((sts = pmGetContextOptions(ctx, &opts)) < 0)
		printf("pmGetContextOptions: Fail: %s\n", pmErrStr(sts));
	}
	else {
	    printf("pmNewContext: Fail: %s\n", pmErrStr(ctx));
	    exit(1);
	}
	if ((sts = pmUseContext(ctx)) < 0) {
	    printf("pmUseContext: Fail: %s\n", pmErrStr(sts));
	    exit(1);
	}
	/* current context is valid */
	if (opts.narchives > 0) {
	    /* archive context */
	    if ((sts = pmGetArchiveLabel(&label)) < 0) {
		printf("pmGetArchiveLabel: Fail: %s\n", pmErrStr(sts));
		exit(1);
	    }
	    else {
#if PMAPI_VERSION < 4
		printf("pmGetArchiveLabel: OK: hostname=%s\n", label.ll_hostname);
#else
		printf("pmGetArchiveLabel: OK: hostname=%s\n", label.hostname);
#endif
	    }
#if PMAPI_VERSION < 4
	    start = label.ll_start;
#else
	    start = label.start;
#endif
	    if ((sts = pmGetArchiveEnd(&end)) < 0) {
		end.tv_sec = PM_MAX_TIME_T;
#if PMAPI_VERSION < 4
		end.tv_usec = 0;
#else
		end.tv_nsec = 0;
#endif
		printf("pmGetArchiveEnd: Fail: %s\n", pmErrStr(sts));
	    }
	    else
		printf("pmGetArchiveEnd: OK: time: %s\n", timestr(&end));
#if PMAPI_VERSION < 4
	    msec = delta.tv_sec * 1000 + delta.tv_usec / 1000;
	    sts = pmSetMode(PM_MODE_INTERP, &start, msec);
#else
	    sts = pmSetMode(PM_MODE_INTERP, &start, &delta);
#endif
	    if (sts < 0)
		printf("pmSetMode: Fail: %s\n", pmErrStr(sts));
	}
	else {
	    /* host context */
	    end.tv_sec = PM_MAX_TIME_T;
#if PMAPI_VERSION < 4
	    pmtimevalNow(&start);
	    end.tv_usec = 0;
	    msec = delta.tv_sec * 1000 + delta.tv_usec / 1000;
	    sts = pmSetMode(PM_MODE_LIVE, &start, msec);
#else
	    pmtimespecNow(&start);
	    end.tv_nsec = 0;
	    sts = pmSetMode(PM_MODE_LIVE, &start, &delta);
#endif
	    if (sts < 0)
		printf("pmSetMode: Fail: %s\n", pmErrStr(sts));
	}
	if ((sts = pmParseTimeWindow(opts.start_optarg, opts.finish_optarg, opts.align_optarg, opts.origin_optarg, &start, &end, &opts.start, &opts.finish, &opts.origin, &err)) < 0)
	printf("pmParseTimeWindow: Fail: %s: %s\n", err, pmErrStr(sts));
    }
    dumpall(&opts);
    pmFreeOptions(&opts);
    if (opts.errors)
	exit(1);

    /*
     * PMNS tests ...
     */
    if ((sts = pmLookupName(1, (const char **)&name, &pmid)) < 0) {
	printf("pmLookupName: Fail: %s\n", pmErrStr(sts));
	pmid = PM_ID_NULL;
    }
    else {
	char	buf1[20];
	char	buf2[20];
	pmID	tmp;
	printf("pmLookupName: OK: %s -> %s\n", name, pmIDStr(pmid));
	tmp = pmID_build(pmID_domain(pmid), pmID_cluster(pmid), pmID_item(pmid));
	if (pmid == tmp)
	    printf("pmID_*: OK %s\n", pmIDStr_r(pmid, buf1, sizeof(buf1)));
	else {
	    printf("pmID_*: Fail: %s != %s\n", pmIDStr_r(tmp, buf1, sizeof(buf1)), pmIDStr_r(pmid, buf2, sizeof(buf2)));
	}
    }

    /*
     * pmDesc tests ...
     */
    indom = PM_INDOM_NULL;
    if (pmid != PM_ID_NULL) {
	if ((sts = pmLookupDescs(1, &pmid, &desc)) < 0)
	    printf("pmLookupDescs: Fail: %s\n", pmErrStr(sts));
	else if ((sts = pmLookupDesc(pmid, &desc)) < 0)
	    printf("pmLookupDesc: Fail: %s\n", pmErrStr(sts));
	else if (desc.indom != PM_INDOM_NULL) {
	    char	buf1[20];
	    char	buf2[20];
	    pmInDom	temp;
	    temp = pmInDom_build(pmInDom_domain(desc.indom), pmInDom_serial(desc.indom));
	    if (temp == desc.indom) {
		printf("pmInDom_*: OK %s\n", pmInDomStr_r(temp, buf1, sizeof(buf1)));
		indom = desc.indom;
	    }
	    else
		printf("pmInDom_*: Fail: %s != %s\n", pmInDomStr_r(temp, buf1, sizeof(buf1)), pmInDomStr_r(desc.indom, buf2, sizeof(buf2)));
	}
    }

    /*
     * pmFetch tests ...
     */
    if (pmid != PM_ID_NULL) {
	pmResult	*rp;
	if ((sts = pmFetch(1, &pmid, &rp)) < 0)
	    printf("pmFetch: Fail: %s\n", pmErrStr(sts));
	else {
	    __pmDumpResult(stdout, rp);
	    pmFreeResult(rp);
	}
    }
#if PMAPI_VERSION < 4
    if (pmid != PM_ID_NULL) {
	pmHighResResult	*rp;
	if ((sts = pmHighResFetch(1, &pmid, &rp)) < 0)
	    printf("pmHighResFetch: Fail: %s\n", pmErrStr(sts));
	else {
	    __pmDumpHighResResult(stdout, rp);
	    pmFreeHighResResult(rp);
	}
    }
#endif

    /*
     * pmInDom tests ...
     */
    if (indom != PM_INDOM_NULL) {
	int	*instlist;
	char	**namelist;
	char	*temp;
	if (opts.nhosts > 0) {
	    if ((sts = pmGetInDom(indom, &instlist, &namelist)) < 0)
		printf("pmGetInDom: Fail: %s\n", pmErrStr(sts));
	    else {
		int	j = sts - 1;
		if ((sts = pmLookupInDom(indom, namelist[j])) < 0)
		    printf("pmLookupInDom: Fail: %s\n", pmErrStr(sts));
		else if ((sts = pmNameInDom(indom, instlist[j], &temp)) < 0)
		    printf("pmNameInDom: Fail: %s\n", pmErrStr(sts));
		else {
		    free(temp);
		    printf("pm*InDom*: OK\n");
		}
		free(instlist);
		free(namelist);
	    }
	}
	else if (opts.narchives > 0) {
	    if ((sts = pmGetInDomArchive(indom, &instlist, &namelist)) < 0)
		printf("pmGetInDomArchive: Fail: %s\n", pmErrStr(sts));
	    else {
		int	j = sts - 1;
		if ((sts = pmLookupInDomArchive(indom, namelist[j])) < 0)
		    printf("pmLookupInDomArchive: Fail: %s\n", pmErrStr(sts));
		else if ((sts = pmNameInDomArchive(indom, instlist[j], &temp)) < 0)
		    printf("pmNameInDomArchive: Fail: %s\n", pmErrStr(sts));
		else {
		    free(temp);
		    printf("pm*InDomArchive*: OK\n");
		}
		free(instlist);
		free(namelist);
	    }
	}
    }

    /*
     * timeval tests (only for live contexts to reduce duplication) ...
     */
    if (opts.nhosts > 0) {
	struct timeval	tv1 = { 123, 456000 };
	struct timeval	tv2;
	double		f;
	double		xpect;
	printf("timeval functions ...\n");
	if ((f = pmtimevalToReal(&tv1)) != 123.456)
	    printf("pmtimevalToReal: Fail: %.6f != 123.456\n", f);
	pmtimevalFromReal(456.123, &tv2);
	if (tv2.tv_sec != 456 || tv2.tv_usec != 123000)
	    printf("pmtimevalFromReal: Fail: %d.%06d != 456.123\n", (int)tv2.tv_sec, (int)tv2.tv_usec);
	tv2.tv_sec = 1;
	tv2.tv_usec = 1000;
	xpect = 124.457;
	if ((f = pmtimevalAdd(&tv1, &tv2)) != xpect)
	    printf("pmtimevalAdd: Fail: %.6f != %.6f\n", f, xpect);
	xpect = 122.455;
	if ((f = pmtimevalSub(&tv1, &tv2)) != xpect)
	    printf("pmtimevalSub: Fail: %.6f != %.6f\n", f, xpect);
	xpect = 124.457;
	pmtimevalInc(&tv1, &tv2);
	f = pmtimevalToReal(&tv1);
	if (f != xpect)
	    printf("pmtimevalInc: Fail: %.6f != %.6f\n", f, xpect);
	xpect = 123.456;
	pmtimevalDec(&tv1, &tv2);
	f = pmtimevalToReal(&tv1);
	if (f != xpect)
	    printf("pmtimevalDec: Fail: %.6f != %.6f\n", f, xpect);
	pmtimevalNow(&tv1);
	if (tv1.tv_sec != 123 && tv1.tv_usec != 456000)
	    printf("pmtimeval*: OK\n");
	else
	    printf("pmtimevalNow: Fail: %d.%06d unexpected\n", (int)tv1.tv_sec, (int)tv1.tv_usec);
    }

    /*
     * timespec tests (only for live contexts to reduce duplication) ...
     */
    if (opts.nhosts > 0) {
	struct timespec	ts1 = { 123, 456000000 };
	struct timespec	ts2;
	double		f;
	double		xpect;
	printf("timespec functions ...\n");
	if ((f = pmtimespecToReal(&ts1)) != 123.456)
	    printf("pmtimespecToReal: Fail: %.6f != 123.456\n", f);
	pmtimespecFromReal(456.123, &ts2);
	if (ts2.tv_sec != 456 || ts2.tv_nsec != 123000000)
	    printf("pmtimespecFromReal: Fail: %d.%06d != 456.123\n", (int)ts2.tv_sec, (int)ts2.tv_nsec);
	ts2.tv_sec = 1;
	ts2.tv_nsec = 1000000;
	xpect = 124.457;
	if ((f = pmtimespecAdd(&ts1, &ts2)) != xpect)
	    printf("pmtimespecAdd: Fail: %.6f != %.6f\n", f, xpect);
	xpect = 122.455;
	if ((f = pmtimespecSub(&ts1, &ts2)) != xpect)
	    printf("pmtimespecSub: Fail: %.6f != %.6f\n", f, xpect);
	xpect = 124.457;
	pmtimespecInc(&ts1, &ts2);
	f = pmtimespecToReal(&ts1);
	if (f != xpect)
	    printf("pmtimespecInc: Fail: %.6f != %.6f\n", f, xpect);
	xpect = 123.456;
	pmtimespecDec(&ts1, &ts2);
	f = pmtimespecToReal(&ts1);
	if (f != xpect)
	    printf("pmtimespecDec: Fail: %.6f != %.6f\n", f, xpect);
	pmtimespecNow(&ts1);
	if (ts1.tv_sec != 123 && ts1.tv_nsec != 456000000)
	    printf("pmtimespec*: OK\n");
	else
	    printf("pmtimespecNow: Fail: %d.%06d unexpected\n", (int)ts1.tv_sec, (int)ts1.tv_nsec);
    }

    exit(0);
}
