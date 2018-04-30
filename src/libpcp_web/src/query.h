/*
 * Copyright (c) 2017-2018 Red Hat.
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
#include "series.h"

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
    N_COUNT,
    N_DELTA,
    N_MAX,
    N_MIN,
    N_SUM,
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

typedef struct meta {
    int			type;	/* PM_TYPE_* */
    int			sem;	/* PM_SEM_* */
    pmUnits		units;
} meta_t;

typedef struct series_set {
    unsigned char	*series;
    int			nseries;
} series_set_t;

typedef struct node {
    enum nodetype	type;
    enum nodetype	subtype;
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
    unsigned long long	cursor;
} node_t;

typedef struct timing {
    /* input string */
    sds			deltas;
    sds			aligns;
    sds			starts;
    sds			ends;
    sds			ranges;
    sds			counts;
    sds			offsets;
    sds			zones;

    /* parsed inputs */
    struct timeval	delta;	
    struct timeval	align;
    struct timeval	start;
    struct timeval	end;
    int			count;		/* sample count */
    int			offset;		/* sample offset */
    int			zone;		/* pmNewZone handle */
} timing_t;

typedef struct series {
    sds			name;
    node_t		*expr;
    timing_t		time;
} series_t;

typedef pmSeriesSettings settings_t;

extern int series_solve(settings_t *, node_t *, timing_t *, pmflags, void *);
extern int series_source(settings_t *, node_t *, timing_t *, pmflags, void *);

extern const char *series_instance_name(sds);
extern const char *series_context_name(sds);
extern const char *series_metric_name(sds);
extern const char *series_label_name(sds);

#endif	/* SERIES_QUERY_H */
