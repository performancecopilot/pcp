/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifndef TRACE_DATA_H
#define TRACE_DATA_H

#include "hash.h"
#include "trace.h"
#include "trace_dev.h"

typedef struct {
    char		*tag;
    unsigned int	type;
    unsigned int	instid;
} instdata_t;

typedef struct __pmHashTab hashtable_t;

int instcmp(void *a, void *b);
void instdel(void *a);
void instprint(hashtable_t *t, void *e);

typedef struct {
    char		*tag;
    unsigned int	id;
    int			fd;
    unsigned int	tracetype : 8;
    unsigned int	taglength : 8;
    unsigned int	padding   : 16;
    __uint64_t		realcount;	/* real total seen by the PMDA */
    double		realtime;	/* total time for transactions */
    __int32_t		txcount;	/* count this interval or -1   */
    double		txmin;		/* minimum value this interval */
    double		txmax;		/* maximum value this interval */
    double		txsum;		/* summed across the interval  */
} hashdata_t;

int datacmp(void *a, void *b);
void datadel(void *a);
void dataprint(hashtable_t *t, void *e);

typedef struct {
    unsigned int	numstats : 8;	/* number of entries in this table */
    unsigned int	working  : 1;	/* this the current working table? */
    hashtable_t		*stats;		
} statlist_t;

typedef struct {
    statlist_t		*ring;		/* points to all statistics */
    unsigned int	level;		/* controls reporting level */
} ringbuf_t;

#ifdef PCP_DEBUG
void debuglibrary(int);
#endif

#endif	/* TRACE_DATA_H */
