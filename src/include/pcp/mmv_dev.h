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
#ifndef PCP_MMV_DEV_H
#define PCP_MMV_DEV_H

#define MMV_VERSION	1

typedef enum mmv_toc_type {
    MMV_TOC_INDOMS	= 1,	/* mmv_disk_indom_t */
    MMV_TOC_INSTANCES	= 2,	/* mmv_disk_instance_t */
    MMV_TOC_METRICS	= 3,	/* mmv_disk_metric_t */
    MMV_TOC_VALUES	= 4,	/* mmv_disk_value_t */
    MMV_TOC_STRINGS	= 5,	/* mmv_disk_string_t */
} mmv_toc_type_t;

/* The way the Table Of Contents is written into the file */
typedef struct mmv_disk_toc {
    mmv_toc_type_t	type;		/* What is it? */
    __int32_t		count;		/* Number of entries */
    __uint64_t		offset;		/* Offset of section from file start */
} mmv_disk_toc_t;

typedef struct mmv_disk_indom {
    __uint32_t		serial;		/* Unique identifier */
    __uint32_t		count;		/* Number of instances */
    __uint64_t		offset;		/* Offset of first instance */
    __uint64_t		shorttext;	/* Offset of short help text string */
    __uint64_t		helptext;	/* Offset of long help text string */
} mmv_disk_indom_t;

typedef struct mmv_disk_instance {
    __uint64_t		indom;		/* Offset into files indom section */
    __uint32_t		padding;	/* zero filled, alignment bits */
    __int32_t		internal;	/* Internal instance ID */
    char		external[MMV_NAMEMAX];	/* External instance ID */
} mmv_disk_instance_t;

typedef struct mmv_disk_string {
    char		payload[MMV_STRINGMAX];	/* NULL terminated string */
} mmv_disk_string_t;

typedef struct mmv_disk_metric {
    char		name[MMV_NAMEMAX];
    __uint32_t		item;		/* Unique identifier */
    mmv_metric_type_t	type;
    mmv_metric_sem_t	semantics;
    pmUnits		dimension;
    __int32_t		indom;		/* Instance domain number */
    __uint32_t		padding;	/* zero filled, alignment bits */
    __uint64_t		shorttext;	/* Offset of short help text string */
    __uint64_t		helptext;	/* Offset of long help text string */
} mmv_disk_metric_t;

typedef struct mmv_disk_value {
    pmAtomValue		value;		/* Union of all possible value types */
    __int64_t		extra;		/* INTEGRAL(starttime)/STRING(offset) */
    __uint64_t		metric;		/* Offset into the metric section */
    __uint64_t		instance;	/* Offset into the instance section */
} mmv_disk_value_t;

typedef struct mmv_disk_header {
    char		magic[4];	/* MMV\0 */
    __int32_t		version;	/* version */
    __uint64_t		g1;		/* Generation numbers */
    __uint64_t		g2;
    __int32_t		tocs;		/* Number of toc entries */
    mmv_stats_flags_t	flags;
    __int32_t		process;	/* client process identifier (flags) */
    __int32_t		cluster;	/* preferred PMDA cluster identifier */
} mmv_disk_header_t;

#endif /* PCP_MMV_DEV_H */
