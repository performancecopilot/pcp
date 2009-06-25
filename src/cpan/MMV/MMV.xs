/*
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
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

#include "mmv_stats.h"
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

static mmv_metric_t *
new_metric_list(AV *array, int count)
{
    SV			**entry;
    int			index;
    mmv_metric_t	*table, *data;

    if ((table = calloc(count, sizeof(mmv_metric_t))) == NULL)
	return NULL;

    for (index = 0; index < count; index++) {
	entry = av_fetch(array, index, 0);
	data = (mmv_metric_t *) (*entry);
	table[index] = *data;
    }
    return table;
}

static mmv_indom_t *
new_indom_list(AV *array, int count)
{
    SV			**entry;
    int			index;
    mmv_indom_t		*table, *data;

    if ((table = calloc(count, sizeof(mmv_indom_t))) == NULL)
	return NULL;

    for (index = 0; index < count; index++) {
	entry = av_fetch(array, index, 0);
	data = (mmv_indom_t *) (*entry);
	table[index] = *data;
    }
    return table;
}

static mmv_instances_t *
new_instance_list(AV *array, int count)
{
    SV			**entry;
    int			index;
    char		*data;
    mmv_instances_t	*table;

    if (count % 2)	/* must be internal/external *pairs* */
	return NULL;
    if ((table = calloc(count, sizeof(mmv_instances_t))) == NULL)
	return NULL;
    for (index = 0; index < count; index++) {
	entry = av_fetch(array, index, 0);
	data = (char *) (*entry);
	if ((index % 2) == 0)	/* internal identifier */
	    table[index / 2].internal = atoi(data);
	else			/* external identifier */
	    strncpy(table[index / 2].external, data, MMV_NAMEMAX);
    }
    return table;
}


MODULE = PCP::MMV		PACKAGE = PCP::MMV

mmv_indom_t *
mmv_indom(serial,shorttext,helptext,instlist)
	int			serial
	char *			shorttext
	char *			helptext
	AV *			instlist
    PREINIT:
	int			count;
	mmv_indom_t *		indom;
	mmv_instances_t *	instances;
    CODE:
	count = av_len(instlist);
	instances = new_instance_list(instlist, count);
	if (!instances)
	    XSRETURN_UNDEF;
	if ((indom = calloc(1, sizeof(mmv_indom_t))) == NULL) {
	    free(instances);
	    XSRETURN_UNDEF;
	}
	indom->serial = serial;
	indom->count = count;
	indom->instances = instances;
	if (shorttext)
	    indom->shorttext = strdup(shorttext);
	if (helptext)
	    indom->helptext = strdup(helptext);
	RETVAL = indom;
    OUTPUT:
	RETVAL

mmv_metric_t *
mmv_metric(name,item,type,indom,units,semantics,shorttext,helptext)
	char *			name
	unsigned int		item
	unsigned int		type
	unsigned int		indom
	unsigned int		units
	unsigned int		semantics
	char *			shorttext
	char *			helptext
    PREINIT:
	mmv_metric_t *		metric;
    CODE:
	if ((metric = calloc(1, sizeof(mmv_metric_t))) == NULL)
	    XSRETURN_UNDEF;
	strncpy(metric->name, name, MMV_NAMEMAX);
	memcpy(&metric->dimension, &units, sizeof(pmUnits));
	metric->semantics = semantics;
	metric->indom = indom;
	metric->type = type;
	metric->item = item;
	if (shorttext)
	    metric->shorttext = strdup(shorttext);
	if (helptext)
	    metric->helptext = strdup(helptext);
	RETVAL = metric;
    OUTPUT:
	RETVAL

