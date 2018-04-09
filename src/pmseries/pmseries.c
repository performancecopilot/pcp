/*
 * Copyright (c) 2017-2018 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "series.h"
#include "pmapi.h"
#include "sds.h"

typedef enum series_flags {
    PMSERIES_COLOUR	= (1<<0),	/* report in colour if possible */
    PMSERIES_FAST	= (1<<1),	/* load only the metric metadata */
    PMSERIES_FULLINDOM	= (1<<2),	/* report with pminfo(1) -I info */
    PMSERIES_FULLPMID	= (1<<3),	/* report with pminfo(1) -M info */
    PMSERIES_SERIESID	= (1<<4),	/* report with pminfo(1) -s info */
    PMSERIES_SOURCEID	= (1<<5),	/* report with pminfo(1) -S info */
    PMSERIES_NEED_EOL	= (1<<6),	/* need to eol-terminate output */
} series_flags;

typedef struct series_insts {
    pmSeriesID		series;		/* instance series identifier */
    sds			instid;		/* internal instance identifier */
    sds			name;		/* external instance identifier */
} series_insts;

typedef struct series_data {
    int			status;		/* command exit status */
    series_flags	flags;		/* flags affecting reporting */

    pmSeriesID		series;		/* current time series */
    pmSourceID		source;		/* current time series source */
    sds			type;		/* current time series data type */
    unsigned int        ninsts;		/* instances for the current series */
    series_insts	*insts;		/* instances for the current series */
} series_data;

static void
series_data_init(series_data *dp, series_flags flags)
{
    memset(dp, 0, sizeof(series_data));
    dp->series = sdsnewlen("", 40);
    dp->flags = flags;
}

static int
series_split(sds string, pmSeriesID **series)
{
    size_t		length = strlen(string);
    int			nseries = 0;

    if (!string || !sdslen(string))
	return 0;
    if ((*series = sdssplitlen(string, length, ",", 1, &nseries)) == NULL)
	return -ENOMEM;
    return nseries;
}

static void
series_free(int nseries, pmSeriesID *series)
{
    if (nseries) {
	while (--nseries)
	    sdsfree(series[nseries]);
	free(series);
    }
}

static series_insts *
series_get_inst(series_data *dp, sds series)
{
    series_insts	*ip;
    int			i;

    for (i = 0; i < dp->ninsts; i++) {
	ip = &dp->insts[i];
	if (sdscmp(series, ip->series) == 0)
	    return ip;
    }
    return NULL;
}

static void
series_add_inst(series_data *dp, pmSeriesID series, sds instid, sds instname)
{
    series_insts	*ip;
    size_t		bytes = sizeof(series_insts) * dp->ninsts;

    if ((ip = realloc(dp->insts, bytes)) != NULL) {
	ip->series = sdsdup(series);
	ip->instid = sdsdup(instid);
	ip->name = sdsdup(instname);
	dp->insts = ip;
    }
}

static void
series_del_insts(series_data *dp)
{
    series_insts	*ip;
    int			i;

    for (i = 0; i < dp->ninsts; i++) {
	ip = &dp->insts[i];
	sdsfree(ip->series);
	sdsfree(ip->instid);
	sdsfree(ip->name);
    }
    free(dp->insts);
    dp->ninsts = 0;
}

static int
series_next(series_data *dp, sds sid)
{
    if (strncmp(dp->series, sid, sdslen(sid)) != 0) {
	if (dp->flags & PMSERIES_NEED_EOL) {
	    dp->flags &= ~PMSERIES_NEED_EOL;
	    putc('\n', stdout);
	}
	dp->series = sdscpylen(dp->series, sid, sdslen(sid));
	if (dp->source)
	    sdsclear(dp->source);
	if (dp->type)
	    sdsclear(dp->type);
	series_del_insts(dp);
	return 1;
    }
    return 0;
}

