/*
 * Copyright (C) 2001,2009 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (C) 2009 Aconex.  All Rights Reserved.
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
#ifndef _MMV_STATS_H
#define _MMV_STATS_H

#include <pcp/pmapi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MMV_NAMEMAX	64
#define MMV_STRINGMAX	256

/* Initial entries should match PM_TYPE_* from <pcp/pmapi.h> */
typedef enum {
    MMV_ENTRY_NOSUPPORT = -1,	/* Not implemented in this version */
    MMV_ENTRY_I32       =  0,	/* 32-bit signed integer */
    MMV_ENTRY_U32       =  1,	/* 32-bit unsigned integer */
    MMV_ENTRY_I64       =  2,	/* 64-bit signed integer */
    MMV_ENTRY_U64       =  3,	/* 64-bit unsigned integer */
    MMV_ENTRY_FLOAT     =  4,	/* 32-bit floating point */
    MMV_ENTRY_DOUBLE    =  5,	/* 64-bit floating point */
    MMV_ENTRY_STRING    =  6,	/* NULL-terminate string */
    MMV_ENTRY_INTEGRAL  = 10,	/* Timestamp & number of outstanding */
} mmv_metric_type_t;

/* These all directly match PM_SEM_* from <pcp/pmapi.h> */
typedef enum {
    MMV_SEM_COUNTER	= 1,	/* Cumulative counter (monotonic increasing) */
    MMV_SEM_INSTANT	= 3,	/* Instantaneous value, continuous domain */
    MMV_SEM_DISCRETE	= 4,	/* Instantaneous value, discrete domain */
} mmv_metric_sem_t;

typedef struct {
    __int32_t		internal;	/* Internal instance ID */
    char		external[MMV_NAMEMAX];	/* External instance ID */
} mmv_instances_t;

typedef struct {
    __uint32_t		serial;		/* Unique identifier */
    __uint32_t		count;		/* Number of instances */
    mmv_instances_t *	instances;	/* Internal/external IDs */
    char *		shorttext;	/* Short help text string */
    char *		helptext;	/* Long help text string */
} mmv_indom_t;

typedef struct {
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

typedef enum {
    MMV_FLAG_NOPREFIX	= 0x1,	/* Don't prefix metric names by filename */
    MMV_FLAG_PROCESS	= 0x2,	/* Indicates process check on PID needed */
} mmv_stats_flags_t;

extern void * mmv_stats_init(const char *, int, mmv_stats_flags_t,
				const mmv_metric_t *, int,
				const mmv_indom_t *, int);
extern pmAtomValue * mmv_lookup_value_desc(void *, const char *, const char *);
extern void mmv_inc_value(void *, pmAtomValue *, double);
extern void mmv_set_string(void *, pmAtomValue *, const char *, int);

static inline void
mmv_stats_static_add(void *__handle,
	const char *__metric, const char *__instance, double __count)
{
    if (__handle) {
	static pmAtomValue * __mmv_metric;
	__mmv_metric = mmv_lookup_value_desc(__handle, __metric, __instance);
	if (__mmv_metric)
	    mmv_inc_value(__handle, __mmv_metric, __count);
    }
}

static inline void
mmv_stats_static_inc(void *__handle,
	const char *__metric, const char *__instance)
{
    mmv_stats_static_add(__handle, __metric, __instance, 1);
}

static inline void
mmv_stats_add(void *__handle,
	const char *__metric, const char *__instance, double __count)
{
    if (__handle) {
	pmAtomValue * __mmv_metric;
	__mmv_metric = mmv_lookup_value_desc(__handle, __metric, __instance);
	if (__mmv_metric)
	    mmv_inc_value(__handle, __mmv_metric, __count);
    }
}

static inline void
mmv_stats_inc(void *__handle,
	const char *__metric, const char *__instance)
{
    mmv_stats_add(__handle, __metric, __instance, 1);
}

static inline void
mmv_stats_add_fallback(void *__handle, const char *__metric,
	const char *__instance, const char *__instance2, double __count)
{
    if (__handle) {
	pmAtomValue * __mmv_metric;
	__mmv_metric = mmv_lookup_value_desc(__handle, __metric, __instance);
	if (__mmv_metric == NULL)
	    __mmv_metric = mmv_lookup_value_desc(__handle,__metric,__instance2);
	if (__mmv_metric)
	    mmv_inc_value(__handle, __mmv_metric, __count);
    }
}

static inline void
mmv_stats_inc_fallback(void *__handle, const char *__metric,
	const char *__instance, const char *__instance2)
{
    mmv_stats_add_fallback(__handle, __metric, __instance, __instance2, 1);
}

static inline pmAtomValue *
mmv_stats_interval_start(void *__handle, pmAtomValue *__value,
	const char *__metric, const char *__instance)
{
    if (__handle) {
	if (__value == NULL)
	    __value = mmv_lookup_value_desc(__handle, __metric, __instance);
	if (__value) {
	    struct timeval __tv;
	    gettimeofday(&__tv, NULL);
	    mmv_inc_value(__handle, __value, -(__tv.tv_sec*1e6 + __tv.tv_usec));
	}
    }
    return __value;
}

static inline void
mmv_stats_interval_end(void *__handle, pmAtomValue *__value)
{
    if (__value && __handle) {
	struct timeval __tv;
	gettimeofday(&__tv, NULL);
	mmv_inc_value(__handle, __value, (__tv.tv_sec*1e6 + __tv.tv_usec));
    }
}

static inline void
mmv_stats_set_string(void *__handle, const char *__metric,
	const char *__instance, const char *__string)
{
    if (__handle) {
	size_t __len = strlen(__string);
	pmAtomValue *__mmv_metric;
	__mmv_metric = mmv_lookup_value_desc(__handle, __metric, __instance);
	mmv_set_string(__handle, __mmv_metric, __string, __len);
    }
}

static inline void
mmv_stats_set_strlen(void *__handle, const char *__metric,
	const char *__instance, const char *__string, size_t __len)
{
    if (__handle) {
	pmAtomValue *__mmv_metric;
	__mmv_metric = mmv_lookup_value_desc(__handle, __metric, __instance);
	mmv_set_string(__handle, __mmv_metric, __string, __len);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* _MMV_STATS_H */
