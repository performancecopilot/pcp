/*
 * pmval - simple performance metrics value dumper
 *
 * Copyright (c) 2014-2015 Red Hat.
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef PMVAL_H
#define PMVAL_H

#include "pmapi.h"
#include "libpcp.h"
#include "pmtime.h"

/* instance id - instance name association */
typedef struct {
    int		id;		/* internal instance identifier */
    char	*name;		/* external instance identifier */
} InstPair;

/* performance metrics in the hash table */
typedef struct {
    char	*name;		/* name of metric */
    pmDesc	desc;		/* metric description */
} DescHash;

/* full description of a performance metric */
typedef struct {
    /* external (printable) description */
    const char	*hostname;
    char	*metric;	/* name of metric */
    int		iall;		/* all instances */
    int		inum;		/* number of instances */
    char	**inames;	/* list of instance names */
    /* internal description */
    int		handle;		/* context handle */
    pmID	pmid;		/* metric identifier */
    pmDesc	desc;		/* metric description */
    float	scale;		/* conversion factor for rate */
    int		*iids;		/* list of instance ids */
    /* internal-external association */
    InstPair	*ipairs;	/* sorted array of id-name */
    /* internal (event) metrics fields/parameters */
    __pmHashCtl	ihash;		/* component map (DescHash) */
    char	*filter;	/* server-side filter value */
} Context;

extern int verbose;
extern int archive;

extern void printevents(Context *, pmValueSet *, int);

#endif	/* PMVAL_H */
