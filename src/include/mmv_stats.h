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

#define MMV_VERSION	1
#define MMV_NAMEMAX	64
#define MMV_STRINGMAX	256

/* Initial entries should match PM_TYPE_* from <pcp/pmapi.h> */
typedef enum {
    MMV_ENTRY_NOSUPPORT = -1,	/* not implemented in this version */
    MMV_ENTRY_I32       =  0,	/* 32-bit signed integer */
    MMV_ENTRY_U32       =  1,	/* 32-bit unsigned integer */
    MMV_ENTRY_I64       =  2,	/* 64-bit signed integer */
    MMV_ENTRY_U64       =  3,	/* 64-bit unsigned integer */
    MMV_ENTRY_FLOAT     =  4,	/* 32-bit floating point */
    MMV_ENTRY_DOUBLE    =  5,	/* 64-bit floating point */
    MMV_ENTRY_STRING    =  6,	/* NULL-terminate string */
    MMV_ENTRY_INTEGRAL  = 10,	/* timestamp & number of outstanding */
} mmv_metric_type_t;

/* These all directly match PM_SEM_* from <pcp/pmapi.h> */
typedef enum {
    MMV_SEM_COUNTER	= 1,	/* cumulative counter (monotonic increasing) */
    MMV_SEM_INSTANT	= 3,	/* instantaneous value, continuous domain */
    MMV_SEM_DISCRETE	= 4,	/* instantaneous value, discrete domain */
} mmv_metric_sem_t;

typedef enum {
    MMV_FLAG_PREFIX	= 0x1,	/* indicates names should be mmv-prefixed */
    MMV_FLAG_PROCESS	= 0x2,	/* indicates process check on PID needed */
} mmv_stats_flags_t;

typedef enum {
    MMV_TOC_INDOM	= 1,
    MMV_TOC_METRICS	= 2,
    MMV_TOC_VALUES	= 3,
    MMV_TOC_STRING	= 4,
} mmv_toc_type_t;

/* The way the Table Of Contents is written into the file */
typedef struct {
    mmv_toc_type_t	typ;		/* What is it? */
    __int32_t		cnt;		/* Number of entries */
    __uint64_t		offset;		/* Offset of section from start of the file */
} mmv_stats_toc_t;

typedef struct {
    __int32_t		internal;		/* Internal instance ID for this domain */
    char		external[MMV_NAMEMAX];	/* External instance ID for this domain */
} mmv_stats_inst_t;

typedef struct {
    char		payload[MMV_STRINGMAX];	/* NULL terminated string or binary data */
} mmv_stats_string_t;

/* This is the in-core structure for mmv_stats_init */
typedef struct {
    char		name[MMV_NAMEMAX];	/* Name of the metric */
    __uint32_t		item;			/* item component of PMID (unique) */
    mmv_metric_type_t	type;			/* Type of the metric */
    mmv_metric_sem_t	semantics;		/* Semantics (counter, discrete, etc) */
    pmUnits		dimension;		/* Dimensions (TIME, SPACE, etc) */
    mmv_stats_inst_t *	indom;			/* Pointer to the array of
						 * mmv_stats_inst_t, terminated by
						 * internal=-1, or NULL */
    char *		shorttext;		/* One line help text string */
    char *		helptext;		/* Detailed help text (256 chars max) */
} mmv_stats_t;

/* This is the on-disk structure for each metric */
typedef struct {
    char		name[MMV_NAMEMAX];
    __uint32_t		item;
    mmv_metric_type_t	type;
    mmv_metric_sem_t	semantics;
    pmUnits		dimension;
    __int32_t		indom;
    __int32_t		padding;
    __uint64_t		shorttext;	/* Offset of short help text string */
    __uint64_t		helptext;	/* Offset of long help text string */
} mmv_stats_metric_t;

typedef struct {
    __uint64_t		metric;		/* Offset into the files metric section */
    __uint64_t		instance;	/* Offset into the files indom section */
    union {
	__int32_t	i32;
	__uint32_t	u32;
	__int64_t	i64;
	__uint64_t	u64;
	float		f;
	double		d;
    } val;
    __int64_t		extra;		/* INTEGRAL (starttime) and STRING (offset) */
} mmv_stats_value_t;

typedef struct {
    char		magic[4];	/* MMV\0 */
    __int32_t		version;	/* version */
    __uint64_t		g1;		/* Generation numbers - use time(2) to init */
    __uint64_t		g2;
    __int32_t		tocs;		/* Number of toc entries */
    mmv_stats_flags_t	flags;
    __int32_t		process;	/* client process identifier (flags) */
    __int32_t		cluster;	/* preferred PMDA cluster identifier */
} mmv_stats_hdr_t;