static void
on_series_info(pmloglevel level, sds message, void *arg)
{
    series_data		*dp = (series_data *)arg;
    int			colour = (dp->flags & PMSERIES_COLOUR);
    FILE		*fp = (level == PMLOG_INFO) ? stdout : stderr;

    return pmLogLevelPrint(fp, level, message, colour);
}

static const char *
series_type_phrase(const char *type_word)
{
    if (strcasecmp(type_word, "32") == 0)
	return "32-bit int";
    if (strcasecmp(type_word, "64") == 0)
	return "64-bit int";
    if (strcasecmp(type_word, "U32") == 0)
	return "32-bit unsigned int";
    if (strcasecmp(type_word, "U64") == 0)
	return "64-bit unsigned int";
    if (strcasecmp(type_word, "FLOAT") == 0)
	return "float";
    if (strcasecmp(type_word, "DOUBLE") == 0)
	return "double";
    if (strcasecmp(type_word, "STRING") == 0)
	return "string";
    if (strcasecmp(type_word, "AGGREGATE") == 0)
	return "aggregate";
    if (strcasecmp(type_word, "AGGREGATE_STATIC") == 0)
	return "aggregate static";
    if (strcasecmp(type_word, "EVENT") == 0)
	return "event record array";
    if (strcasecmp(type_word, "HIGHRES_EVENT") == 0)
	return "highres event record array";
    if (strcasecmp(type_word, "NO_SUPPORT") == 0)
	return "Not Supported";
    return "???";
}

/*
 *  pmSeriesLoad calls and associated callbacks
 */

static int
series_load(pmSeriesSettings *settings, sds query, series_flags flags)
{
    series_data		data;
    pmflags		meta = flags & PMSERIES_FAST? PMFLAG_METADATA : 0;

    series_data_init(&data, flags);
    pmSeriesLoad(settings, query, meta, (void *)&data);
    return data.status;
}

/*
 *  pmSeriesQuery calls and associated callbacks
 */

static int
on_series_match(pmSeriesID sid, void *arg)
{
    series_data		*dp = (series_data *)arg;

    if (series_next(dp, sid))
	printf("%s\n", sid);
    return 0;
}

static int
on_series_value(pmSeriesID sid, int nfields, sds *value, void *arg)
{
    series_insts	*ip;
    series_data		*dp = (series_data *)arg;
    sds			timestamp, series, data;
    int			need_free = 1;

    if (nfields < PMVALUE_MAXFIELD)
	return -EINVAL;
    timestamp = value[PMVALUE_TIMESTAMP];
    series = value[PMVALUE_SERIES];
    data = value[PMVALUE_DATA];

    if (series_next(dp, sid))
	printf("\n%s\n", sid);

    if (dp->type == NULL)
	dp->type = sdsempty();
    if (strncmp(dp->type, "AGGREGATE", sizeof("AGGREGATE")-1) == 0)
	data = sdscatrepr(sdsempty(), data, sdslen(data));
    else if (strncmp(dp->type, "STRING", sizeof("STRING")-1) == 0)
	data = sdscatfmt(sdsempty(), "\"%S\"", data);
    else
	need_free = 0;

    if (sdslen(series) == 0)
	printf("    [%s] %s\n", timestamp, data);
    else if ((ip = series_get_inst(dp, series)) == NULL)
	printf("    [%s] %s %s\n", timestamp, data, series);
    else
	printf("    [%s] %s \"%s\"\n", timestamp, data, ip->name);

    if (need_free)
	sdsfree(data);
    return 0;
}

static int
series_query(pmSeriesSettings *settings, sds query, series_flags flags)
{
    series_data		data;
    pmflags		meta = flags & PMSERIES_FAST? PMFLAG_METADATA : 0;

    series_data_init(&data, flags);
    pmSeriesQuery(settings, query, meta, (void *)&data);
    return data.status;
}

/*
 *  pmSeriesDescs calls, callbacks
 */

