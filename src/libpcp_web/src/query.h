/*
 * Copyright (c) 2017 Red Hat.
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

typedef struct meta {
    int			type;	/* PM_TYPE_* */
    int			sem;	/* PM_SEM_* */
    pmUnits		units;
} meta_t;

typedef struct node {
    int			type;
    int			subtype;
    char		*value;
    struct node		*left;
    struct node		*right;
    struct meta		meta;
    char		*key;
    int			nseries;
    void		*series;
} node_t;

typedef struct timing {
    /* input string */
    char		*deltas;
    char		*aligns;
    char		*starts;
    char		*ends;
    char		*ranges;
    char		*counts;
    char		*offsets;
    char		*zones;

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
    char		*name;
    node_t		*expr;
    timing_t		time;
} series_t;

typedef pmSeriesSettings settings_t;
typedef pmseries_flags flags_t;

extern int series_solve(settings_t *, node_t *, timing_t *, flags_t, void *);
extern int series_source(settings_t *, node_t *, timing_t *, flags_t, void *);

extern char *series_instance_name(char *, size_t);
extern char *series_metric_name(char *, size_t);
extern char *series_label_name(char *, size_t);
extern char *series_note_name(char *, size_t);

/* node_t types */
#define N_INTEGER	1
#define N_NAME		2
#define N_PLUS		3
#define N_MINUS		4
#define N_STAR		5
#define N_SLASH		6
#define N_AVG		7
#define N_COUNT		8
#define N_DELTA		9
#define N_MAX		10
#define N_MIN		11
#define N_SUM		12
#define N_ANON		13
#define N_RATE		14
#define N_INSTANT	15
#define N_DOUBLE	16
#define N_LT		17
#define N_LEQ		18
#define N_EQ		19
#define N_GEQ		20
#define N_GT		21
#define N_NEQ		22
#define N_AND		23
#define N_OR		24
#define N_REQ		25
#define N_RNE		26
#define N_NEG		27
#define N_STRING	28
#define N_RESCALE	29
#define N_SCALE		30
#define N_DEFINED	31

/* node_t time-related sub-types */
#define N_RANGE		100
#define N_INTERVAL	101
#define N_TIMEZONE	102
#define N_START		103
#define N_FINISH	104
#define N_SAMPLES	105
#define N_ALIGN		106
#define N_OFFSET	107

/* node_t name-related sub-types */
#define N_QUERY		200
#define N_METRIC	201
#define N_INSTANCE	202
#define N_LABEL		203

#endif	/* SERIES_QUERY_H */
