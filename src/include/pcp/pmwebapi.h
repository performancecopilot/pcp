/*
 * Copyright (c) 2017-2022,2025 Red Hat.
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
#ifndef PCP_PMWEBAPI_H
#define PCP_PMWEBAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pcp/pmapi.h>
#include <pcp/sds.h>

/*
 * Opaque structures - forward declarations.
 * (see also <pcp/dict.h> <pcp/mmv_stats.h>)
 */

struct dict;
struct mmv_registry;

/*
 * Generalised asynchronous logging facilities.
 */

typedef enum pmLogLevel {
    PMLOG_TRACE,	/* verbose event tracing */
    PMLOG_DEBUG,	/* debugging diagnostics */
    PMLOG_INFO,		/* general information */
    PMLOG_WARNING,	/* generic warnings */
    PMLOG_ERROR,	/* generic error */
    PMLOG_REQUEST,	/* error processing a request */
    PMLOG_RESPONSE,	/* error processing a response */
    PMLOG_CORRUPT,	/* corrupt time series database */
} pmLogLevel;

typedef void (*pmLogInfoCallBack)(pmLogLevel, sds, void *);

extern int pmLogLevelIsTTY(void);
extern void pmLogLevelPrint(FILE *, pmLogLevel, sds, int);
extern const char *pmLogLevelStr(pmLogLevel);

/*
 * Configuration file services
 */

extern struct dict *pmIniFileSetup(const char *);
extern void pmIniFileUpdate(struct dict *, const char *, const char *, sds);
extern sds pmIniFileLookup(struct dict *, const char *, const char *);
extern void pmIniFileFree(struct dict *);

/*
 * Fast, scalable time series querying services
 */

typedef sds pmSID;	/* external 40-byte time series or source identifier */

typedef enum pmSeriesFlags {
    PM_SERIES_FLAG_NONE		= (0),
    PM_SERIES_FLAG_METADATA	= (1 << 0),	/* only load metric metadata */
    PM_SERIES_FLAG_ACTIVE	= (1 << 1),	/* continual source updates */
    PM_SERIES_FLAG_TEXT		= (1 << 2),	/* load metric & indom help */
    PM_SERIES_FLAG_ALL		= ((unsigned int)~PM_SERIES_FLAG_NONE)
} pmSeriesFlags;

typedef struct pmSeriesExpr {
    sds		query;		/* canonical expression string */
} pmSeriesExpr;

typedef struct pmSeriesDesc {
    sds		indom;		/* dotted-pair instance domain identifier */
    sds		pmid;		/* dotted-triple metric identifier */
    sds		semantics;	/* pmSemStr(3) metric semantics */
    sds		source;		/* source identifier, from whence we came */
    sds		type;		/* pmTypeStr(3) metric type */
    sds		units;		/* pmUnitsStr(3) metric units */
} pmSeriesDesc;

typedef struct pmSeriesInst {
    sds		instid;		/* first seen numeric instance identifier */
    sds		name;		/* full external (string) instance name */
    sds		source;		/* instances source series identifier */
    sds		series;		/* series identifier for inst value */
} pmSeriesInst;

typedef struct pmSeriesValue {
    sds		timestamp;	/* sample time this value was taken */
    sds		series;		/* series identifier for this value */
    sds		data;		/* actual value, as binary safe sds */
    pmTimespec	ts;		/* sample time, converted to binary */
} pmSeriesValue;

typedef struct pmSeriesLabel {
    sds		name;		/* name (string) of this label */
    sds		value;		/* value of this label */
} pmSeriesLabel;

typedef struct pmSeriesTimeWindow {
    sds		delta;		/* sample interval */
    sds		align;		/* alignment for sample start */
    sds		start;		/* start time */
    sds		end;		/* end time */
    sds		range;		/* sample time range */
    sds		count;		/* number of samples */
    sds		offset;		/* offset from sample start */
    sds		zone;		/* timezone of time strings */
} pmSeriesTimeWindow;

typedef void (*pmSeriesSetupCallBack)(void *);
typedef int (*pmSeriesMatchCallBack)(pmSID, void *);
typedef int (*pmSeriesStringCallBack)(pmSID, sds, void *);
typedef int (*pmSeriesDescCallBack)(pmSID, pmSeriesDesc *, void *);
typedef int (*pmSeriesExprCallBack)(pmSID, pmSeriesExpr *, void *);
typedef int (*pmSeriesInstCallBack)(pmSID, pmSeriesInst *, void *);
typedef int (*pmSeriesValueCallBack)(pmSID, pmSeriesValue *, void *);
typedef int (*pmSeriesLabelCallBack)(pmSID, pmSeriesLabel *, void *);
typedef void (*pmSeriesDoneCallBack)(int, void *);