static int
on_series_desc(pmSeriesID series, int nfields, sds *desc, void *arg)
{
    series_data		*dp = (series_data *)arg;
    static const char	*unknown = "???";
    unsigned int	domain, cluster, item, serial;
    pmInDom		indom_value = PM_IN_NULL;
    pmID		pmid_value = PM_ID_NULL;
    sds			indom, pmid, semantics, source, type, units;

    if (nfields < PMDESC_MAXFIELD)
	return -EINVAL;
    indom = desc[PMDESC_INDOM];
    pmid = desc[PMDESC_PMID];
    semantics = desc[PMDESC_SEMANTICS];
    source = desc[PMDESC_SOURCE];
    type = desc[PMDESC_TYPE];
    units = desc[PMDESC_UNITS];

    if (series_next(dp, series)) {
	dp->source = sdsnewlen(source, sdslen(source));
	dp->type = sdsnewlen(type, sdslen(type));
	printf("\n%s", series);
    } else {
	printf("   ");
    }

    if (sscanf(pmid, "%u.%u.%u", &domain, &cluster, &item) == 3)
	pmid_value = pmID_build(domain, cluster, item);
    else if (strcmp(pmid, "none") == 0)
	pmid = "PM_ID_NULL";
    if (sscanf(indom, "%u.%u", &domain, &serial) == 2)
	indom_value = pmInDom_build(domain, serial);
    else if (strcmp(indom, "none") == 0)
	indom = "PM_INDOM_NULL";

    printf(" PMID: %s", pmid);
    if (dp->flags & PMSERIES_FULLPMID)
	printf(" = %u = 0x%x", pmid_value, pmid_value);
    printf("\n");
    printf("    Data Type: %s", series_type_phrase(type));
    if (strcmp(type, unknown) == 0)
	printf(" (%s)", type);
    printf("  InDom: %s", indom);
    if (dp->flags & PMSERIES_FULLINDOM)
	printf(" = %u =", indom_value);
    printf(" 0x%x\n", indom_value);
    printf("    Semantics: %s", semantics);
    if (strcmp(semantics, unknown) == 0)
	printf(" (%s)", semantics);
    printf("  Units: %s\n", *units == '\0' ? "none" : units);
    if (dp->flags & PMSERIES_SOURCEID)
	printf("    Source: %s\n", source);

    return 0;
}

static int
series_descriptor(pmSeriesSettings *settings, sds query, series_flags flags)
{
    int			nseries, sts;
    char		msg[PM_MAXERRMSGLEN];
    pmSeriesID		*series = NULL;
    series_data		data;

    if ((nseries = sts = series_split(query, &series)) < 0) {
	fprintf(stderr, "%s: no series identifiers in string '%s': %s\n",
		pmGetProgname(), query, pmErrStr_r(sts, msg, sizeof(msg)));
	return 2;
    }
    series_data_init(&data, flags);
    pmSeriesDescs(settings, nseries, series, (void *)&data);
    series_free(nseries, series);
    return data.status;
}

/*
 *  pmSeriesInstances calls, callbacks
 */

static int
on_series_instname(pmSeriesID series, sds name, void *arg)
{
    series_data		*dp = (series_data *)arg;

    if (series == NULL)	{	/* dumping all instance names */
	printf("%s\n", name);
    } else if (series_next(dp, series)) {
	printf("\n%s\n", series);
	printf("    Instances: %s", name);
	dp->flags |= PMSERIES_NEED_EOL;
    } else {
	printf(", %s", name);
	dp->flags |= PMSERIES_NEED_EOL;
    }
    return 0;
}

