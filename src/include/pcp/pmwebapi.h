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
#ifndef PCP_PMWEBAPI_H
#define PCP_PMWEBAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pcp/pmapi.h>
#include "sds.h"

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
 * Fast, scalable time series querying services
 */

typedef sds pmSID;	/* external 40-byte time series or source identifier */

typedef enum pmSeriesFlags {
    PM_SERIES_FLAG_NONE		= (0),
    PM_SERIES_FLAG_METADATA	= (1 << 0),	/* only load metric metadata */
    PM_SERIES_FLAG_ACTIVE	= (1 << 1),	/* continual source updates */
    PM_SERIES_FLAG_ALL		= ((unsigned int)~PM_SERIES_FLAG_NONE)
} pmSeriesFlags;

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
} pmSeriesValue;

typedef struct pmSeriesLabel {
    sds		name;		/* name (string) of this label */
    sds		value;		/* value of this label */
} pmSeriesLabel;

typedef void (*pmSeriesSetupCallBack)(void *);
typedef int (*pmSeriesMatchCallBack)(pmSID, void *);
typedef int (*pmSeriesStringCallBack)(pmSID, sds, void *);
typedef int (*pmSeriesDescCallBack)(pmSID, pmSeriesDesc *, void *);
typedef int (*pmSeriesInstCallBack)(pmSID, pmSeriesInst *, void *);
typedef int (*pmSeriesValueCallBack)(pmSID, pmSeriesValue *, void *);
typedef int (*pmSeriesLabelCallBack)(pmSID, pmSeriesLabel *, void *);
typedef void (*pmSeriesDoneCallBack)(int, void *);

typedef struct pmSeriesCallBacks {
    pmSeriesMatchCallBack	on_match;	/* one series identifier */
    pmSeriesDoneCallBack	on_match_done;	/* last identifier send back */
    pmSeriesDescCallBack	on_desc;	/* metric descriptor */
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
    sds                         hostspec;       /* initial connect hostspec */
    void			*metrics;	/* registry of perf metrics */
    void			*events;	/* opaque event library data */
    void			*slots;		/* opaque server slots data */
    void			*data;		/* private internal lib data */
} pmSeriesModule;

typedef struct pmSeriesSettings {
    pmSeriesModule		module;
    pmSeriesCallBacks		callbacks;
} pmSeriesSettings;

extern void pmSeriesSetup(pmSeriesModule *, void *);
extern int pmSeriesDescs(pmSeriesSettings *, int, pmSID *, void *);
extern int pmSeriesLabels(pmSeriesSettings *, int, pmSID *, void *);
extern int pmSeriesInstances(pmSeriesSettings *, int, pmSID *, void *);
extern int pmSeriesMetrics(pmSeriesSettings *, int, pmSID *, void *);
extern int pmSeriesSources(pmSeriesSettings *, int, pmSID *, void *);
extern int pmSeriesQuery(pmSeriesSettings *, sds, pmSeriesFlags, void *);
extern int pmSeriesLoad(pmSeriesSettings *, sds, pmSeriesFlags, void *);
extern void pmSeriesClose(pmSeriesModule *);

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
    unsigned int		handle;		/* callbacks context handle */
    sds				logname;	/* archive directory dirname */
    void			*metrics;	/* registry of perf metrics */
    void			*events;	/* opaque event library data */
    void			*slots;		/* opaque server slots data */
    void			*data;		/* private internal lib data */
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
} pmDiscoverCallBacks;

typedef struct pmDiscoverSettings {
    pmDiscoverModule		module;
    pmDiscoverCallBacks		callbacks;
} pmDiscoverSettings;

extern void pmDiscoverSetup(pmDiscoverSettings *, void *);
extern void pmDiscoverClose(pmDiscoverSettings *);

#ifdef __cplusplus
}
#endif

#endif /* PCP_PMWEBAPI_H */
