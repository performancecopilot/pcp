/*
 * Copyright (c) 2017-2021 Red Hat.
 * Copyright (c) 2020 Yushan ZHANG.
 * Copyright (c) 2022 Shiyao CHEN.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#ifndef SERIES_QUERY_H
#define SERIES_QUERY_H

#include "pmapi.h"
#include "pmwebapi.h"
#include "batons.h"
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

/*
 * Time series querying
 */

/* Various query node_t types */
typedef enum nodetype {
    N_INTEGER	= 1,
    N_NAME,
    N_PLUS,
    N_MINUS,
    N_STAR,
    N_SLASH,
    N_AVG,
    N_AVG_INST,
    N_AVG_SAMPLE,
    N_COUNT,
    N_DELTA,
    N_MAX,
    N_MAX_INST,
    N_MAX_SAMPLE,
    N_MIN,
    N_MIN_INST,
    N_MIN_SAMPLE,
    N_SUM,
    N_SUM_INST,
    N_SUM_SAMPLE,
    N_STDEV_INST,
    N_STDEV_SAMPLE,
    N_TOPK_INST,
    N_TOPK_SAMPLE,
    N_NTH_PERCENTILE_INST,
    N_NTH_PERCENTILE_SAMPLE,
    N_ANON,
    N_RATE,
    N_INSTANT,
    N_DOUBLE,
    N_LT,
    N_LEQ,
    N_EQ,
    N_GLOB,
    N_GEQ,
    N_GT,
    N_NEQ,
    N_AND,
    N_OR,
    N_REQ,
    N_RNE,
    N_NEG,
    N_STRING,
    N_RESCALE,
    N_SCALE,
    N_DEFINED,
    N_ABS,
    N_FLOOR,
    N_LOG,
    N_SQRT,
    N_ROUND,

/* node_t time-related sub-types */
    N_RANGE = 100,
    N_INTERVAL,
    N_TIMEZONE,
    N_START,
    N_FINISH,
    N_SAMPLES,
    N_ALIGN,
    N_OFFSET,

/* node_t name-related sub-types */
    N_QUERY = 200,
    N_LABEL,
    N_METRIC,
    N_CONTEXT,
    N_INSTANCE,

    MAX_NODETYPE
} nodetype_t;

typedef struct seriesGetSID {
    seriesBatonMagic	header;		/* MAGIC_SID */
    sds			name;		/* series or source SID */
    sds			metric;		/* back-pointer for instance series */
    /* various flags */
    unsigned int	freed : 1;	/* freed individually on completion */
    void		*baton;
} seriesGetSID;

typedef struct meta {
    int			type;	/* PM_TYPE_* */
    int			sem;	/* PM_SEM_* */
    pmUnits		units;
} meta_t;

typedef struct series_set {
    unsigned char	*series;
    int			nseries;
} series_set_t;

typedef struct series_instance_set {
    /* Number of series instances */
    int			num_instances;
    pmSeriesValue	*series_instance;
} series_instance_set_t;

typedef struct series_sample_set {
    seriesGetSID		*sid;
    sds				metric_name;
    pmSeriesDesc		series_desc;
    void			*baton;
    int				compatibility;
    /* Number of series samples */
    int				num_samples;
    series_instance_set_t	*series_sample;
} series_sample_set_t;

typedef struct series_value_set {
    /* Number of series identifiers*/
    int				num_series;
    series_sample_set_t		*series_values;
} series_value_set_t;


typedef struct timing {
    /* input string */
    pmSeriesTimeWindow	window;

    /* parsed inputs */
    struct timeval	delta;	
    struct timeval	align;
    struct timeval	start;
    struct timeval	end;
    unsigned int	count;		/* sample count */
    unsigned int	offset;		/* sample offset */
    int			zone;		/* pmNewZone handle */
} timing_t;


typedef struct node {
    enum nodetype	type;
    enum nodetype	subtype;
    void		*baton;

    sds			key;
    sds 		value;
    struct node		*left;
    struct node		*right;
    struct meta		meta;

    /* result set of series at this node */
    struct series_set	result;

    /* partial match data for glob/regex */
    int			nmatches;
    sds			*matches;
    regex_t		regex;	/* compiled regex */
    unsigned long long	cursor;

    /* result set of time series values at this node */
    series_value_set_t	value_set;

    /* Corresponding time specifier */
    timing_t		time;
} node_t;


typedef struct series {
    sds			name;
    node_t		*expr;
    timing_t		time;
} series_t;

extern int series_parse(sds, series_t *, char **, void *);
extern int series_solve(pmSeriesSettings *, node_t *, timing_t *, pmSeriesFlags, void *);
extern int series_load(pmSeriesSettings *, node_t *, timing_t *, pmSeriesFlags, void *);
extern void series_stats_inc(pmSeriesSettings *, unsigned int);

extern const char *series_instance_name(sds);
extern const char *series_context_name(sds);
extern const char *series_metric_name(sds);
extern const char *series_label_name(sds);

#endif	/* SERIES_QUERY_H */
