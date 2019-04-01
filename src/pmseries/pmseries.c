/*
 * Copyright (c) 2017-2019 Red Hat.
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

#include "pmwebapi.h"
#include <uv.h>

typedef enum series_flags {
    PMSERIES_COLOUR	= (1<<0),	/* report in colour if possible */
    PMSERIES_FAST	= (1<<1),	/* load only the metric metadata */
    PMSERIES_FULLINDOM	= (1<<2),	/* report with pminfo(1) -I info */
    PMSERIES_FULLPMID	= (1<<3),	/* report with pminfo(1) -M info */
    PMSERIES_SERIESID	= (1<<4),	/* report with pminfo(1) -s info */
    PMSERIES_NEED_EOL	= (1<<5),	/* need to eol-terminate output */
    PMSERIES_NEED_COMMA	= (1<<6),	/* need comma line separation */
    PMSERIES_INSTLABELS	= (1<<7),	/* labels by instance identifier */
    PMSERIES_ONLY_NAMES	= (1<<8),	/* report on label names only */
    PMSERIES_NEED_DESCS	= (1<<9),	/* output requires descs lookup */
    PMSERIES_NEED_INSTS	= (1<<10),	/* output requires insts lookup */

    PMSERIES_OPT_ALL	= (1<<16),	/* -a, --all option */
    PMSERIES_OPT_SOURCE = (1<<17),	/* -S, --source option */
    PMSERIES_OPT_DESC	= (1<<18),	/* -d, --desc option */
    PMSERIES_OPT_INSTS	= (1<<19),	/* -i, --instances option */
    PMSERIES_OPT_LABELS	= (1<<20),	/* -l, --labels option */
    PMSERIES_OPT_LOAD	= (1<<21),	/* -L, --load option */
    PMSERIES_OPT_METRIC	= (1<<22),	/* -m, --metric option */
    PMSERIES_OPT_QUERY	= (1<<23),	/* -q, --query option (default) */
} series_flags;

#define PMSERIES_META_OPTS	(PMSERIES_OPT_DESC | PMSERIES_OPT_INSTS | \
				 PMSERIES_OPT_LABELS | PMSERIES_OPT_METRIC)

struct series_data;
typedef void (*series_callback)(struct series_data *, void *);

typedef struct series_entry {
    series_callback	func;
    void		*arg;
    struct series_entry	*next;
} series_entry;

typedef struct series_command {
    int			nseries;
    int			nsource;
    sds			*series;
    sds			*source;
    sds			pattern;	/* glob pattern for string matches */
} series_command;

typedef struct series_label {
    sds			name;
    sds			value;
} series_label;

typedef struct series_inst {
    sds			instid;		/* internal instance identifier */
    sds			name;		/* external instance identifier */
    sds			series;		/* instance series back-pointer */
    unsigned int	nlabels;	/* number of instance labels */
    series_label	*labels;	/* series (instance) labels */
} series_inst;

typedef struct series_data {
    sds			query;
    uv_loop_t		*loop;
    pmSeriesSettings	settings;
    series_command	args;		/* detailed command line arguments */
    series_entry	*head;		/* list of function callbacks */
    series_entry	*next;		/* next function callback to issue */
    series_flags	flags;		/* flags affecting reporting */
    int			status;		/* command exit status */

    pmSID		series;		/* current time series */
    pmSID		source;		/* current time series source */
    sds			type;		/* current time series (value) type */

    unsigned int	nlabels;	/* number of metric labels */
    series_label	*labels;	/* series (metric) labels */

    unsigned int        ninsts;		/* instances for the current series */
    series_inst		*insts;		/* instances for the current series */
    pmSID		*iseries;	/* series identifiers for instances */
} series_data;

static void on_series_done(int, void *);

static series_data *
series_data_init(series_flags flags, sds query)
{
    series_data		*dp = calloc(1, sizeof(series_data));

    if (dp == NULL) {
	fprintf(stderr, "%s: out of memory allocating series data\n",
			pmGetProgname());
	exit(127);
    }
    dp->query = query;
    dp->flags = flags;
    dp->series = sdsempty();
    dp->source = sdsempty();
    return dp;
}

static void
series_add_inst(series_data *dp, pmSID series, sds instid, sds instname)
{
    series_inst		*ip;
    pmSID		*isp;
    size_t		bytes;

    bytes = sizeof(sds) * (dp->ninsts + 1);
    if ((isp = realloc(dp->iseries, bytes)) != NULL) {
	bytes = sizeof(series_inst) * (dp->ninsts + 1);
	if ((ip = realloc(dp->insts, bytes)) != NULL) {
	    dp->insts = ip;
	    ip += dp->ninsts;
	    memset(ip, 0, sizeof(series_inst));
	    ip->instid = sdsdup(instid);
	    ip->name = sdsdup(instname);
	    ip->series = sdsdup(series);
	    dp->iseries = isp;
	    isp += dp->ninsts;
	    *isp = ip->series;
	    dp->ninsts++;
	} else {
	    fprintf(stderr, "%s: failed to allocate %" FMT_INT64 " bytes\n",
		    pmGetProgname(), (__int64_t)bytes);
	    free(isp);
	}
    } else {
	fprintf(stderr, "%s: failed to allocate %" FMT_INT64 " bytes\n",
		pmGetProgname(), (__int64_t)bytes);
    }
}

