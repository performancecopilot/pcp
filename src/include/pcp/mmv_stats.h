/*
 * Copyright (C) 2013 Red Hat.
 * Copyright (C) 2009 Aconex.  All Rights Reserved.
 * Copyright (C) 2001,2009 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef PCP_MMV_STATS_H
#define PCP_MMV_STATS_H

#ifdef __cplusplus
extern "C" {
#endif

#define MMV_NAMEMAX	64
#define MMV_STRINGMAX	256

typedef enum mmv_metric_type {
    MMV_TYPE_NOSUPPORT = PM_TYPE_NOSUPPORT,
    MMV_TYPE_I32       = PM_TYPE_32,	/* 32-bit signed integer */
    MMV_TYPE_U32       = PM_TYPE_U32,	/* 32-bit unsigned integer */
    MMV_TYPE_I64       = PM_TYPE_64,	/* 64-bit signed integer */
    MMV_TYPE_U64       = PM_TYPE_U64,	/* 64-bit unsigned integer */
    MMV_TYPE_FLOAT     = PM_TYPE_FLOAT,	/* 32-bit floating point */
    MMV_TYPE_DOUBLE    = PM_TYPE_DOUBLE,/* 64-bit floating point */
    MMV_TYPE_STRING    = PM_TYPE_STRING,/* NULL-terminate string */
    MMV_TYPE_ELAPSED   = 9,		/* 64-bit elapsed time */
} mmv_metric_type_t;

typedef enum mmv_metric_sem {
    MMV_SEM_COUNTER	= PM_SEM_COUNTER,
    MMV_SEM_INSTANT	= PM_SEM_INSTANT,
    MMV_SEM_DISCRETE	= PM_SEM_DISCRETE,
} mmv_metric_sem_t;

typedef struct mmv_instances {
    __int32_t		internal;	/* Internal instance ID */
    char		external[MMV_NAMEMAX];	/* External instance ID */
} mmv_instances_t;

typedef struct mmv_indom {
    __uint32_t		serial;		/* Unique identifier */
    __uint32_t		count;		/* Number of instances */
    mmv_instances_t *	instances;	/* Internal/external IDs */
    char *		shorttext;	/* Short help text string */
    char *		helptext;	/* Long help text string */
} mmv_indom_t;

typedef struct mmv_metric {
    char		name[MMV_NAMEMAX];
    __uint32_t		item;		/* Unique identifier */
    mmv_metric_type_t	type;
    mmv_metric_sem_t	semantics;
    pmUnits		dimension;
    __uint32_t		indom;		/* Indom serial */
    char *		shorttext;	/* Short help text string */
    char *		helptext;	/* Long help text string */
} mmv_metric_t;

#ifdef HAVE_BITFIELDS_LTOR
#define MMV_UNITS(a,b,c,d,e,f)	{a,b,c,d,e,f,0}
#else
#define MMV_UNITS(a,b,c,d,e,f)	{0,f,e,d,c,b,a}
#endif

typedef enum mmv_stats_flags {
    MMV_FLAG_NOPREFIX	= 0x1,	/* Don't prefix metric names by filename */
    MMV_FLAG_PROCESS	= 0x2,	/* Indicates process check on PID needed */
} mmv_stats_flags_t;

extern void * mmv_stats_init(const char *, int, mmv_stats_flags_t,
				const mmv_metric_t *, int,
				const mmv_indom_t *, int);
extern void mmv_stats_stop(const char *, void *);

extern pmAtomValue * mmv_lookup_value_desc(void *, const char *, const char *);
extern void mmv_inc_value(void *, pmAtomValue *, double);
extern void mmv_set_value(void *, pmAtomValue *, double);
extern void mmv_set_string(void *, pmAtomValue *, const char *, int);

extern void mmv_stats_add(void *, const char *, const char *, double);
extern void mmv_stats_inc(void *, const char *, const char *);
extern void mmv_stats_set(void *, const char *, const char *, double);
extern void mmv_stats_add_fallback(void *, const char *, const char *,
				const char *, double);
extern void mmv_stats_inc_fallback(void *, const char *, const char *,
				const char *);
extern pmAtomValue * mmv_stats_interval_start(void *, pmAtomValue *,
				const char *, const char *);
extern void mmv_stats_interval_end(void *, pmAtomValue *);
extern void mmv_stats_set_string(void *, const char *,
				const char *, const char *);
extern void mmv_stats_set_strlen(void *, const char *,
				const char *, const char *, size_t);

#ifdef __cplusplus
}
#endif

#endif /* PCP_MMV_STATS_H */