static int
on_series_instance(pmSeriesID sid, int nfields, sds *inst, void *arg)
{
    series_insts	*ip;
    series_data		*dp = (series_data *)arg;
    sds			instid, instname, series;

    if (nfields < PMINST_MAXFIELD)
	return -EINVAL;
    instid = inst[PMINST_INSTID];
    instname = inst[PMINST_NAME];
    series = inst[PMINST_SERIES];

    if (series_next(dp, sid) && !(dp->flags & PMSERIES_SERIESID))
	printf("\n%s\n", sid);

    if ((ip = series_get_inst(dp, series)) == NULL)
	series_add_inst(dp, instid, instname, series);
    if (!(dp->flags & PMSERIES_SERIESID))
	printf("    inst [%s or \"%s\"] series %s", instid, instname, series);
    return 0;
}

static int
series_instance(pmSeriesSettings *settings, sds query, series_flags flags)
{
    int			nseries, sts;
    char		msg[PM_MAXERRMSGLEN];
    pmSeriesID		*series = NULL;
    series_data		data;

    if ((nseries = sts = series_split(query, &series)) < 0) {
	fprintf(stderr, "%s: cannot find series identifiers in '%s': %s\n",
		pmGetProgname(), query, pmErrStr_r(sts, msg, sizeof(msg)));
	return 1;
    }
    series_data_init(&data, flags);
    pmSeriesInstances(settings, nseries, series, (void *)&data);
    series_free(nseries, series);
    return data.status;
}

/*
 *  pmSeriesLabels calls, callbacks
 */

static int
on_series_label(pmSeriesID series, sds label, void *arg)
{
    series_data		*dp = (series_data *)arg;

    if (series == NULL)	{	/* dumping all label names */
	printf("%s\n", label);
    } else if (series_next(dp, series)) {
	printf("\n%s\n", series);
	printf("    Labels: %s", label);
	dp->flags |= PMSERIES_NEED_EOL;
    } else {
	printf(", %s", label);
	dp->flags |= PMSERIES_NEED_EOL;
    }
    return 0;
}

static int
on_series_labelset(pmSeriesID series, int nfields, sds *label, void *arg)
{
    series_insts	*ip;
    series_data		*dp = (series_data *)arg;
    sds			name, value;

    if (nfields < PMLABEL_MAXFIELD)
	return -EINVAL;
    name = label[PMLABEL_NAME];
    value = label[PMLABEL_VALUE];

    if (series_next(dp, series) && !(dp->flags & PMSERIES_SERIESID))
	printf("\n%s\n", series);

    if ((ip = series_get_inst(dp, series)) == NULL)
	printf("    inst [%s or \"%s\"] label {\"%s\":%s}\n",
		    ip->instid, ip->name, name, value);
    else
	printf("    label {\"%s\":%s}\n", name, value);
    return 0;
}

static int
series_labels(pmSeriesSettings *settings, sds query, series_flags flags)
{
    int			nseries, sts;
    char		msg[PM_MAXERRMSGLEN];
    pmSeriesID		*series = NULL;
    series_data		data;

    if ((nseries = sts = series_split(query, &series)) < 0) {
	fprintf(stderr, "%s: cannot find series identifiers in '%s': %s\n",
		pmGetProgname(), query, pmErrStr_r(sts, msg, sizeof(msg)));
	return 2;
    }
    series_data_init(&data, flags);
    pmSeriesLabels(settings, nseries, series, (void *)&data);
    series_free(nseries, series);
    return data.status;
}

/*
 *  pmSeriesMetrics calls, callbacks
 */

static int
on_series_metric(pmSeriesID series, sds name, void *arg)
{
    series_data		*dp = (series_data *)arg;

    if (series == NULL)	{	/* dumping all metric names */
	printf("%s\n", name);
    } else if (series_next(dp, series)) {
	printf("\n%s\n", series);
	printf("    Metric: %s", name);
	dp->flags |= PMSERIES_NEED_EOL;
    } else {
	printf(", %s", name);
	dp->flags |= PMSERIES_NEED_EOL;
    }
    return 0;
}