static void
series_del_insts(series_data *dp)
{
    series_inst		*ip;
    int			i;

    for (i = 0; i < dp->ninsts; i++) {
	ip = &dp->insts[i];
	sdsfree(ip->series);
	sdsfree(ip->instid);
	sdsfree(ip->name);
    }
    if (dp->insts) {
	free(dp->insts);
	dp->insts = NULL;
    }
    dp->ninsts = 0;
}

static void
series_free(int nseries, pmSID *series)
{
    if (nseries) {
	while (--nseries)
	    sdsfree(series[nseries]);
	free(series);
    }
}

static void
series_data_free(series_data *dp)
{
    sdsfree(dp->query);

    if (dp->args.nsource)
	series_free(dp->args.nsource, dp->args.source);
    if (dp->args.nseries)
	series_free(dp->args.nseries, dp->args.series);
    sdsfree(dp->series);
    sdsfree(dp->source);
    if (dp->type)
	sdsfree(dp->type);
    if (dp->args.pattern)
	sdsfree(dp->args.pattern);
    series_del_insts(dp);
}

static int
series_split(sds string, pmSID **series)
{
    size_t		length;
    int			nseries = 0;

    if (!string || !sdslen(string))
	return 0;
    length = sdslen(string);
    if ((*series = sdssplitlen(string, length, ",", 1, &nseries)) == NULL)
	return -ENOMEM;
    return nseries;
}

static series_inst *
series_get_inst(series_data *dp, sds series)
{
    series_inst		*ip;
    int			i;

    for (i = 0; i < dp->ninsts; i++) {
	ip = &dp->insts[i];
	if (sdscmp(series, ip->series) == 0)
	    return ip;
    }
    return NULL;
}