typedef struct pmSeriesCallBacks {
    pmSeriesMatchCallBack	on_match;	/* one series identifier */
    pmSeriesDescCallBack	on_desc;	/* metric descriptor */
    pmSeriesExprCallBack	on_expr;	/* query expression */
    pmSeriesInstCallBack	on_inst;	/* instance details */
    pmSeriesLabelCallBack	on_labelmap;	/* label name value pair */
    pmSeriesStringCallBack	on_instance;	/* one instance name */
    pmSeriesStringCallBack	on_context;	/* one context name */
    pmSeriesStringCallBack	on_metric;	/* one metric name */
    pmSeriesStringCallBack	on_label;	/* one label name */
    pmSeriesValueCallBack	on_value;	/* timestamped value */
    pmSeriesDoneCallBack	on_done;	/* request completed */
} pmSeriesCallBacks;

typedef struct pmSeriesModule {
    pmLogInfoCallBack		on_info;	/* general diagnostics call */
    pmSeriesSetupCallBack	on_setup;	/* server connections setup */
    void			*privdata;	/* private internal lib data */
} pmSeriesModule;

typedef struct pmSeriesSettings {
    pmSeriesModule		module;
    pmSeriesCallBacks		callbacks;
} pmSeriesSettings;

extern int pmSeriesSetup(pmSeriesModule *, void *);
extern int pmSeriesSetSlots(pmSeriesModule *, void *);
extern int pmSeriesSetEventLoop(pmSeriesModule *, void *);
extern int pmSeriesSetConfiguration(pmSeriesModule *, struct dict *);
extern int pmSeriesSetMetricRegistry(pmSeriesModule *, struct mmv_registry *);
extern void pmSeriesClose(pmSeriesModule *);

extern int pmSeriesDescs(pmSeriesSettings *, int, sds *, void *);
extern int pmSeriesExprs(pmSeriesSettings *, int, sds *, void *);
extern int pmSeriesLabels(pmSeriesSettings *, int, sds *, void *);
extern int pmSeriesLabelValues(pmSeriesSettings *, int, sds *, void *);
extern int pmSeriesInstances(pmSeriesSettings *, int, sds *, void *);
extern int pmSeriesMetrics(pmSeriesSettings *, int, sds *, void *);
extern int pmSeriesSources(pmSeriesSettings *, int, sds *, void *);
extern int pmSeriesValues(pmSeriesSettings *, pmSeriesTimeWindow *, int, sds *, void *);
extern int pmSeriesWindow(pmSeriesSettings *, sds, pmSeriesTimeWindow *, void *);
extern int pmSeriesQuery(pmSeriesSettings *, sds, pmSeriesFlags, void *);
extern int pmSeriesLoad(pmSeriesSettings *, sds, pmSeriesFlags, void *);

/*
 * Timer list interface - global, thread-safe
 */
typedef void (*pmWebTimerCallBack)(void *);
extern int pmWebTimerRegister(pmWebTimerCallBack, void *);
extern int pmWebTimerRelease(int); /* deregister one timer */
extern int pmWebTimerSetup(void);
extern int pmWebTimerSetEventLoop(void *);
extern int pmWebTimerSetMetricRegistry(struct mmv_registry *);
extern void pmWebTimerClose(void);

/*
 * Asynchronous archive location and contents discovery services
 */
struct pmDiscoverModule;

typedef struct pmDiscoverContext {
    pmSID			source;		/* source identifier hash */
    pmLabelSet			*labelset;	/* context labels of source */
    sds				hostname;	/* hostname from the context */
    sds				name;		/* name for creating context */
    int				type;		/* type for creating context */
} pmDiscoverContext;

typedef struct pmDiscoverEvent {
    struct pmDiscoverModule	*module;	/* module handling the event */
    pmDiscoverContext		context;	/* metric source identifiers */
    pmTimespec			timestamp;	/* time that event occurred */
    void			*data;		/* private internal lib data */
} pmDiscoverEvent;

typedef struct pmDiscoverModule {
    pmLogInfoCallBack		on_info;	/* general diagnostics call */
    void			*privdata;	/* private internal lib data */
} pmDiscoverModule;

typedef void (*pmDiscoverSourceCallBack)(pmDiscoverEvent *, void *);
typedef void (*pmDiscoverClosedCallBack)(pmDiscoverEvent *, void *);
typedef void (*pmDiscoverLabelsCallBack)(pmDiscoverEvent *,
		int, int, pmLabelSet *, int, void *);
