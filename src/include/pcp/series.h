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
#include "sds.h" /* define typedef sds */

typedef sds pmSeriesID;	/* external 40-byte form of series identifier */
typedef sds pmSourceID;	/* external 40-byte form of source identifier */

typedef enum pmloglevel {
    PMLOG_INFO = 1,	/* general information */
    PMLOG_WARNING,	/* generic warnings */
    PMLOG_ERROR,	/* generic error */
    PMLOG_REQUEST,	/* error processing a request */
    PMLOG_RESPONSE,	/* error processing a response */
    PMLOG_CORRUPTED,	/* corrupt time series database */
} pmloglevel;

extern int pmLogLevelIsTTY(void);
extern void pmLogLevelPrint(FILE *, pmloglevel, sds, int);
extern const char *pmLogLevelStr(pmloglevel);

typedef enum pmflags {
    PMFLAG_METADATA  = (1<<0),	/* only load metric metadata not values */
    PMFLAG_ACTIVE    = (1<<1),	/* continual updates from metric source */
} pmflags;

typedef enum pmdescfields {
    PMDESC_INDOM,		/* dotted-pair instance domain identifier */
    PMDESC_PMID,		/* dotted-triple metric identifier */
    PMDESC_SEMANTICS,		/* pmSemStr(3) metric semantics */
    PMDESC_SOURCE,		/* source identifier, from whence we came */
    PMDESC_TYPE,		/* pmTypeStr(3) metric type */
    PMDESC_UNITS,		/* pmUnitsStr(3) metric units */
    PMDESC_MAXFIELD
} pmdescfields;

typedef enum pminstfields {
    PMINST_INSTID,		/* first seen numeric instance identifier */
    PMINST_NAME,		/* full external (string) instance name */
    PMINST_SERIES,		/* metric series identifier for values */
    PMINST_MAXFIELD
} pminstfields;

typedef enum pmvaluefields {
    PMVALUE_TIMESTAMP,		/* sample time this value was taken */
    PMVALUE_SERIES,		/* series identifier for this value */
    PMVALUE_DATA,		/* actual value, as binary safe sds */
    PMVALUE_MAXFIELD
} pmvaluefields;

typedef enum pmlabelfields {
    PMLABEL_NAME,		/* name (string) of this label */
    PMLABEL_VALUE,		/* value of this label */
    PMLABEL_MAXFIELD
} pmlabelfields;

typedef int (*pmSeriesMatchCallBack)(pmSeriesID, void *);
typedef int (*pmSeriesStringCallBack)(pmSeriesID, sds, void *);
typedef int (*pmSeriesStructCallBack)(pmSeriesID, int, sds *, void *);
typedef void (*pmSeriesInfoCallBack)(pmloglevel, sds, void *);
typedef void (*pmSeriesDoneCallBack)(int, void *);

typedef struct pmSeriesSettings {
    pmSeriesMatchCallBack	on_match;	/* one series identifier */
    pmSeriesStructCallBack	on_desc;	/* metric descriptor */
    pmSeriesStructCallBack	on_inst;	/* instance details */
    pmSeriesStructCallBack	on_labelset;	/* set of labels */
    pmSeriesStringCallBack	on_instance;	/* one instance name */
    pmSeriesStringCallBack	on_context;	/* one context name */
    pmSeriesStringCallBack	on_metric;	/* one metric name */
    pmSeriesStringCallBack	on_label;	/* one label name */
    pmSeriesStructCallBack	on_value;	/* timestamped value */
    pmSeriesInfoCallBack	on_info;	/* diagnostics */
    pmSeriesDoneCallBack	on_done;	/* request completed */
} pmSeriesSettings;

extern void pmSeriesDescs(pmSeriesSettings *, int, pmSeriesID *, void *);
extern void pmSeriesLabels(pmSeriesSettings *, int, pmSeriesID *, void *);
extern void pmSeriesInstances(pmSeriesSettings *, int, pmSeriesID *, void *);
extern void pmSeriesMetrics(pmSeriesSettings *, int, pmSeriesID *, void *);
extern void pmSeriesSources(pmSeriesSettings *, int, pmSeriesID *, void *);
extern void pmSeriesQuery(pmSeriesSettings *, sds, pmflags, void *);
extern void pmSeriesLoad(pmSeriesSettings *, sds, pmflags, void *);

#endif /* PCP_SERIES_H */
