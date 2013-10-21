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

#include "pmapi.h"
#include "mmv_stats.h"
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

static int
list_to_metric(SV *list, mmv_metric_t *metric)
{
    int		i, len;
    SV		**entry[8];
    AV		*mlist = (AV *) SvRV(list);

    if (SvTYPE((SV *)mlist) != SVt_PVAV) {
	warn("metric declaration is not an array reference");
	return -1;
    }
    len = av_len(mlist) + 1;
    if (len < 6) {
	warn("too few entries in metric array reference");
	return -1;
    }
    if (len > 8) {
	warn("too many entries in metric array reference");
	return -1;
    }
    for (i = 0; i < len; i++)
	entry[i] = av_fetch(mlist, i, 0);

    strncpy(metric->name, SvPV_nolen(*entry[0]), MMV_NAMEMAX);
    metric->name[MMV_NAMEMAX-1] = '\0';
    metric->item = SvIV(*entry[1]);
    metric->type = SvIV(*entry[2]);
    metric->indom = SvIV(*entry[3]);
    i = SvIV(*entry[4]);
    memcpy(&metric->dimension, &i, sizeof(pmUnits));
    metric->semantics = SvIV(*entry[5]);
    if (len > 6)
	metric->shorttext = strdup(SvPV_nolen(*entry[6]));
    else
	metric->shorttext = NULL;
    if (len > 7)
	metric->helptext = strdup(SvPV_nolen(*entry[7]));
    else
	metric->helptext = NULL;
    return 0;
}

static int
list_to_instances(SV *list, mmv_instances_t **insts)
{
    mmv_instances_t	*instances;
    int			i, len;
    AV			*inlist = (AV *) SvRV(list);

    if (SvTYPE((SV *)inlist) != SVt_PVAV) {
	warn("instances declaration is not an array reference");
	return -1;
    }
    len = av_len(inlist) + 1;
    if (len++ % 2) {
	warn("odd number of entries in instance array reference");
	return -1;
    }

    len /= 2;
    instances = (mmv_instances_t *)calloc(len, sizeof(mmv_instances_t));
    if (instances == NULL) {
	warn("insufficient memory for instance array");
	return -1;
    }
    for (i = 0; i < len; i++) {
	SV **id = av_fetch(inlist, i*2, 0);
	SV **name = av_fetch(inlist, i*2+1, 0);
	instances[i].internal = SvIV(*id);
	strncpy(instances[i].external, SvPV_nolen(*name), MMV_NAMEMAX);
	instances[i].external[MMV_NAMEMAX-1] = '\0';
    }
    *insts = instances;
    return len;
}

static int
list_to_indom(SV *list, mmv_indom_t *indom)
{
    int		i, len;
    SV		**entry[4];
    AV		*ilist = (AV *) SvRV(list);

    if (SvTYPE((SV *)ilist) != SVt_PVAV) {
	warn("indom declaration is not an array reference");
	return -1;
    }
    len = av_len(ilist) + 1;
    if (len < 2) {
	warn("too few entries in indom array reference");
	return -1;
    }
    if (len > 4) {
	warn("too many entries in indom array reference");
	return -1;
    }
    for (i = 0; i < len; i++)
	entry[i] = av_fetch(ilist, i, 0);

    indom->serial = SvIV(*entry[0]);
    if ((i = list_to_instances(*entry[1], &indom->instances)) < 0)
	return -1;
    indom->count = i;
    if (len > 2)
	indom->shorttext = strdup(SvPV_nolen(*entry[2]));
    else
	indom->shorttext = NULL;
    if (len > 3)
	indom->helptext = strdup(SvPV_nolen(*entry[3]));
    else
	indom->helptext = NULL;
    return 0;
}

static int
list_to_metrics(SV *list, mmv_metric_t **metriclist, int *mcount)
{
    mmv_metric_t	*metrics;
    int			i, len;
    AV			*mlist = (AV *) SvRV(list);

    if (SvTYPE((SV *)mlist) != SVt_PVAV) {
	warn("metrics list is not an array reference");
	return -1;
    }
    len = av_len(mlist) + 1;
    metrics = (mmv_metric_t *)calloc(len, sizeof(mmv_metric_t));
    if (metrics == NULL) {
	warn("insufficient memory for metrics array");
	return -1;
    }
    for (i = 0; i < len; i++) {
	SV **entry = av_fetch(mlist, i, 0);
	if (list_to_metric(*entry, &metrics[i]) < 0)
	    break;
    }
    *metriclist = metrics;
    *mcount = len;
    return (i == len);
}

static int
list_to_indoms(SV *list, mmv_indom_t **indomlist, int *icount)
{
    mmv_indom_t		*indoms;
    int			i, len;
    AV			*ilist = (AV *) SvRV(list);

    if (SvTYPE((SV *)ilist) != SVt_PVAV) {
	warn("indoms list is not an array reference");
	return -1;
    }
    len = av_len(ilist) + 1;
    indoms = (mmv_indom_t *)calloc(len, sizeof(mmv_indom_t));
    if (indoms == NULL) {
	warn("insufficient memory for indoms array");
	return -1;
    }
    for (i = 0; i < len; i++) {
	SV **entry = av_fetch(ilist, i, 0);
	if (list_to_indom(*entry, &indoms[i]) < 0)
	    break;
    }
    *indomlist = indoms;
    *icount = len;
    return (i == len);
}


MODULE = PCP::MMV		PACKAGE = PCP::MMV

void *
mmv_stats_init(name,cl,fl,metrics,indoms)
	char *			name
	int 			cl
	int 			fl
	SV *			metrics
	SV *			indoms
    PREINIT:
	int			i, j;
	int			mcount;
	int			icount;
	mmv_metric_t *		mlist;
	mmv_indom_t *		ilist;
    CODE:
	i = list_to_metrics(metrics, &mlist, &mcount);
	j = list_to_indoms(indoms, &ilist, &icount);

	if (i <= 0 || j <= 0) {
	    warn("mmv_stats_init: bad list conversion: metrics=%d indoms=%d\n", i, j);
	    RETVAL = NULL;
	}
	else {
	    RETVAL = mmv_stats_init(name, cl, fl, mlist, mcount, ilist, icount);
	    if (RETVAL == NULL)
		warn("mmv_stats_init failed: %s\n", osstrerror());
	}

	for (i = 0; i < icount; i++) {
	    if (ilist[i].shorttext)
		free(ilist[i].shorttext);
	    if (ilist[i].helptext)
		free(ilist[i].helptext);
	    free(ilist[i].instances);
	}
	if (ilist)
	    free(ilist);
	for (i = 0; i < mcount; i++) {
	    if (mlist[i].shorttext)
		free(mlist[i].shorttext);
	    if (mlist[i].helptext)
		free(mlist[i].helptext);
	}
	if (mlist)
	    free(mlist);

	if (!RETVAL)
	    XSRETURN_UNDEF;
    OUTPUT:
	RETVAL

void
mmv_stats_stop(handle,name)
	void *			handle
	char *			name
    CODE:
	mmv_stats_stop(handle, name);

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
mmv_stats_set(handle,metric,instance, value)
	void *			handle
	char *			metric
	char *			instance
	double			value
    CODE:
	mmv_stats_set(handle, metric, instance, value);

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

