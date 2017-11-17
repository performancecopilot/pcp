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
#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "util.h"

/* time manipulation */
int
tsub(struct timeval *a, struct timeval *b)
{
    if ((a == NULL) || (b == NULL))
	return -1;
    pmtimevalDec(a, b);
    if (a->tv_sec < 0) {
	/* clip negative values at zero */
	a->tv_sec = 0;
	a->tv_usec = 0;
    }
    return 0;
}

int
tadd(struct timeval *a, struct timeval *b)
{
    if ((a == NULL) || (b == NULL))
	return -1;
    pmtimevalInc(a, b);
    return 0;
}

void
fputstamp(struct timeval *stamp, int delimiter, FILE *out)
{
    static char	timebuf[32];
    char	*ddmm;
    char	*yr;

    ddmm = pmCtime((const time_t *)&stamp->tv_sec, timebuf);
    ddmm[10] = ' ';
    ddmm[11] = '\0';
    yr = &ddmm[20];
    fprintf(out, "%c'%s", delimiter, ddmm);
    pmPrintStamp(out, stamp);
    fprintf(out, " %4.4s\'", yr);
}

const char *
value_instname(value_t *value)
{
    static char	namebuf[2048];
    const char	*n;

    if ((n = value->name) != NULL) {
	pmsprintf(namebuf, sizeof(namebuf), "\"%s\"", n);
	namebuf[sizeof(namebuf)-1] = '\0';
    } else {
	strncpy(namebuf, "null", sizeof(namebuf));
    }
    return namebuf;
}

unsigned int
value_instid(value_t *value)
{
    if (value->inst == PM_IN_NULL)
	return 0;
    return value->inst;
}

/* drop any trailing zeros after a decimal point */
static void
value_precision(char *buf, int maxlen, int usedlen)
{
    char	*p, *mantissa = NULL;

    for (p = buf; *p; p++) {
	if (*p != '.')
	    continue;
	mantissa = p;
	break;
    }
    if (!mantissa)
	return;
    for (p = buf + usedlen; p > mantissa; p--) {
	if (*p && *p != '0')
	    return;
	*p = '\0';
    }
}

const char *
value_atomstr(metric_t *metric, value_t *value)
{
    static char	valuebuf[512];
    int		len;

    switch (metric->desc.type) {
    case PM_TYPE_32:
	pmsprintf(valuebuf, sizeof(valuebuf), "%ld",
		(long)value->lastval.l);
	break;
    case PM_TYPE_U32:
	pmsprintf(valuebuf, sizeof(valuebuf), "%lu",
		(unsigned long)value->lastval.ul);
	break;
    case PM_TYPE_64:
	pmsprintf(valuebuf, sizeof(valuebuf), "%lld",
		(long long)value->lastval.ll);
	break;
    case PM_TYPE_U64:
	pmsprintf(valuebuf, sizeof(valuebuf), "%llu",
		(unsigned long long)value->lastval.ull);
	break;
    case PM_TYPE_DOUBLE:
	if ((long long)value->lastval.d == value->lastval.d)
	    pmsprintf(valuebuf, sizeof(valuebuf), "%lld",
			(long long)value->lastval.d);
	else {
	    len = pmsprintf(valuebuf, sizeof(valuebuf), "%f", value->lastval.d);
	    value_precision(valuebuf, sizeof(valuebuf), len);
	}
	break;
    case PM_TYPE_FLOAT:
	if ((long long)value->lastval.f == value->lastval.f)
	    pmsprintf(valuebuf, sizeof(valuebuf), "%lld",
			(long long)value->lastval.f);
	else {
	    len = pmsprintf(valuebuf, sizeof(valuebuf), "%f", value->lastval.f);
	    value_precision(valuebuf, sizeof(valuebuf), len);
	}
	break;
    default:
	/* TODO: support remaining data types - indirect maps */
	pmsprintf(valuebuf, sizeof(valuebuf), "%lu", 0UL);
	break;
    }
    return valuebuf;
}

int
merge_labelsets(metric_t *metric, value_t *value, char *buffer, int length,
	int (*filter)(const pmLabel *, const char *, void *), void *type)
{
    pmLabelSet	*sets[6];
    cluster_t	*cluster = metric->cluster;
    domain_t	*domain = cluster->domain;
    context_t	*context = domain->context;
    indom_t	*indom = metric->indom;
    int		nsets = 0;

    if (context->labels)
	sets[nsets++] = context->labels;
    if (domain->labels)
	sets[nsets++] = domain->labels;
    if (indom && indom->labels)
	sets[nsets++] = indom->labels;
    if (cluster->labels)
	sets[nsets++] = cluster->labels;
    if (metric->labels)
	sets[nsets++] = metric->labels;
    if (value->labels)
	sets[nsets++] = value->labels;

    return pmMergeLabelSets(sets, nsets, buffer, length, filter, type);
}

/* extract only identifying labels (not optional "notes") */
static int
labels(const pmLabel *label, const char *json, void *arg)
{
    if ((label->flags & PM_LABEL_OPTIONAL) != 0)
	return 0;
    return 1;
}

char *
value_labels(metric_t *metric, value_t *value)
{
    static char	lbuf[PM_MAXLABELJSONLEN];
    int		sts;

    sts = merge_labelsets(metric, value, lbuf, sizeof(lbuf), labels, NULL);
    if (sts < 0)
	return NULL;
    return lbuf;
}
