/*
 * Copyright (c) 2017 Red Hat.
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

#define PMSIDSZ	40
typedef struct { unsigned char name[PMSIDSZ+1]; } pmSeriesID;
#define PMSERIESIDSZ	sizeof(pmSeriesID)

typedef enum pmseries_level {
    PMSERIES_INFO,
    PMSERIES_WARNING,
    PMSERIES_ERROR,
    PMSERIES_REQUEST,
    PMSERIES_RESPONSE,
    PMSERIES_CORRUPT,
} pmseries_level;

typedef enum pmseries_flags {
    PMSERIES_METADATA,	/* only load metric metadata not values */
    PMSERIES_ACTIVE,	/* continual updates from metric source */
} pmseries_flags;

typedef int (*pmSeriesMatchCallback)(pmSeriesID *, void *);
typedef int (*pmSeriesValueCallback)(pmSeriesID *, const char *,
	const char *, void *);
typedef int (*pmSeriesDescCallback)(pmSeriesID *, const char *,
	const char *, const char *, const char *, const char *, void *);
typedef int (*pmSeriesInstanceCallback)(pmSeriesID *, int,
		const char *, void *);
typedef int (*pmSeriesStringCallback)(pmSeriesID *, const char *, void *);
typedef void (*pmSeriesInfoCallback)(pmseries_level, const char *, void *);
typedef void (*pmSeriesDoneCallback)(int, void *);

typedef struct pmSeriesSettings {
    pmSeriesMatchCallback	on_match;	/* one timeseries ID */
    pmSeriesValueCallback	on_value;	/* one timeseries value */
    pmSeriesDescCallback	on_desc;	/* one descriptor */
    pmSeriesInstanceCallback	on_instance;	/* one instance */
    pmSeriesStringCallback	on_metric;	/* one metric name */
    pmSeriesStringCallback	on_labels;	/* one set of labels */
    pmSeriesInfoCallback	on_info;	/* diagnostics */
    pmSeriesDoneCallback	on_done;	/* request completed */
} pmSeriesSettings;

extern void pmSeriesDesc(pmSeriesSettings *, int, pmSeriesID *, void *);
extern void pmSeriesLabel(pmSeriesSettings *, int, pmSeriesID *, void *);
extern void pmSeriesMetric(pmSeriesSettings *, int, pmSeriesID *, void *);
extern void pmSeriesInstance(pmSeriesSettings *, int, pmSeriesID *, void *);
extern void pmSeriesQuery(pmSeriesSettings *, const char *, pmseries_flags, void *);
extern void pmSeriesLoad(pmSeriesSettings *, const char *, pmseries_flags, void *);
extern const char *pmSeriesLevelStr(pmseries_level);

#endif /* PCP_SERIES_H */