typedef void (*pmDiscoverMetricCallBack)(pmDiscoverEvent *,
		pmDesc *, int, char **, void *);
typedef void (*pmDiscoverValuesCallBack)(pmDiscoverEvent *,
		pmResult *, void *);
typedef void (*pmDiscoverInDomCallBack)(pmDiscoverEvent *,
		pmInResult *, void *);
typedef void (*pmDiscoverTextCallBack)(pmDiscoverEvent *,
		int, int, char *, void *);

typedef struct pmDiscoverCallBacks {
    pmDiscoverSourceCallBack	on_source;	/* metric source discovered */
    pmDiscoverClosedCallBack	on_closed;	/* end of discovery updates */
    pmDiscoverLabelsCallBack	on_labels;	/* new labelset discovered */
    pmDiscoverMetricCallBack	on_metric;	/* metric descriptor, names */
    pmDiscoverValuesCallBack	on_values;	/* metrics value set arrived */
    pmDiscoverInDomCallBack	on_indom;	/* instance domain discovered */
    pmDiscoverTextCallBack	on_text;	/* new help text discovered */
    struct pmDiscoverCallBacks	*next;		/* optional list of callbacks */
} pmDiscoverCallBacks;

typedef struct pmDiscoverSettings {
    pmDiscoverModule		module;
    pmDiscoverCallBacks		callbacks;
} pmDiscoverSettings;

extern int pmDiscoverSetup(pmDiscoverModule *, pmDiscoverCallBacks *, void *);
extern int pmDiscoverSetSlots(pmDiscoverModule *, void *);
extern int pmDiscoverSetEventLoop(pmDiscoverModule *, void *);
extern int pmDiscoverSetConfiguration(pmDiscoverModule *, struct dict *);
extern int pmDiscoverSetMetricRegistry(pmDiscoverModule *, struct mmv_registry *);
extern void pmDiscoverClose(pmDiscoverModule *);

/*
 * Interfaces providing PMWEBAPI(3) backward compatibility.
 * Provides live performance data only; no archive support.
 */
typedef struct pmWebSource {
    pmSID		source;
    sds			hostspec;
    sds			labels;
} pmWebSource;

typedef struct pmWebAccess {
    sds			username;
    sds			password;
    sds			realm;
} pmWebAccess;

typedef struct pmWebMetric {
    pmSID		series;
    pmID		pmid;
    sds			name;
    sds			sem;
    sds			type;
    sds			units;
    pmInDom		indom;
    sds			labels;
    sds			oneline;
    sds			helptext;
} pmWebMetric;

typedef struct pmWebResult {
    long long		seconds;
    long long		nanoseconds;
} pmWebResult;

typedef struct pmWebValueSet {
    pmSID		series;
    pmID		pmid;
    sds			name;
    sds			labels;
} pmWebValueSet;

typedef struct pmWebValue {	/* used with both fetch and scrape */
    pmSID		series;
    pmID		pmid;
    unsigned int	inst;
    sds			value;
} pmWebValue;

typedef struct pmWebInDom {
    pmInDom		indom;
    sds			labels;
    sds			oneline;
    sds			helptext;
    unsigned int	numinsts;
} pmWebInDom;

typedef struct pmWebInstance {
    pmInDom		indom;
    unsigned int	inst;
    sds			labels;
    sds			name;
} pmWebInstance;

typedef struct pmWebChildren {
    sds			name;
    unsigned int	numleaf;
    unsigned int	numnonleaf;
    sds			*leaf;
    sds			*nonleaf;
} pmWebChildren;

typedef struct pmWebScrape {
    pmWebMetric		metric;
    pmWebInstance	instance;
    pmWebValue		value;
    long long		seconds;
    long long		nanoseconds;
} pmWebScrape;

typedef struct pmWebLabelSet {
    pmLabelSet		*sets[6];
    int			nsets;
    sds			buffer;
    unsigned int	instid;
    sds			instname;
} pmWebLabelSet;

typedef void (*pmWebContextCallBack)(sds, pmWebSource *, void *);
typedef int (*pmWebAccessCallBack)(sds, pmWebAccess *, int *, sds *, void *);
typedef void (*pmWebMetricCallBack)(sds, pmWebMetric *, void *);
typedef int (*pmWebFetchCallBack)(sds, pmWebResult *, void *);
typedef int (*pmWebFetchValueSetCallBack)(sds, pmWebValueSet *, void *);
typedef int (*pmWebFetchValueCallBack)(sds, pmWebValue *, void *);
typedef int (*pmWebInDomCallBack)(sds, pmWebInDom *, void *);
typedef int (*pmWebInDomInstanceCallBack)(sds, pmWebInstance *, void *);
typedef int (*pmWebChildrenCallBack)(sds, pmWebChildren *, void *);
typedef int (*pmWebScrapeCallBack)(sds, pmWebScrape *, void *);
typedef void (*pmWebScrapeLabelSetCallBack)(sds, pmWebLabelSet *, void *);
typedef void (*pmWebStatusCallBack)(sds, int, sds, void *);

