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

static mmv_stats_inst_t *
new_instance(int id, char *name)
{
    mmv_stats_inst_t *instance;

    if ((instance = malloc(sizeof(mmv_stats_inst_t))) == NULL)
	return NULL;

    strncpy(instance->external, name, MMV_NAMEMAX);
    instance->internal = id;
    return instance;
}

static mmv_stats_t *
new_metric(char *name, unsigned int type, mmv_stats_inst_t *indom, unsigned int units)
{
    mmv_stats_t *metric;

    if ((metric = malloc(sizeof(mmv_stats_t))) == NULL)
	return NULL;

    strncpy(metric->name, name, MMV_NAMEMAX);
    memcpy(&metric->dimension, &units, sizeof(pmUnits));
    metric->indom = indom;
    metric->type = type;
    return metric;
}

static mmv_stats_t *
new_stats(AV *array, int count)
{
    SV		**entry;
    int		index;
    mmv_stats_t	*table, *tmp;

    if ((table = malloc(count * sizeof(mmv_stats_t))) == NULL)
	return NULL;

    for (index = 0; index < count; index++) {
	entry = av_fetch(array, index, 0);
	tmp = (mmv_stats_t *) (*entry);
	table[index] = *tmp;
    }
    return table;
}


MODULE = PCP::MMV		PACKAGE = PCP::MMV

mmv_stats_inst_t *
mmv_instance(id,name)
	int			id
	char *			name
    CODE:
	RETVAL = new_instance(id, name);
	if (!RETVAL)
	    XSRETURN_UNDEF;
    OUTPUT:
	RETVAL

mmv_stats_t *
mmv_metric(name,type,insts,units)
	char *			name
	unsigned int		type
	mmv_stats_inst_t *	insts
	unsigned int		units
    CODE:
	RETVAL = new_metric(name, type, insts, units);
	if (!RETVAL)
	    XSRETURN_UNDEF;
    OUTPUT:
	RETVAL

void *
mmv_handle(name,stats)
	char *			name
	AV *			stats
    PREINIT:
	int			count;
	mmv_stats_t *		array;
    CODE:
	count = av_len(stats);
	array = new_stats(stats, count);
	RETVAL = mmv_stats_init(name, array, count);
	free(array);
	if (!RETVAL)
	    XSRETURN_UNDEF;
    OUTPUT:
	RETVAL

void
mmv_stats_static_add(hndl,metric,instance,count)
	void *			hndl
	char *			metric
	char *			instance
	double			count
    CODE:
	MMV_STATS_STATIC_ADD(hndl, metric, instance, count);

void
mmv_stats_static_inc(hndl,metric,instance)
	void *			hndl
	char *			metric
	char *			instance
    CODE:
	MMV_STATS_STATIC_INC(hndl, metric, instance);

void
mmv_stats_add(hndl,metric,instance,count)
	void *			hndl
	char *			metric
	char *			instance
	double			count
    CODE:
	MMV_STATS_ADD(hndl, metric, instance, count);

void
mmv_stats_inc(hndl,metric,instance)
	void *			hndl
	char *			metric
	char *			instance
    CODE:
	MMV_STATS_INC(hndl, metric, instance);

void
mmv_stats_add_fallback(hndl,metric,instance,instance2,count)
	void *			hndl
	char *			metric
	char *			instance
	char *			instance2
	double			count
    CODE:
	MMV_STATS_ADD_FALLBACK(hndl, metric, instance, instance2, count);

void
mmv_stats_inc_fallback(hndl,metric,instance,instance2)
	void *			hndl
	char *			metric
	char *			instance
	char *			instance2
    CODE:
	MMV_STATS_INC_FALLBACK(hndl, metric, instance, instance2);

void
mmv_stats_interval_start(hndl,vptr,metric,instance)
	void *			hndl
	mmv_stats_value_t *	vptr
	char *			metric
	char *			instance
    CODE:
	MMV_STATS_INTERVAL_START(hndl, vptr, metric, instance);

void
mmv_stats_interval_end(hndl, vptr)
	void *			hndl
	mmv_stats_value_t *	vptr
    CODE:
	MMV_STATS_INTERVAL_END(hndl, vptr);

