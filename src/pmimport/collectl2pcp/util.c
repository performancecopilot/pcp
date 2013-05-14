/*
 * Copyright (c) 2013 Red Hat Inc.
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
 *
 */

#include "metrics.h"

char *
strfield_r(char *p, int f, char *r)
{
    char *s = NULL;

    strcpy(r, p);
    if (f > 0) {
	s = strtok(r, " \t");
	for (f--; f; f--)
	    s = strtok(NULL, " \t");
    }

    return s;
}

metric_t *
find_metric(char *name)
{
    metric_t *m;

    /* TODO: use an index */
    for (m = metrics; m->name; m++) {
    	if (strcmp(name, m->name) == 0)
	    break;
    }

    return m->name ? m : NULL;
}

handler_t *
find_handler(char *buf)
{
    handler_t *h;

    /* TODO: use an index and maybe regex */
    for (h = handlers; h->pattern; h++) {
    	if (strncmp(h->pattern, buf, strlen(h->pattern)) == 0)
	    break;
    }

    return h->pattern ? h : NULL;
}

int
put_int_value(char *name, int indom, char *instance, int val)
{
    int sts = 0;
    metric_t *m;
    char valbuf[64];

    if ((m = find_metric(name)) == NULL)
	fprintf(stderr, "Warning: put_int_value \"%s\" metric not defined\n", name);
    else {
    	sprintf(valbuf, "%d", val);
	sts = pmiPutValue(m->name, instance, valbuf);
	if (indom != PM_INDOM_NULL && sts == PM_ERR_INST) {
	    /* handle a new instance appearing */
	    indom_cnt[indom]++;
	    sts = pmiAddInstance(indom, instance, indom_cnt[indom]);
	    if (sts < 0)
		fprintf(stderr, "Warning: put_int_value failed to add new instance \"%s\" for indom:%d err:%d : %s\n",
		    instance, indom, sts, pmiErrStr(sts));
	    sts = pmiPutValue(m->name, instance, valbuf);
	}
	if (sts < 0)
	    fprintf(stderr, "Warning: put_int_value \"%s\" instance:\"%s\" value:%d failed: %s\n",
		m->name, instance ? instance : "NULL", val, pmiErrStr(sts));
    }

    return sts;
}

int
put_str_value(char *name, int indom, char *instance, char *val)
{
    int sts = 0;
    metric_t *m;

    if ((m = find_metric(name)) == NULL)
	fprintf(stderr, "Warning: put_str_value \"%s\" metric not defined\n", name);
    else
    if (val) {
	sts = pmiPutValue(m->name, instance, val);
	if (indom != PM_INDOM_NULL && sts == PM_ERR_INST) {
	    /* handle a new instance appearing */
	    indom_cnt[indom]++;
	    sts = pmiAddInstance(indom, instance, indom_cnt[indom]);
	    if (sts < 0)
		fprintf(stderr, "Warning: put_str_value failed to add new instance \"%s\" for indom:%d err:%d : %s\n",
		    instance, indom, sts, pmiErrStr(sts));
	    sts = pmiPutValue(m->name, instance, val);
	}
	if (sts < 0)
	    fprintf(stderr, "Warning: put_str_value \"%s\" inst:\"%s\" value:\"%s\" failed: %s\n",
		m->name, instance ? instance : "NULL", val ? val : "NULL", pmiErrStr(sts));
    }

    return sts;
}