extern void * mmv_stats_init (const char *, const mmv_stats_t *, int, mmv_stats_flags_t, int);
extern mmv_stats_value_t * mmv_lookup_value_desc (void *, const char *, const char *);
extern void mmv_inc_value (void *, mmv_stats_value_t *, double);
extern void mmv_set_string (void *, mmv_stats_value_t *, const char *, int);

#ifndef MMV_STATS_STATIC_ADD
#define MMV_STATS_STATIC_ADD(hndl,metric,instance,count)                  \
if (hndl != NULL) {                                                       \
    static mmv_stats_value_t * __mmv_##metric;                            \
    __mmv_##metric = mmv_lookup_value_desc (hndl,#metric,instance);       \
    if (__mmv_##metric) mmv_inc_value (hndl, __mmv_##metric, count);      \
}
#endif

#ifndef MMV_STATS_STATIC_INC
#define MMV_STATS_STATIC_INC(hndl,metric,instance)                        \
    MMV_STATS_STATIC_ADD(hndl,metric,instance,1)
#endif

#ifndef MMV_STATS_ADD
#define MMV_STATS_ADD(hndl,metric,instance,count)                         \
if ( hndl != NULL ) {                                                     \
    mmv_stats_value_t * __mmv_##metric =                                  \
        mmv_lookup_value_desc (hndl,#metric,instance);                    \
    if (__mmv_##metric) mmv_inc_value (hndl, __mmv_##metric, count);      \
}
#endif

#ifndef MMV_STATS_INC
#define MMV_STATS_INC(hndl,metric,instance)                               \
    MMV_STATS_ADD(hndl,metric,instance,1)
#endif

#ifndef MMV_STATS_ADD_FALLBACK
#define MMV_STATS_ADD_FALLBACK(hndl,metric,instance,instance2,count)      \
if  (hndl != NULL ) {                                                     \
    mmv_stats_value_t * __mmv_##metric =                                  \
        mmv_lookup_value_desc (hndl,#metric,instance);                    \
    if (__mmv_##metric == NULL )                                          \
        __mmv_##metric = mmv_lookup_value_desc (hndl,#metric,instance2);  \
    if (__mmv_##metric) mmv_inc_value (hndl, __mmv_##metric, count);      \
}
#endif

#ifndef MMV_STATS_INC_FALLBACK
#define MMV_STATS_INC_FALLBACK(hndl,metric,instance,instance2)           \
    MMV_STATS_ADD_FALLBACK(hndl,metric,instance,instance2,1)
#endif

#ifndef MMV_STATS_INTERVAL_START
#define MMV_STATS_INTERVAL_START(hndl,vptr,metric,instance)              \
if ( hndl != NULL ) {                                                    \
    if ( vptr == NULL ) {                                                \
	vptr = mmv_lookup_value_desc (hndl, #metric, instance);          \
    }                                                                    \
    if ( vptr != NULL ) {                                                \
	struct timeval tv;                                               \
	gettimeofday (&tv, NULL);                                        \
	mmv_inc_value (hndl, vptr, -(tv.tv_sec*1e6 + tv.tv_usec));       \
    }                                                                    \
}
#endif

#ifndef MMV_STATS_INTERVAL_END
#define MMV_STATS_INTERVAL_END(hndl, vptr)                               \
if ( vptr != NULL ) {                                                    \
    struct timeval tv;                                                   \
    gettimeofday (&tv, NULL);                                            \
    mmv_inc_value (hndl, vptr, (tv.tv_sec*1e6 + tv.tv_usec));            \
}
#endif

#ifndef MMV_STATS_SET_STRING
#define MMV_STATS_SET_STRING(hndl,metric,instance,string)                 \
if ( hndl != NULL ) {                                                     \
    mmv_stats_value_t * __mmv_##metric =                                  \
        mmv_lookup_value_desc (hndl,#metric,instance);                    \
    if (__mmv_##metric)                                                   \
	mmv_set_string (hndl, __mmv_##metric, string, strlen(string));    \
}
#endif

#ifndef MMV_STATS_SET_STRLEN
#define MMV_STATS_SET_STRLEN(hndl,metric,instance,string,size)            \
if ( hndl != NULL ) {                                                     \
    mmv_stats_value_t * __mmv_##metric =                                  \
        mmv_lookup_value_desc (hndl,#metric,instance);                    \
    if (__mmv_##metric)                                                   \
	mmv_set_string (hndl, __mmv_##metric, string, size);              \
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* _MMV_STATS_H */
