/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
 * Copyright (c) 2011 Red Hat Inc.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#include "pmtime.h"

#define ALL_SAMPLES	-1

/* performance metrics from the command line */
typedef struct {
    char	*name;		/* name of metric */
    pmID	pmid;		/* metric identifier */
    pmDesc	desc;		/* metric description */
    int		ninst;		/* number of instances, 0 for all instances */
    char	**iname;	/* instance names */
    int		*inst;		/* instance ids */
    __pmHashCtl	ihash;		/* mapping when all instances requested */
} metric_t;

void doargs(int, char **);

/*
 * globals ... see declarations in pmevent.c for explanations
 */
extern char		*host; /* as per pmGetContextHostName */
extern char		*archive;
extern int		ahtype;
extern int		ctxhandle;
extern int		verbose;
extern struct timeval	now;
extern struct timeval	first;
extern struct timeval	last;
extern struct timeval	delta;
extern long		samples;
extern int		gui;
extern int		port;
extern pmTimeControls	controls;

extern metric_t		*metrictab;
extern int		nmetric;
extern pmID		*pmidlist;