typedef struct pmWebGroupCallBacks {
    pmWebContextCallBack	on_context;
    pmWebMetricCallBack		on_metric;
    pmWebFetchCallBack		on_fetch;
    pmWebFetchValueSetCallBack	on_fetch_values;
    pmWebFetchValueCallBack	on_fetch_value;
    pmWebInDomCallBack		on_indom;
    pmWebInDomInstanceCallBack	on_instance;
    pmWebChildrenCallBack	on_children;
    pmWebScrapeCallBack		on_scrape;
    pmWebScrapeLabelSetCallBack	on_scrape_labels;
    pmWebAccessCallBack		on_check;	/* general access check call */
    pmWebStatusCallBack		on_done;	/* all-purpose done callback */
} pmWebGroupCallBacks;

typedef struct pmWebGroupModule {
    pmLogInfoCallBack		on_info;	/* general diagnostics call */
    void			*privdata;	/* private internal lib data */
} pmWebGroupModule;

typedef struct pmWebGroupSettings {
    pmWebGroupModule		module;
    pmWebGroupCallBacks		callbacks;
} pmWebGroupSettings;

extern int pmWebGroupContext(pmWebGroupSettings *, sds, struct dict *, void *);
extern void pmWebGroupDerive(pmWebGroupSettings *, sds, struct dict *, void *);
extern void pmWebGroupFetch(pmWebGroupSettings *, sds, struct dict *, void *);
extern void pmWebGroupInDom(pmWebGroupSettings *, sds, struct dict *, void *);
extern void pmWebGroupMetric(pmWebGroupSettings *, sds, struct dict *, void *);
extern void pmWebGroupChildren(pmWebGroupSettings *, sds, struct dict *, void *);
extern void pmWebGroupProfile(pmWebGroupSettings *, sds, struct dict *, void *);
extern void pmWebGroupScrape(pmWebGroupSettings *, sds, struct dict *, void *);
extern void pmWebGroupStore(pmWebGroupSettings *, sds, struct dict *, void *);
extern void pmWebGroupDestroy(pmWebGroupSettings *, sds, void *);

extern int pmWebGroupSetup(pmWebGroupModule *);
extern int pmWebGroupSetEventLoop(pmWebGroupModule *, void *);
extern int pmWebGroupSetConfiguration(pmWebGroupModule *, struct dict *);
extern int pmWebGroupSetMetricRegistry(pmWebGroupModule *, struct mmv_registry *);
extern void pmWebGroupClose(pmWebGroupModule *);

/*
 * Interfaces providing pmlogger 'push' functionality,
 * a webhook for archives logs for central monitoring.
 */

typedef void (*pmLogArchiveCallBack)(int, void *);
typedef void (*pmLogStatusCallBack)(int, void *);

typedef struct pmLogGroupCallBacks {
    pmLogArchiveCallBack	on_archive;	/* archive label established */
    pmLogStatusCallBack		on_done;	/* all-purpose done callback */
} pmLogGroupCallBacks;

typedef struct pmLogGroupModule {
    pmLogInfoCallBack		on_info;	/* general diagnostics call */
    void			*privdata;	/* private internal lib data */
} pmLogGroupModule;

typedef struct pmLogGroupSettings {
    pmLogGroupModule		module;
    pmLogGroupCallBacks		callbacks;
} pmLogGroupSettings;

extern int pmLogGroupLabel(pmLogGroupSettings *, const char *, size_t, struct dict *, void *);
extern int pmLogGroupMeta(pmLogGroupSettings *, int, const char *, size_t, struct dict *, void *);
extern int pmLogGroupIndex(pmLogGroupSettings *, int, const char *, size_t, struct dict *, void *);
extern int pmLogGroupVolume(pmLogGroupSettings *, int, unsigned int, const char *, size_t, struct dict *, void *);

extern int pmLogGroupSetup(pmLogGroupModule *);
extern int pmLogGroupSetEventLoop(pmLogGroupModule *, void *);
extern int pmLogGroupSetConfiguration(pmLogGroupModule *, struct dict *);
extern int pmLogGroupSetMetricRegistry(pmLogGroupModule *, struct mmv_registry *);
extern void pmLogGroupClose(pmLogGroupModule *);

/*
 * Full text search for metrics and instance domains.
 */