static int
series_next(series_data *dp, sds sid)
{
    if (sdscmp(dp->series, sid) != 0) {
	dp->flags &= ~PMSERIES_NEED_COMMA;
	if (dp->flags & PMSERIES_NEED_EOL) {
	    dp->flags &= ~PMSERIES_NEED_EOL;
	    putc('\n', stdout);
	}
	dp->series = sdscpy(dp->series, sid);
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
on_series_info(pmLogLevel level, sds message, void *arg)
{
    series_data		*dp = (series_data *)arg;
    int			colour = (dp->flags & PMSERIES_COLOUR);
    FILE		*fp = (level == PMLOG_INFO) ? stdout : stderr;

    if (level >= PMLOG_ERROR)
	dp->status = 1;	/* exit pmseries with error */
    if (level >= PMLOG_INFO || pmDebugOptions.series)
	pmLogLevelPrint(fp, level, message, colour);
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

static void
series_load(series_data *dp)
{
    pmSeriesFlags	meta = dp->flags & PMSERIES_FAST ?
				PM_SERIES_FLAG_METADATA : 0;
    int			sts;

    if ((sts = pmSeriesLoad(&dp->settings, dp->query, meta, dp)) < 0)
	on_series_done(sts, dp);
}

static int
on_series_match(pmSID sid, void *arg)
{
    series_data		*dp = (series_data *)arg;

    if (series_next(dp, sid))
	printf("%s\n", sid);
    return 0;
}

static int
on_series_value(pmSID sid, pmSeriesValue *value, void *arg)
{
    series_data		*dp = (series_data *)arg;
    series_inst		*ip;
    sds			timestamp, series, data;
    int			need_free = 1;

    timestamp = value->timestamp;
    series = value->series;
    data = value->data;

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

    if (sdscmp(series, sid) == 0)
	printf("    [%s] %s\n", timestamp, data);
    else if ((ip = series_get_inst(dp, series)) == NULL)
	printf("    [%s] %s %s\n", timestamp, data, series);
    else
	printf("    [%s] %s \"%s\"\n", timestamp, data, ip->name);

    if (need_free)
	sdsfree(data);
    return 0;
}

static void
series_query(series_data *dp)
{
    pmSeriesFlags	meta = dp->flags & PMSERIES_FAST ?
		   		PM_SERIES_FLAG_METADATA : 0;
    int			sts;

    if ((sts = pmSeriesQuery(&dp->settings, dp->query, meta, dp)) < 0)
	on_series_done(sts, dp);
}

static int
on_series_desc(pmSID series, pmSeriesDesc *desc, void *arg)
{
    series_data		*dp = (series_data *)arg;
    static const char	*unknown = "???";
    unsigned int	domain, cluster, item, serial;
    pmInDom		indom_value = PM_IN_NULL;
    pmID		pmid_value = PM_ID_NULL;
    sds			indom, pmid, semantics, source, type, units;

    indom = desc->indom;
    pmid = desc->pmid;
    semantics = desc->semantics;
    source = desc->source;
    type = desc->type;
    units = desc->units;

    if (series_next(dp, series)) {
	dp->type = sdsnewlen(type, sdslen(type));
	printf("\n%s", series);
    } else {
	printf("   ");
    }
    dp->source = dp->source ?
		sdscpylen(dp->source, source, sdslen(source)) :
		sdsnewlen(source, sdslen(source));

    if (sscanf(pmid, "%u.%u.%u", &domain, &cluster, &item) == 3)
	pmid_value = pmID_build(domain, cluster, item);
    else if (strcmp(pmid, "none") == 0)
	pmid = "PM_ID_NULL";
    if (sscanf(indom, "%u.%u", &domain, &serial) == 2)
	indom_value = pmInDom_build(domain, serial);
    else if (strcmp(indom, "none") == 0)
	indom = "PM_INDOM_NULL";

    if (dp->flags & (PMSERIES_FULLPMID|PMSERIES_OPT_DESC))
	printf(" PMID: %s", pmid);
    if (dp->flags & PMSERIES_FULLPMID)
	printf(" = %u = 0x%x", pmid_value, pmid_value);
    if (dp->flags & (PMSERIES_FULLPMID|PMSERIES_OPT_DESC))
	printf("\n");
    if (dp->flags & PMSERIES_OPT_DESC) {
	printf("    Data Type: %s", series_type_phrase(type));
	if (strcmp(type, unknown) == 0)
	    printf(" (%s)", type);
    }
    if (dp->flags & (PMSERIES_FULLINDOM|PMSERIES_OPT_DESC))
	printf("  InDom: %s", indom);
    if (dp->flags & PMSERIES_FULLINDOM)
	printf(" = %u =", indom_value);
    if (dp->flags & (PMSERIES_FULLINDOM|PMSERIES_OPT_DESC))
	printf(" 0x%x\n", indom_value);
    if (dp->flags & PMSERIES_OPT_DESC) {
	printf("    Semantics: %s", semantics);
	if (strcmp(semantics, unknown) == 0)
	    printf(" (%s)", semantics);
	printf("  Units: %s\n", *units == '\0' ? "none" : units);
    }
    if (dp->flags & PMSERIES_SERIESID)
	printf("    Source: %s\n", source);
    dp->flags &= ~PMSERIES_NEED_EOL;

    return 0;
}

static int
on_series_instance(pmSID series, sds name, void *arg)
{
    series_data		*dp = (series_data *)arg;

    if (!(dp->flags & PMSERIES_OPT_INSTS))
	return 0;
    if (series == NULL)	{	/* report all instance names */
	printf("%s\n", name);
	return 0;
    }
    if (series_next(dp, series))
	printf("\n%s", series);
    if (dp->flags & PMSERIES_NEED_COMMA)
	printf(", %s", name);
    else
	printf("    Instances: %s", name);
    dp->flags |= (PMSERIES_NEED_EOL | PMSERIES_NEED_COMMA);
    return 0;
}

static int
on_series_inst(pmSID sid, pmSeriesInst *inst, void *arg)
{
    series_data		*dp = (series_data *)arg;
    sds			instid, instname, series;

    instid = inst->instid;
    instname = inst->name;
    series = inst->series;

    if (series_next(dp, sid) && (dp->flags & PMSERIES_OPT_INSTS))
	printf("\n%s\n", sid);
    if (series_get_inst(dp, series) == NULL)
	series_add_inst(dp, series, instid, instname);
    return 0;
}

static int
series_instance_compare(const void *a, const void *b)
{
    series_inst		*ap = (series_inst *)a;
    series_inst		*bp = (series_inst *)b;

    if (sdscmp(ap->instid, bp->instid) != 0)
	return (int)(atoll(ap->instid) - atoll(bp->instid));
    return strcmp(ap->name, bp->name);
}

static void
series_instance_names(series_data *dp, void *arg)
{
    series_inst		*ip;
    pmSID		*isp = dp->iseries;
    int			i;

    (void)arg;
    qsort(dp->insts, dp->ninsts, sizeof(series_inst), series_instance_compare);
    for (i = 0; i < dp->ninsts; i++) {
	ip = &dp->insts[i];
	if (dp->flags & PMSERIES_OPT_INSTS)
	    printf("    inst [%s or \"%s\"] series %s\n",
		    ip->instid, ip->name, ip->series);
	isp[i] = ip->series;
    }
    on_series_done(0, dp);
}

static int
series_labels_compare(const void *a, const void *b)
{
    series_label	*ap = (series_label *)a;
    series_label	*bp = (series_label *)b;

    return sdscmp(ap->name, bp->name);
}

static sds
series_labels_sort(sds s, unsigned int nlabels, series_label *labels)
{
    series_label	*lp;
    unsigned int	i;

    qsort(labels, nlabels, sizeof(series_label), series_labels_compare);

    s = sdscpylen(s, "{", 1);
    for (i = 0; i < nlabels; i++) {
	if (i > 0)
	    s = sdscat(s, ",");
	lp = &labels[i];
	s = sdscatfmt(s, "\"%S\":%S", lp->name, lp->value);
    }
    return sdscatfmt(s, "}");
}

static void
series_metric_labels(series_data *dp, void *arg)
{
    sds			labels;

    (void)arg;
    if (!(dp->flags & PMSERIES_ONLY_NAMES)) {
	labels = series_labels_sort(sdsempty(), dp->nlabels, dp->labels);
	if (sdslen(labels) > 2)
	    printf("    labels %s\n", labels);
	sdsfree(labels);
    }
    on_series_done(0, dp);
}

static void
series_instance_labels(series_data *dp, void *arg)
{
    series_inst		*ip = NULL;
    sds			labels;
    unsigned int	i;

    if (!(dp->flags & PMSERIES_ONLY_NAMES)) {
	labels = sdsempty();
	for (i = 0; i < dp->ninsts; i++) {
	    ip = &dp->insts[i];
	    labels = series_labels_sort(labels, ip->nlabels, ip->labels);
	    printf("    inst [%s or \"%s\"] labels %s\n",
		    ip->instid, ip->name, labels);
	}
	sdsfree(labels);
    }
    on_series_done(0, dp);
}

static void
series_add_labels(sds name, sds value,
		unsigned int *nlabelsp, series_label **labelsp)
{
    unsigned int	nlabels = *nlabelsp;
    series_label	*p, *lp = *labelsp;
    size_t		bytes;

    bytes = sizeof(series_label) * (nlabels + 1);
    if ((p = realloc(lp, bytes)) != NULL) {
	lp = p;
	p += nlabels;
	p->name = sdsdup(name);
	p->value = sdsdup(value);

	*labelsp = lp;
	*nlabelsp = nlabels + 1;
    } else {
	fprintf(stderr, "%s: failed to allocate %" FMT_INT64 " bytes\n",
		pmGetProgname(), (__int64_t)bytes);
    }
}

static int
on_series_label(pmSID series, sds label, void *arg)
{
    series_data		*dp = (series_data *)arg;

    if (series == NULL) {	/* report all label names */
	printf("%s\n", label);
	return 0;
    }
    if (!(dp->flags & PMSERIES_ONLY_NAMES))
	return 0;
    if (series_next(dp, series))
	printf("\n%s", series);
    if (dp->flags & PMSERIES_NEED_COMMA)
	printf(", %s", label);
    else
	printf("    Labels: %s", label);
    dp->flags |= (PMSERIES_NEED_COMMA | PMSERIES_NEED_EOL);
    return 0;
}

static int
on_series_labelmap(pmSID series, pmSeriesLabel *label, void *arg)
{
    series_inst		*ip = NULL;
    series_data		*dp = (series_data *)arg;
    sds			name, value;

    if (dp->flags & PMSERIES_INSTLABELS) {
	if ((ip = series_get_inst(dp, series)) == NULL)
	    return 0;
    } else if (series_next(dp, series) && !(dp->flags & PMSERIES_SERIESID)) {
	printf("\n%s\n", series);
    }

    name = label->name;
    value = label->value;
    if (dp->flags & PMSERIES_INSTLABELS)
	series_add_labels(name, value, &ip->nlabels, &ip->labels);
    else
	series_add_labels(name, value, &dp->nlabels, &dp->labels);
    return 0;
}

static int
on_series_metric(pmSID series, sds name, void *arg)
{
    series_data		*dp = (series_data *)arg;

    if (series == NULL) {	/* report all metric names */
	printf("%s\n", name);
	return 0;
    }
    if (series_next(dp, series))
	printf("\n%s", series);
    if (dp->flags & PMSERIES_NEED_COMMA)
	printf(", %s", name);
    else
	printf("    Metric: %s", name);
    dp->flags |= (PMSERIES_NEED_COMMA | PMSERIES_NEED_EOL);
    return 0;
}

static int
on_series_context(pmSID source, sds name, void *arg)
{
    series_data		*dp = (series_data *)arg;

    if (source == NULL)	{	/* report all metric sources */
	printf("%s\n", name);
	return 0;
    }
    dp->source = dp->source ?
		sdscpylen(dp->source, source, sdslen(source)) :
		sdsnewlen(source, sdslen(source));
    if (!dp->source || sdscmp(dp->source, source) != 0)
	printf("\n%s", source);
    if (dp->flags & PMSERIES_NEED_COMMA)
	printf(", %s", name);
    else
	printf("    Context: %s", name);
    dp->flags |= (PMSERIES_NEED_COMMA | PMSERIES_NEED_EOL);
    return 0;
}

static void
series_source(series_data *dp)
{
    int			nsources, sts;
    char		msg[PM_MAXERRMSGLEN];
    pmSID		*sources = NULL;

    if ((nsources = sts = series_split(dp->query, &sources)) < 0) {
	fprintf(stderr, "%s: cannot find source identifiers in '%s': %s\n",
		pmGetProgname(), dp->query, pmErrStr_r(sts, msg, sizeof(msg)));
    } else {
	if (nsources) {
	    dp->args.nsource = nsources;
	    dp->args.source = sources;
	} else {
	    sources = &dp->args.pattern;
	}
	if ((sts = pmSeriesSources(&dp->settings, nsources, sources, dp)) < 0)
	    on_series_done(sts, dp);
    }
}

/*
 * Finishing up interacting with the library via callbacks
 */

static void
on_series_done(int sts, void *arg)
{
    series_callback	func;
    series_entry	*entry;
    series_data		*dp = (series_data *)arg;
    char		msg[PM_MAXERRMSGLEN];

    if (dp->flags & PMSERIES_NEED_EOL) {
	dp->flags &= ~PMSERIES_NEED_EOL;
	putc('\n', stdout);
    }
    if (dp->status == 0 && sts < 0) {
	fprintf(stderr, "%s: %s\n", pmGetProgname(),
			pmErrStr_r(sts, msg, sizeof(msg)));
	dp->status = 1;
    }

    if ((entry = dp->next) != NULL) {
	dp->next = entry->next;
	func = entry->func;
	arg = entry->arg;
	func(dp, arg);
    } else {
	pmSeriesClose(&dp->settings.module);
	series_data_free(dp);
    }
}

/* append the given function pointer into the callback chain */
static void
series_link_report(series_data *dp, series_callback func, void *arg)
{
    series_entry	*tail, *entry = calloc(1, sizeof(series_entry));

    if (entry == NULL) {
	fprintf(stderr, "%s: out of memory allocating callback chain\n",
			pmGetProgname());
	exit(127);
    }
    entry->func = func;
    entry->arg = arg;

    if (dp->head == NULL) {
	dp->head = dp->next = entry;
    } else {
	tail = dp->head;
	while (tail->next != NULL)
	    tail = tail->next;
	tail->next = entry;
    }
}

static void
series_report_header(series_data *dp, void *arg)
{
    if (arg != NULL && series_next(dp, (pmSID)arg))
	printf("\n%s\n", (pmSID)arg);
    on_series_done(0, dp);
}

static void
series_done_report(series_data *dp, void *arg)
{
    (void)arg;
    dp->flags &= ~PMSERIES_NEED_COMMA;
    on_series_done(0, dp);
}

static void
series_report_footer(series_data *dp, void *arg)
{
    (void)arg;
    dp->flags &= ~PMSERIES_NEED_EOL;
    on_series_done(0, dp);
}

static void
series_desc_report(series_data *dp, void *arg)
{
    pmSID	sid = (pmSID)arg;
    int		sts;

    if ((sts = pmSeriesDescs(&dp->settings, sid? 1 : 0, &sid, dp)) < 0)
	on_series_done(sts, dp);
}

/* pass series identifier (one count) or pattern (zero count) */
#define SERIES_PARAMS(dp, arg, count, param) \
	if (arg) { count = 1; param = arg; } \
	else { count = 0; param = dp->args.pattern; }

static void
series_source_report(series_data *dp, void *arg)
{
    sds		param;
    int		sts, count;

    SERIES_PARAMS(dp, arg, count, param);
    if ((sts = pmSeriesSources(&dp->settings, count, &param, dp)) < 0)
	on_series_done(sts, dp);
}

static void
series_metric_report(series_data *dp, void *arg)
{
    sds		param;
    int		sts, count;

    SERIES_PARAMS(dp, arg, count, param);
    if ((sts = pmSeriesMetrics(&dp->settings, count, &param, dp)) < 0)
	on_series_done(sts, dp);
}

static void
series_labels_report(series_data *dp, void *arg)
{
    sds		param;
    int		sts, count;

    SERIES_PARAMS(dp, arg, count, param);
    if ((sts = pmSeriesLabels(&dp->settings, count, &param, dp)) < 0)
	on_series_done(sts, dp);
}

static void
series_instances_report(series_data *dp, void *arg)
{
    sds		param;
    int		sts, count;

    SERIES_PARAMS(dp, arg, count, param);
    if ((sts = pmSeriesInstances(&dp->settings, count, &param, dp)) < 0)
	on_series_done(sts, dp);
}

static void
series_instlabels_report(series_data *dp, void *arg)
{
    int		sts;

    (void)arg;
    if (dp->ninsts > 0) {
	dp->flags |= PMSERIES_INSTLABELS;
	sts = pmSeriesLabels(&dp->settings, dp->ninsts, dp->iseries, dp);
	if (sts < 0)
	    on_series_done(sts, dp);
    } else {
	/* nothing to do - move on to next command handler */
	on_series_done(0, dp);
    }
}

/*
 * Setup the (async) callback chain that will produce the
 * pmseries output requested based on command line option
 * list presented.  This may be called iteratively to add
 * more series than just one, or may be called for *zero*
 * series in the 'report all metadata' modes.
 */
static void
series_data_report(series_data *dp, int nseries, pmSID series)
{
    series_link_report(dp, series_report_header, series);

    if (dp->flags & (PMSERIES_OPT_DESC|PMSERIES_NEED_DESCS)) {
	series_link_report(dp, series_desc_report, series);
	series_link_report(dp, series_done_report, series);
    }

    if (dp->flags & PMSERIES_OPT_SOURCE) {
	series_link_report(dp, series_source_report, series);
	series_link_report(dp, series_done_report, series);
    }

    if (dp->flags & PMSERIES_OPT_METRIC) {
	series_link_report(dp, series_metric_report, series);
	series_link_report(dp, series_done_report, series);
    }

    if (dp->flags & PMSERIES_OPT_LABELS) {
	series_link_report(dp, series_labels_report, series);
	series_link_report(dp, series_metric_labels, series);
	series_link_report(dp, series_done_report, series);
    }

    if (dp->flags & (PMSERIES_OPT_INSTS|PMSERIES_NEED_INSTS)) {
	series_link_report(dp, series_instances_report, series);
	series_link_report(dp, series_instance_names, series);
	series_link_report(dp, series_done_report, series);
    }

    /* report per-instance label information */
    if ((dp->flags & PMSERIES_OPT_LABELS) && nseries != 0) {
	series_link_report(dp, series_instlabels_report, series);
	series_link_report(dp, series_instance_labels, series);
	series_link_report(dp, series_done_report, series);
    }

    series_link_report(dp, series_report_footer, series);
}

static void
series_report(series_data *dp)
{
    int			nseries, sts, i;
    char		msg[PM_MAXERRMSGLEN];
    pmSID		*series = NULL;
    series_entry	*entry;

    if ((nseries = sts = series_split(dp->query, &series)) < 0) {
	fprintf(stderr, "%s: no series identifiers in string '%s': %s\n",
		pmGetProgname(), dp->query, pmErrStr_r(sts, msg, sizeof(msg)));
    } else {
	dp->args.nseries = nseries;
	dp->args.series = series;

	if (nseries == 0)	/* report all names, instances, labels, ... */
	    series_data_report(dp, 0, NULL);
	for (i = 0; i < nseries; i++)
	    series_data_report(dp, 1, series[i]);
	entry = dp->head;
	entry->func(dp, entry->arg);
    }
}

static void
on_series_setup(void *arg)
{
    series_data		*dp = (series_data *)arg;
    series_flags	flags = dp->flags;
    sds			msg;

    if (pmDebugOptions.series) {
	msg = sdsnew("Connection established");
	on_series_info(PMLOG_DEBUG, msg, arg);
	sdsfree(msg);
    }

    if (flags & PMSERIES_OPT_LOAD)
	series_load(dp);
    else if (flags & PMSERIES_OPT_QUERY)
	series_query(dp);
    else if ((flags & PMSERIES_OPT_SOURCE) && !(flags & PMSERIES_META_OPTS))
	series_source(dp);
    else
	series_report(dp);
}

static void
pmseries_request(uv_timer_t *arg)
{
    uv_handle_t		*handle = (uv_handle_t *)arg;
    series_data		*dp = (series_data *)handle->data;

    pmSeriesSetup(&dp->settings.module, dp);
}

static int
pmseries_execute(series_data *dp)
{
    uv_loop_t		*loop = dp->loop;
    uv_timer_t		request;
    uv_handle_t		*handle = (uv_handle_t *)&request;

    handle->data = (void *)dp;
    uv_timer_init(loop, &request);
    uv_timer_start(&request, pmseries_request, 0, 0);
    uv_run(loop, UV_RUN_DEFAULT);
    return dp->status;
}

static int
pmseries_overrides(int opt, pmOptions *opts)
{
    switch (opt) {
    case 'a':
    case 'h':
    case 'g':
    case 'L':
    case 's':
    case 'S':
    case 'n':
    case 'p':
	return 1;
    }
    return 0;
}

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Connection Options"),
    { "config", 1, 'c', "FILE", "configuration file path"},
    { "host", 1, 'h', "HOST", "connect to Redis using given host name" },
    { "port", 1, 'p', "PORT", "connect to Redis using given TCP/IP port" },
    PMAPI_OPTIONS_HEADER("General Options"),
    { "load", 0, 'L', 0, "load time series values and metadata" },
    { "query", 0, 'q', 0, "perform a time series query (default)" },
    PMOPT_VERSION,
    PMOPT_DEBUG,
    PMOPT_HELP,
    PMAPI_OPTIONS_HEADER("Reporting Options"),
    { "all", 0, 'a', 0, "report all metadata (-dilms) for time series" },
    { "desc", 0, 'd', 0, "metric descriptor for time series" },
    { "fullindom", 0, 'I', 0, "print InDom in verbose format" },
    { "instances", 0, 'i', 0, "report names for time series instances" },
    { "fast", 0, 'F', 0, "query or load series metadata, not values" },
    { "glob", 1, 'g', "PATTERN", "glob pattern to restrict matches" },
    { "labels", 0, 'l', 0, "list all labels for time series" },
    { "fullpmid", 0, 'M', 0, "print PMID in verbose format" },
    { "metrics", 0, 'm', 0, "report names for time series metrics" },
    { "names", 0, 'n', 0, "print label names only, not values" },
    { "sources", 0, 'S', 0, "report names for time series sources" },
    { "series", 0, 's', 0, "print series ID for metrics, instances and sources" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_BOUNDARIES,
    .short_options = "ac:dD:Fg:h:iIlLmMnqp:sSV?",
    .long_options = longopts,
    .short_usage = "[options] [query ... | series ... | source ...]",
    .override = pmseries_overrides,
};

int
main(int argc, char *argv[])
{
    sds			option, query, match = NULL;
    int			c, sts;
    const char		*split = ",";
    const char		*space = " ";
    const char		*inifile = NULL;
    const char		*redis_host = NULL;
    unsigned int	redis_port = 6379;	/* default Redis port */
    struct dict		*config;
    series_flags	flags = 0;
    series_data		*dp;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'a':	/* command line contains series identifiers */
	    flags |= (PMSERIES_OPT_ALL | PMSERIES_SERIESID);
	    break;

	case 'c':	/* path to .ini configuration file */
	    inifile = opts.optarg;
	    break;

	case 'd':	/* command line contains series identifiers */
	    flags |= PMSERIES_OPT_DESC;
	    break;

	case 'F':	/* perform metadata-only --load, or --query */
	    flags |= PMSERIES_FAST;
	    break;

        case 'g':
	    match = sdsnew(opts.optarg);
	    break;

        case 'h':	/* Redis host to connect to */
	    redis_host = opts.optarg;
	    break;

	case 'i':	/* command line contains series identifiers */
	    flags |= PMSERIES_OPT_INSTS;
	    break;

	case 'I':	/* full form InDom reporting, ala pminfo -I */
	    flags |= PMSERIES_FULLINDOM;
	    break;

	case 'l':	/* command line contains series identifiers */
	    flags |= PMSERIES_OPT_LABELS;
	    break;

	case 'L':	/* command line contains source load string */
	    flags |= PMSERIES_OPT_LOAD;
	    split = space;
	    break;

	case 'm':	/* command line contains series identifiers */
	    flags |= PMSERIES_OPT_METRIC;
	    break;

	case 'M':	/* full form PMID reporting, ala pminfo -M */
	    flags |= PMSERIES_FULLPMID;
	    break;

	case 'n':	/* report label names only, not values */
	    flags |= (PMSERIES_OPT_LABELS|PMSERIES_ONLY_NAMES);
	    break;

        case 'p':	/* Redis port to connect to */
	    redis_port = (unsigned int)strtol(opts.optarg, NULL, 10);
	    break;

	case 'q':	/* command line contains query string */
	    flags |= PMSERIES_OPT_QUERY;
	    split = space;
	    break;

	case 's':	/* report series identifiers, ala pminfo -s */
	    flags |= PMSERIES_SERIESID;
	    break;

	case 'S':	/* command line contains source identifiers */
	    flags |= PMSERIES_OPT_SOURCE;
	    break;

	default:
	    opts.errors++;
	    break;
	}
    }

    /*
     * Parse the configuration file, extracting a dictionary of key/value
     * pairs.  Each key is "section.name" and values are always strings.
     * If no config given, default is /etc/pcp/pmproxy.conf (in addition,
     * local user path settings in $HOME/.pcp/pmproxy.conf are merged) -
     * pmseries(1) uses only keys from the [pmseries] section, but share
     * the main pmproxy.conf file(s) (via symlink) for user convenience.
     */
    if ((config = pmIniFileSetup(inifile)) == NULL) {
	pmprintf("%s: cannot setup from configuration file %s\n",
			pmGetProgname(), inifile? inifile : "pmseries.conf");
	opts.errors++;
    } else {
	/*
	 * Push command line options into the configuration, and ensure
	 * we have some default for attemping Redis server connections.
	 */
	if ((option = pmIniFileLookup(config, "pmseries", "servers")) == NULL ||
            (redis_host != NULL || redis_port != 6379)) {
            option = sdscatfmt(sdsempty(), "%s:%u",
                    redis_host? redis_host : "localhost", redis_port);
            pmIniFileUpdate(config, "pmseries", "servers", option);
        }
    }

    if (flags & PMSERIES_OPT_ALL)
	flags |= PMSERIES_META_OPTS;

    if ((flags & PMSERIES_OPT_LOAD) &&
	(flags & (PMSERIES_META_OPTS | PMSERIES_OPT_SOURCE))) {
	pmprintf("%s: error - cannot use load and reporting options together\n",
			pmGetProgname());
	opts.errors++;
    }
    else if ((flags & PMSERIES_OPT_LOAD) && (flags & PMSERIES_OPT_QUERY)) {
	pmprintf("%s: error - cannot use load and querying options together\n",
			pmGetProgname());
	opts.errors++;
    }
    else if ((flags & PMSERIES_OPT_QUERY) &&
	(flags & (PMSERIES_META_OPTS | PMSERIES_OPT_SOURCE))) {
	pmprintf("%s: error - cannot use query and metadata options together\n",
			pmGetProgname());
	opts.errors++;
    }

    if (flags & (PMSERIES_FULLPMID | PMSERIES_FULLINDOM))
	flags |= PMSERIES_NEED_DESCS;
    if (flags & PMSERIES_SERIESID)
	flags |= PMSERIES_NEED_DESCS;

    if (flags & PMSERIES_OPT_LABELS)
	flags |= PMSERIES_NEED_INSTS;

    /*
     * Determine default mode if no specific options presented.
     * If all parameters are series hashes, assume --all metadata
     * mode otherwise assume its a --query request.
     */
    if (!(flags & (PMSERIES_META_OPTS|PMSERIES_OPT_LOAD|PMSERIES_OPT_SOURCE)) &&
	!(flags & (PMSERIES_NEED_DESCS|PMSERIES_NEED_INSTS))) {
	for (c = opts.optind; c < argc; c++) {
	    if (strlen(argv[c]) != 40)
		break;
	}
	if (c != argc || opts.optind == argc)
	    flags |= PMSERIES_OPT_QUERY;
	else
	    flags |= PMSERIES_OPT_ALL | PMSERIES_META_OPTS | PMSERIES_SERIESID;
    }

    if (opts.optind == argc && !opts.errors) {
	if ((flags & PMSERIES_OPT_QUERY)) {
	   pmprintf("%s: error - no query string provided\n",
			   pmGetProgname());
	   opts.errors++;
	}
	else if (!(flags & (PMSERIES_META_OPTS|PMSERIES_OPT_SOURCE)) ||
		 /* --all needs a timeseries identifier to work on */
		 ((flags & PMSERIES_OPT_ALL) && opts.optind == argc)) {
	    pmprintf("%s: error - no series string(s) provided\n",
			    pmGetProgname());
	    opts.errors++;
	}
    }

    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT)) {
	sts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(sts);
    }

    if (pmLogLevelIsTTY())
	flags |= PMSERIES_COLOUR;

    if (opts.optind == argc)
	query = sdsempty();
    else
	query = sdsjoin(&argv[opts.optind], argc - opts.optind, (char *)split);

    dp = series_data_init(flags, query);
    dp->loop = uv_default_loop();
    dp->args.pattern = match;

    dp->settings.callbacks.on_match = on_series_match;
    dp->settings.callbacks.on_desc = on_series_desc;
    dp->settings.callbacks.on_inst = on_series_inst;
    dp->settings.callbacks.on_labelmap = on_series_labelmap;
    dp->settings.callbacks.on_instance = on_series_instance;
    dp->settings.callbacks.on_context = on_series_context;
    dp->settings.callbacks.on_metric = on_series_metric;
    dp->settings.callbacks.on_value = on_series_value;
    dp->settings.callbacks.on_label = on_series_label;
    dp->settings.callbacks.on_done = on_series_done;

    dp->settings.module.on_info = on_series_info;
    dp->settings.module.on_setup = on_series_setup;

    pmSeriesSetEventLoop(&dp->settings.module, dp->loop);
    pmSeriesSetConfiguration(&dp->settings.module, config);

    return pmseries_execute(dp);
}
