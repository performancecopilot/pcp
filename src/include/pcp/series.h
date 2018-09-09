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
#ifndef PCP_SERIES_H
#define PCP_SERIES_H

#include <stdio.h>
#include "sds.h"	/* typedef sds (char *) */

typedef sds pmSID;	/* external 40-byte series or source identifier */

typedef enum pmloglevel {
    PMLOG_TRACE,	/* verbose event tracing */
    PMLOG_DEBUG,	/* debugging diagnostics */
    PMLOG_INFO,		/* general information */
    PMLOG_WARNING,	/* generic warnings */
    PMLOG_ERROR,	/* generic error */
    PMLOG_REQUEST,	/* error processing a request */
    PMLOG_RESPONSE,	/* error processing a response */
    PMLOG_CORRUPT,	/* corrupt time series database */
} pmloglevel;

extern int pmLogLevelIsTTY(void);
extern void pmLogLevelPrint(FILE *, pmloglevel, sds, int);
extern const char *pmLogLevelStr(pmloglevel);

typedef enum pmflags {
    PMFLAG_METADATA  = (1<<0),	/* only load metric metadata not values */
    PMFLAG_ACTIVE    = (1<<1),	/* continual updates from metric source */
} pmflags;

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
    sds		series;		/* metric series identifier for values */
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

typedef void (*pmSeriesInfoCallBack)(pmloglevel, sds, void *);
typedef void (*pmSeriesSetupCallBack)(void *);

typedef void (*pmSeriesInitCallBack)(void *);
typedef int (*pmSeriesMatchCallBack)(pmSID, void *);
typedef int (*pmSeriesStringCallBack)(pmSID, sds, void *);
typedef int (*pmSeriesDescCallBack)(pmSID, pmSeriesDesc *, void *);
typedef int (*pmSeriesInstCallBack)(pmSID, pmSeriesInst *, void *);
typedef int (*pmSeriesValueCallBack)(pmSID, pmSeriesValue *, void *);
typedef int (*pmSeriesLabelCallBack)(pmSID, pmSeriesLabel *, void *);
typedef void (*pmSeriesDoneCallBack)(int, void *);

typedef struct pmSeriesCommand {
    sds                         hostspec;       /* initial connect hostspec */
    pmSeriesInfoCallBack	on_info;	/* general diagnostics call */
    pmSeriesSetupCallBack	on_setup;	/* server connections setup */
    void			*events;	/* opaque event library data */
    void			*slots;		/* opaque server slots data */
    void			*data;		/* private internal lib data */
} pmSeriesCommand;

typedef struct pmSeriesSettings {
    pmSeriesCommand		command;	/* general command setup */
    pmSeriesMatchCallBack	on_match;	/* one series identifier */
    pmSeriesDescCallBack	on_desc;	/* metric descriptor */
    pmSeriesInstCallBack	on_inst;	/* instance details */
    pmSeriesLabelCallBack	on_labelmap;	/* label name value pair */
    pmSeriesStringCallBack	on_instance;	/* one instance name */
    pmSeriesStringCallBack	on_context;	/* one context name */
    pmSeriesStringCallBack	on_metric;	/* one metric name */
    pmSeriesStringCallBack	on_label;	/* one label name */
    pmSeriesValueCallBack	on_value;	/* timestamped value */
    pmSeriesDoneCallBack	on_done;	/* request completed */
} pmSeriesSettings;

extern int pmSeriesDescs(pmSeriesSettings *, int, pmSID *, void *);
extern int pmSeriesLabels(pmSeriesSettings *, int, pmSID *, void *);
extern int pmSeriesInstances(pmSeriesSettings *, int, pmSID *, void *);
extern int pmSeriesMetrics(pmSeriesSettings *, int, pmSID *, void *);
extern int pmSeriesSources(pmSeriesSettings *, int, pmSID *, void *);
extern int pmSeriesQuery(pmSeriesSettings *, sds, pmflags, void *);
extern int pmSeriesLoad(pmSeriesSettings *, sds, pmflags, void *);

extern void pmSeriesSetup(pmSeriesCommand *, void *);
extern void pmSeriesClose(pmSeriesCommand *);

#endif /* PCP_SERIES_H */