typedef enum pmSearchFlags {
    PM_SEARCH_FLAG_NONE		= (0),
    PM_SEARCH_FLAG_NOTEXT	= (1 << 0),	/* no text in the response */
    PM_SEARCH_FLAG_HIGHLIGHT	= (1 << 1),	/* highlight search terms */
} pmSearchFlags;

typedef enum pmSearchTextType {
    PM_SEARCH_TYPE_UNKNOWN	= 0,
    PM_SEARCH_TYPE_METRIC	= 1,
    PM_SEARCH_TYPE_INDOM	= 2,
    PM_SEARCH_TYPE_INST		= 3
} pmSearchTextType;

typedef struct pmSearchTextRequest {
    sds			query;		/* query string */
    pmSearchFlags	flags;		/* query control bits */
    unsigned int	count;		/* maximum results to return */
    unsigned int	offset;		/* results pagination offset */

    unsigned int	type_metric : 1;	/* restrict query types */
    unsigned int	type_indom : 1;
    unsigned int	type_inst : 1;
    unsigned int	type_pad : 1;

    unsigned int	highlight_name : 1;	/* highlight results */
    unsigned int	highlight_oneline : 1;
    unsigned int	highlight_helptext : 1;

    unsigned int	infields_name : 1;	/* restrict query fields */
    unsigned int	infields_oneline : 1;
    unsigned int	infields_helptext : 1;

    unsigned int	return_name : 1;	/* restrict returned fields */
    unsigned int	return_indom : 1;
    unsigned int	return_oneline : 1;
    unsigned int	return_helptext : 1;
    unsigned int	return_type : 1;

    unsigned int	reserved: 17;	/* zero padding */
} pmSearchTextRequest;

typedef struct pmSearchTextResult {
    unsigned int	total;		/* total number of results */
    unsigned int	count;		/* query result index 'count' */
    double		timer;		/* elapsed time (in seconds) */
    double		score;		/* search engine hit ranking */

    pmSearchTextType	type;		/* query result document type */
    sds			docid;		/* unique result identifier */
    sds			name;		/* metric / instance name */
    sds			indom;
    sds			oneline;
    sds			helptext;
} pmSearchTextResult;

typedef struct pmSearchMetrics {
    unsigned long long	docs;		/* number of documents */
    unsigned long long	terms;		/* number of distinct terms */
    unsigned long long	records;	/* number of search records */
    double		inverted_sz_mb;
    double		inverted_cap_mb;
    double		inverted_cap_ovh;
    double		offset_vectors_sz_mb;
    double		skip_index_size_mb;
    double		score_index_size_mb;
    double		records_per_doc_avg;
    double		bytes_per_record_avg;
    double		offsets_per_term_avg;
    double		offset_bits_per_record_avg;
} pmSearchMetrics;

typedef void (*pmSearchSetupCallBack)(void *);
typedef void (*pmSearchTextResultCallBack)(pmSearchTextResult *, void *);
typedef void (*pmSearchMetricsCallBack)(pmSearchMetrics *, void *);
typedef void (*pmSearchDoneCallBack)(int, void *);

typedef struct pmSearchCallBacks {
    pmSearchTextResultCallBack	on_text_result;	/* text search hit */
    pmSearchMetricsCallBack	on_metrics;	/* runtime stats */
    pmSearchDoneCallBack	on_done;	/* request completed */
} pmSearchCallBacks;

typedef struct pmSeriesModule pmSearchModule;	/* shared structure */

typedef struct pmSearchSettings {
    pmSearchModule		module;
    pmSearchCallBacks		callbacks;
} pmSearchSettings;

extern int pmSearchSetup(pmSearchModule *, void *);
extern int pmSearchSetSlots(pmSearchModule *, void *);
extern int pmSearchSetEventLoop(pmSearchModule *, void *);
extern int pmSearchSetConfiguration(pmSearchModule *, struct dict *);
extern int pmSearchSetMetricRegistry(pmSearchModule *, struct mmv_registry *);
extern void pmSearchClose(pmSearchModule *);
extern int pmSearchEnabled(void *);

extern const char *pmSearchTextTypeStr(pmSearchTextType);
extern int pmSearchInfo(pmSearchSettings *, sds, void *);
extern int pmSearchTextInDom(pmSearchSettings *, pmSearchTextRequest *, void *);
extern int pmSearchTextSuggest(pmSearchSettings *, pmSearchTextRequest *, void *);
extern int pmSearchTextQuery(pmSearchSettings *, pmSearchTextRequest *, void *);

#ifdef __cplusplus
}
#endif

#endif /* PCP_PMWEBAPI_H */
