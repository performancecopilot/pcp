/*
 * Copyright (c) 2017 Red Hat.
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
#ifndef SERIES_LOAD_H
#define SERIES_LOAD_H

#include "pmapi.h"
#include "impl.h"
#include "libpcp.h"

#undef HASHSIZE
#define HASHSIZE	40	/* SHA1: 40 for convenience, could be 20 */
#undef HBUFSIZE
#define HBUFSIZE	HASHSIZE+2

typedef struct context {
    int			type;
    int			context;
    const char		*source;	/* stored as a "note" (e.g. log) */
    const char		**metrics;	/* metric specification strings */
    int			nmetrics;
    /*int		markcount;	-- num mark records seen (move?) */
    pmLabelSet		*labels;
} context_t;

typedef struct domain {
    unsigned int	domain;
    pmLabelSet		*labels;
    context_t		*context;
} domain_t;

typedef struct indom {
    pmInDom		indom;
    pmLabelSet		*labels;
    domain_t		*domain;
} indom_t;

typedef struct cluster {
    unsigned int	cluster;
    pmLabelSet		*labels;
    domain_t		*domain;
} cluster_t;

typedef struct metric {
    pmDesc		desc;
    pmLabelSet		*labels;
    char		**names;
    int			*mapids;
    unsigned int	numnames;
    int			outype;
    double		scale;
    struct value	**vlist;
    unsigned int	listsize;
    cluster_t		*cluster;
    indom_t		*indom;
} metric_t;

typedef struct value {
    int			inst;		/* instance ID or PM_IN_NULL */
    char		*name;		/* instance name or NULL */
    pmLabelSet		*labels;	/* instance labels of NULL */
    char		hash[HBUFSIZE];	/* SHA1 of mandatory metadata */
    unsigned int	cached:1;	/* metadata is already cached */
    unsigned int	marked:1;	/* seen since last "mark" record? */
    unsigned int	markcount;	/* num mark records seen (move?) */
    unsigned int	count;		/* total number of samples */
    struct timeval	firsttime;	/* time of first sample */
    struct timeval	lasttime;	/* time of previous sample */
    pmAtomValue		lastval;	/* value from previous sample */
} value_t;

#endif	/* SERIES_LOAD_H */