static int
series_metric(pmSeriesSettings *settings, sds query, series_flags flags)
{
    int			nseries, sts;
    char		msg[PM_MAXERRMSGLEN];
    pmSeriesID		*series = NULL;
    series_data		data;

    if ((nseries = sts = series_split(query, &series)) < 0) {
	fprintf(stderr, "%s: cannot find series identifiers in '%s': %s\n",
		pmGetProgname(), query, pmErrStr_r(sts, msg, sizeof(msg)));
	return 2;
    }
    series_data_init(&data, flags);
    pmSeriesMetrics(settings, nseries, series, (void *)&data);
    series_free(nseries, series);
    return data.status;
}

/*
 *  pmSeriesSources calls, callbacks
 */

static int
on_series_context(pmSeriesID series, sds name, void *arg)
{
    series_data		*dp = (series_data *)arg;

    if (series == NULL)	{	/* dumping all metric sources */
	printf("%s\n", name);
    } else if (series_next(dp, series)) {
	printf("\n%s\n", series);
	printf("    Source: %s", name);
	dp->flags |= PMSERIES_NEED_EOL;
    } else {
	printf(", %s", name);
	dp->flags |= PMSERIES_NEED_EOL;
    }
    return 0;
}

static int
on_series_source(pmSourceID source, int nfields, sds *context, void *arg)
{
    sds			name, sid, type;

    (void)arg;
    if (nfields < PMSOURCE_MAXFIELD)
	return -EINVAL;
    name = context[PMSOURCE_NAME];
    sid = context[PMSOURCE_SERIES];
    type = context[PMSOURCE_TYPE];

    printf("    %s source %s (%s)\n", type, name, sid);
    return 0;
}

static int
series_source(pmSeriesSettings *settings, sds query, series_flags flags)
{
    int			nsources, sts;
    char		msg[PM_MAXERRMSGLEN];
    pmSourceID		*sources = NULL;
    series_data		data;

    if ((nsources = sts = series_split(query, &sources)) < 0) {
	fprintf(stderr, "%s: cannot find source identifiers in '%s': %s\n",
		pmGetProgname(), query, pmErrStr_r(sts, msg, sizeof(msg)));
	return 2;
    }
    series_data_init(&data, flags);
    pmSeriesSources(settings, nsources, sources, (void *)&data);
    series_free(nsources, sources);
    return data.status;
}

/*
 * Finishing up interacting with the library via callbacks
 */

void
on_series_done(int sts, void *arg)
{
    series_data		*dp = (series_data *)arg;
    char		msg[PM_MAXERRMSGLEN];

    if (dp->flags & PMSERIES_NEED_EOL) {
	dp->flags &= ~PMSERIES_NEED_EOL;
	putc('\n', stdout);
    }
    if (sts < 0) {
	fprintf(stderr, "%s: %s\n", pmGetProgname(),
			pmErrStr_r(sts, msg, sizeof(msg)));
    }
    dp->status = (sts < 0) ? 1 : 0;
}

static int
series_meta_all(pmSeriesSettings *settings, sds query, series_flags flags)
{
    int			nseries, sts, i;
    char		msg[PM_MAXERRMSGLEN];
    pmSeriesID		*series = NULL;

    if ((nseries = sts = series_split(query, &series)) < 0) {
	fprintf(stderr, "%s: cannot find series identifiers in '%s': %s\n",
		pmGetProgname(), query, pmErrStr_r(sts, msg, sizeof(msg)));
	return 1;
    }
    for (i = sts = 0; i < nseries; i++) {
	series_data	data = {.series = series[i], .flags = flags};

	pmSeriesDescs(settings, 1, &series[i], (void *)&data);
	pmSeriesInstances(settings, 1, &series[i], (void *)&data);
	pmSeriesLabels(settings, 1, &data.source, (void *)&data);
	pmSeriesSources(settings, 1, &series[i], (void *)&data);
	pmSeriesMetrics(settings, 1, &series[i], (void *)&data);
	if (data.status)
	    sts = data.status;
    }
    series_free(nseries, series);
    return sts ? 1 : 0;
}

