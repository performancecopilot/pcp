/*
 * Copyright (c) 2017-2018 Red Hat.
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
#include "series.h"

typedef struct context {
    sds			name;		/* source archive or hostspec */
    sds			host;		/* hostname from archive/host */
    sds			origin;		/* host where series loaded in */
    unsigned char	hash[20];	/* context labels metadata SHA1 */
    unsigned int	type	: 7;
    unsigned int	cached	: 1;
    double		location[2];	/* latitude and longitude */
    int			context;	/* PMAPI context */
    long long		hostid;		/* hostname source identifier */
    long long		mapid;		/* internal source identifier */
    pmInDom		mapinst;	/* instance name identifier map */
    pmInDom		mapnames;	/* label name identifier map */
    pmInDom		mapvalues;	/* label value identifier map */
    int			nmetrics;	/* TODO remove nmetrics/metrics */
    const char		**metrics;	/* metric specification strings */
    pmLabelSet		*labels;
} context_t;

typedef struct domain {
    unsigned int	domain;
    context_t		*context;
    pmLabelSet		*labels;
} domain_t;

typedef struct indom {
    pmInDom		indom;
    domain_t		*domain;
    pmLabelSet		*labels;
} indom_t;

typedef struct cluster {
    unsigned int	cluster;
    domain_t		*domain;
    pmLabelSet		*labels;
} cluster_t;

typedef struct labellist {
    long long		nameid;
    long long		valueid;
    sds			name;
    sds			value;
    unsigned int	flags;
    struct labellist	*next;
    struct dict		*valuemap;
    void		*arg;
} labellist_t;

typedef struct value {
    unsigned char	hash[20];	/* SHA1 of intrinsic metadata */
    int			inst;		/* internal instance identifier */
    long long		mapid;		/* internal name identifier */
    unsigned int	cached : 1;	/* metadata is already cached */
    unsigned int	updated : 1;	/* last sample modified value */
    unsigned int	padding : 30;	/* zero-fill structure padding */
    labellist_t		*labellist;	/* label name/value mapping set */
    char		*name;		/* external instance name or "?" */
    pmLabelSet		*labels;	/* instance labels or NULL */
    pmAtomValue		atom;		/* most recent sampled value */
} value_t;

typedef struct instlist {
    unsigned int	listsize;	/* high-water-mark inst count */
    unsigned int	listcount;	/* currently init'd inst count */
    struct value	value[0];
} instlist_t;

typedef struct metric {
    unsigned char	hash[20];	/* SHA1 of intrinsic metadata */
    cluster_t		*cluster;
    indom_t		*indom;
    pmDesc		desc;
    pmLabelSet		*labels;	/* metric item labels or NULL */
    long long		*mapids;	/* internal name(s) identifiers */
    char		**names;	/* PMNS entries for this metric */
    labellist_t		*labellist;	/* label name/value mapping set */
    unsigned int	numnames : 16;	/* count of metric PMNS entries */
    unsigned int	padding : 14;	/* zero-fill structure padding */
    unsigned int	updated : 1;	/* last sample returned success */
    unsigned int	cached : 1;	/* metadata is already cached */
    int			error;		/* a PMAPI negative error code */
    union {
	pmAtomValue	atom;		/* singleton value (PM_IN_NULL) */
	struct instlist *inst;		/* instance values and metadata */
    } u;
} metric_t;

struct seriesLoadBaton;
extern void doneSeriesLoadBaton(struct seriesLoadBaton *);
extern void setSeriesLoadBatonRef(struct seriesLoadBaton *, unsigned int);
extern void incSeriesLoadBatonRef(struct seriesLoadBaton *);
extern void decSeriesLoadBatonRef(struct seriesLoadBaton *);

extern void *seriesLoadBatonSlots(struct seriesLoadBaton *);
extern pmSeriesInfoCallBack seriesLoadBatonInfo(struct seriesLoadBaton *);
extern context_t *seriesLoadBatonContext(struct seriesLoadBaton *);
extern void *seriesLoadBatonUser(struct seriesLoadBaton *);

#endif	/* SERIES_LOAD_H */