void *
mmv_stats_init(name,cl,fl,metrics,indoms)
	char *			name
	int 			cl
	int 			fl
	AV *			metrics
	AV *			indoms
    PREINIT:
	int			i;
	int			mcount;
	int			icount;
	mmv_metric_t *		mlist;
	mmv_indom_t *		ilist;
    CODE:
	mcount = av_len(metrics);
	mlist = new_metric_list(metrics, mcount);
	icount = av_len(indoms);
	ilist = new_indom_list(indoms, icount);

	RETVAL = mmv_stats_init(name, cl, fl, mlist, mcount, ilist, icount);

	for (i = 0; i < icount; i++) {
	    if (ilist[i].shorttext) free(ilist[i].shorttext);
	    if (ilist[i].helptext) free(ilist[i].helptext);
	    free(ilist[i].instances);
	}
	free(ilist);
	for (i = 0; i < mcount; i++) {
	    if (mlist[i].shorttext) free(mlist[i].shorttext);
	    if (mlist[i].helptext) free(mlist[i].helptext);
	}
	free(mlist);

	if (!RETVAL)
	    XSRETURN_UNDEF;
    OUTPUT:
	RETVAL

int
mmv_units(dim_space,dim_time,dim_count,scale_space,scale_time,scale_count)
	unsigned int		dim_space
	unsigned int		dim_time
	unsigned int		dim_count
	unsigned int		scale_space
	unsigned int		scale_time
	unsigned int		scale_count
    PREINIT:
	pmUnits			units;
    CODE:
	units.pad = 0;
	units.dimSpace = dim_space;	units.scaleSpace = scale_space;
	units.dimTime = dim_time;	units.scaleTime = scale_time;
	units.dimCount = dim_count;	units.scaleCount = scale_count;
	RETVAL = *(int *)(&units);
    OUTPUT:
	RETVAL

pmAtomValue *
mmv_lookup_value_desc(handle,metric,instance)
	void *			handle
	char *			metric
	char *			instance
    CODE:
	RETVAL = mmv_lookup_value_desc(handle, metric, instance);
    OUTPUT:
	RETVAL

void
mmv_inc_value(handle,atom,value)
	void *			handle
	pmAtomValue *		atom
	double			value
    CODE:
	mmv_inc_value(handle, atom, value);

void
mmv_set_value(handle,atom,value)
	void *			handle
	pmAtomValue *		atom
	double			value
    CODE:
	mmv_set_value(handle, atom, value);

void
mmv_set_string(handle,atom,string)
	void *			handle
	pmAtomValue *		atom
	SV *			string
    PREINIT:
	int			length;
	char *			data;
    CODE:
	data = SvPV_nolen(string);
	length = strlen(data);
	mmv_set_string(handle, atom, data, length);

void
mmv_stats_add(handle,metric,instance,count)
	void *			handle
	char *			metric
	char *			instance
	double			count
    CODE:
	mmv_stats_add(handle, metric, instance, count);

void
mmv_stats_inc(handle,metric,instance)
	void *			handle
	char *			metric
	char *			instance
    CODE:
	mmv_stats_inc(handle, metric, instance);

void
mmv_stats_add_fallback(handle,metric,instance,instance2,count)
	void *			handle
	char *			metric
	char *			instance
	char *			instance2
	double			count
    CODE:
	mmv_stats_add_fallback(handle, metric, instance, instance2, count);

void
mmv_stats_inc_fallback(handle,metric,instance,instance2)
	void *			handle
	char *			metric
	char *			instance
	char *			instance2
    CODE:
	mmv_stats_inc_fallback(handle, metric, instance, instance2);

void
mmv_stats_interval_start(handle,value,metric,instance)
	void *			handle
	pmAtomValue *		value
	char *			metric
	char *			instance
    CODE:
	mmv_stats_interval_start(handle, value, metric, instance);

void
mmv_stats_interval_end(handle, value)
	void *			handle
	pmAtomValue *		value
    CODE:
	mmv_stats_interval_end(handle, value);

void
mmv_stats_set_string(handle,metric,instance,string)
	void *			handle
	char *			metric
	char *			instance
	SV *			string
    PREINIT:
	int			length;
	char *			data;
    CODE:
	data = SvPV_nolen(string);
	length = strlen(data);
	mmv_stats_set_strlen(handle, metric, instance, data, length);