static int
pmseries_overrides(int opt, pmOptions *opts)
{
    return (opt == 'a' || opt == 'L' || opt == 's' || opt == 'S');
}

static pmSeriesSettings settings = {
    .on_match		= on_series_match,
    .on_desc		= on_series_desc,
    .on_inst		= on_series_instance,
    .on_instname	= on_series_instname,
    .on_metric		= on_series_metric,
    .on_context		= on_series_context,
    .on_source		= on_series_source,
    .on_value		= on_series_value,
    .on_label		= on_series_label,
    .on_labelset	= on_series_labelset,
    .on_info		= on_series_info,
    .on_done		= on_series_done,
};

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General Options"),
    { "all", 0, 'a', 0, "report all metadata (-dilmsS) for time series" },
    { "contexts", 0, 'c', 0, "report context names for a time series source" },
    { "desc", 0, 'd', 0, "metric descriptor for time series" },
    { "instance", 0, 'i', 0, "instance identifiers for time series" },
    { "labels", 0, 'l', 0, "list all labels for time series" },
    { "load", 0, 'L', 0, "load time series values and metadata" },
    { "metrics", 0, 'm', 0, "metric names for time series" },
    { "query", 0, 'q', 0, "perform a time series query (default)" },
    PMAPI_OPTIONS_HEADER("Reporting Options"),
    PMOPT_DEBUG,
    { "fast", 0, 'F', 0, "query or load series metadata, not values" },
    { "fullpmid", 0, 'M', 0, "print PMID in verbose format" },
    { "fullindom", 0, 'I', 0, "print InDom in verbose format" },
    { "source", 0, 'S', 0, "print the source for each time series" },
    { "series", 0, 's', 0, "print the series for each instance" },
    PMOPT_VERSION,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_BOUNDARIES,
    .short_options = "acdD:FiIlLmMqsSV?",
    .long_options = longopts,
    .short_usage = "[options] [query ... | series ... | source ...]",
    .override = pmseries_overrides,
};

typedef int (*series_command)(pmSeriesSettings *, sds, series_flags);

int
main(int argc, char *argv[])
{
    sds			query;
    int			c, sts;
    series_flags	flags = 0;
    series_command	command = series_query;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'a':	/* command line contains series identifiers */
	    command = series_meta_all;
	    flags |= (PMSERIES_SOURCEID | PMSERIES_SERIESID);
	    break;

	case 'c':	/* command line contains source identifiers */
	    command = series_source;
	    break;

	case 'd':	/* command line contains series identifiers */
	    command = series_descriptor;
	    break;

	case 'F':	/* perform metadata-only --load, or --query */
	    flags |= PMSERIES_FAST;
	    break;

	case 'i':	/* command line contains series identifiers */
	    command = series_instance;
	    break;

	case 'I':	/* full form InDom reporting, ala pminfo -I */
	    flags |= PMSERIES_FULLINDOM;
	    break;

	case 'l':	/* command line contains series identifiers */
	    command = series_labels;
	    break;

	case 'L':	/* command line contains source load string */
	    command = series_load;
	    break;

	case 'm':	/* command line contains series identifiers */
	    command = series_metric;
	    break;

	case 'M':	/* full form PMID reporting, ala pminfo -M */
	    flags |= PMSERIES_FULLPMID;
	    break;

	case 'q':	/* command line contains query string */
	    command = series_query;
	    break;

	case 'S':	/* report source identifiers, ala pminfo -S */
	    flags |= PMSERIES_SOURCEID;
	    break;

	case 's':	/* report series identifiers, ala pminfo -s */
	    flags |= PMSERIES_SERIESID;
	    break;

	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.optind == argc)
	opts.errors++;

    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT)) {
	sts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(sts);
    }

    if (pmLogLevelIsTTY())
	flags |= PMSERIES_COLOUR;

    query = sdsjoin(&argv[opts.optind], argc - opts.optind, " ");
    sts = command(&settings, query, flags);
    sdsfree(query);

    return sts;
}
