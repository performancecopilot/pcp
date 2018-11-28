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
#ifndef SERIES_LOAD_H
#define SERIES_LOAD_H

#include "pmapi.h"
#include "pmwebapi.h"

#ifdef HAVE_LIBUV
#include <uv.h>
#endif

typedef struct seriesname {
    sds			sds;		/* external name for the series */
    unsigned char	id[20];		/* SHA1 of external series name */
    unsigned char	hash[20];	/* SHA1 of intrinsic metadata */
} seriesname_t;

typedef struct context {
    seriesname_t	name;		/* source archive or hostspec */
    sds			host;		/* hostname from archive/host */
    sds			origin;		/* host where series loaded in */
    unsigned char	hostid[20];	/* SHA1 of host identifier */
    double		location[2];	/* latitude and longitude */
    unsigned int	type	: 7;	/* PMAPI context type */
    unsigned int	setup	: 1;	/* context established */
    unsigned int	cached	: 1;	/* context/source in cache */
    unsigned int	garbage	: 1;	/* context pending removal */
    unsigned int	updated : 1;	/* context labels are updated */
    unsigned int	padding : 21;	/* zero-filled struct padding */
    unsigned int	timeout;	/* context timeout in milliseconds */
#ifdef HAVE_LIBUV
    uv_timer_t		timer;
#endif
    int			context;	/* PMAPI context handle */
    int			randomid;	/* random number identifier */
    struct dict		*pmids;		/* metric pmID to metric struct */
    struct dict		*metrics;	/* metric names to metric struct */
    struct dict		*indoms;	/* indom number to indom struct */
    struct dict		*domains;	/* domain number to domain struct */
    struct dict		*clusters;	/* domain+cluster to cluster struct */
    sds			labels;		/* context labelset as string */
    pmLabelSet		*labelset;	/* labelset at context level */
} context_t;

typedef struct domain {
    unsigned int	domain;
    unsigned int	updated;	/* domain labels are updated */
    context_t		*context;
    pmLabelSet		*labelset;
} domain_t;

typedef struct labellist {
    unsigned char	nameid[20];
    unsigned char	valueid[20];
    sds			name;
    sds			value;
    unsigned int	flags;
    struct labellist	*next;
    struct dict		*valuemap;
    void		*arg;
} labellist_t;

typedef struct instance {
    seriesname_t	name;		/* instance naming information */
    unsigned int	inst;		/* internal instance identifier */
    unsigned int	cached : 1;	/* metadata is already cached */
    unsigned int	updated : 1;	/* instance labels are updated */
    unsigned int	padding : 30;
    sds			labels;		/* fully merged inst labelset */
    pmLabelSet		*labelset;	/* labels at inst level or NULL */
    labellist_t		*labellist;	/* label name/value mapping set */
} instance_t;

typedef struct indom {
    pmInDom		indom;
    domain_t		*domain;
    unsigned int	cached : 1;	/* metadata written into cache */
    unsigned int	updated : 1;	/* instance labels are updated */
    unsigned int	padding : 30;	/* zero-fill structure padding */
    sds			helptext;	/* indom help text (optional) */
    sds			oneline;	/* indom oneline text (optional) */
    sds			labels;		/* fully merged indom labelset */
    pmLabelSet		*labelset;	/* labels at indom level or NULL */
    struct dict		*insts;		/* map identifiers to instances */
} indom_t;

typedef struct cluster {
    unsigned int	cluster;
    unsigned int	updated;	/* cluster labels are updated */
    domain_t		*domain;
    pmLabelSet		*labelset;
} cluster_t;

typedef struct value {
    int			inst;		/* internal instance identifier */
    unsigned int	updated;	/* last sample modified value */
    pmAtomValue		atom;		/* most recent sampled value */
} value_t;

typedef struct valuelist {
    unsigned int	listsize;	/* high-water-mark inst count */
    unsigned int	listcount;	/* currently init'd inst count */
    value_t		value[0];
} valuelist_t;

typedef struct metric {
    pmDesc		desc;
    cluster_t		*cluster;
    indom_t		*indom;
    sds			helptext;	/* metric help text (optional) */
    sds			oneline;	/* oneline help text (optional) */
    sds			labels;		/* fully merged metric labelset */
    pmLabelSet		*labelset;	/* metric item labels or NULL */
    labellist_t		*labellist;	/* label name/value mapping set */
    seriesname_t	*names;		/* metric names and mappings */
    unsigned int	numnames : 16;	/* count of metric PMNS entries */
    unsigned int	padding : 14;	/* zero-fill structure padding */
    unsigned int	updated : 1;	/* last sample returned success */
    unsigned int	cached : 1;	/* metadata written into cache */
    int			error;		/* a PMAPI negative error code */
    union {
	pmAtomValue	atom;		/* singleton value (PM_IN_NULL) */
	valuelist_t	*vlist;		/* instance values and metadata */
    } u;
} metric_t;

struct seriesGetContext;
extern void doneSeriesGetContext(struct seriesGetContext *, const char *);

struct seriesLoadBaton;
extern void doneSeriesLoadBaton(struct seriesLoadBaton *, const char *);

extern context_t *seriesLoadBatonContext(struct seriesLoadBaton *);
extern void seriesLoadBatonFetch(struct seriesLoadBaton *);

#endif	/* SERIES_LOAD_H */
