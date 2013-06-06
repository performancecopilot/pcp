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

metric_t *
find_metric(char *name)
{
    metric_t *m;

    for (m = metrics; m->name; m++) {
    	if (strcmp(name, m->name) == 0)
	    break;
    }

    return m->name ? m : NULL;
}

handler_t *
find_handler(char *tag)
{
    handler_t *h;

    for (h = handlers; h->pattern; h++) {
	if (strchr(h->pattern, '*') != NULL) {
	    /* match tag as a prefix, e.g. cpu* matches cpu123 */
	    if (strncmp(h->pattern, tag, strlen(h->pattern)-1) == 0)
		break;
	}
	else {
	    /* exact match */
	    if (strcmp(h->pattern, tag) == 0)
		break;
	}
    }

    return h->pattern ? h : NULL;
}

int
put_str_instance(pmInDom indom, char *instance)
{
    int sts;
    __pmInDom_int *idp = __pmindom_int(&indom);
    int id = indom_cnt[idp->serial]++;

    sts = pmiAddInstance(indom, instance, id);
    return sts ? sts : id;
}

int
put_str_value(char *name, pmInDom indom, char *instance, char *val)
{
    int sts = 0;

    if (!val)
	fprintf(stderr, "Warning: put_str_value: ignored NULL value for \"%s\"\n", name);
    else if (indom != PM_INDOM_NULL && !instance)
	fprintf(stderr, "Warning: put_str_value: ignored NULL instance for non-singular indom for \"%s\"\n", name);
    else {
	sts = pmiPutValue(name, instance, val);
	if (sts == PM_ERR_NAME) {
#if 0
	    if (vflag)
		fprintf(stderr, "Warning: unknown metric name \"%s\". Check metrics.c\n", name);
#endif
	    return sts;
	}
	if (indom != PM_INDOM_NULL && instance && (sts == PM_ERR_INST || sts == PM_ERR_INDOM)) {
	    /* New instance has appeared */
	    sts = put_str_instance(indom, instance);
	    if (sts < 0)
		fprintf(stderr, "Warning: put_str_value failed to add new instance \"%s\" for indom:%d err:%d : %s\n",
		    instance, indom, sts, pmiErrStr(sts));
	    else if (vflag)
		printf("New instance %s[%d] \"%s\"\n", name, sts, instance);
	    sts = pmiPutValue(name, instance, val);
	}
	if (sts < 0)
	    fprintf(stderr, "Warning: put_str_value \"%s\" inst:\"%s\" value:\"%s\" failed: err=%d %s\n",
		name, instance ? instance : "NULL", val ? val : "NULL", sts, pmiErrStr(sts));
    }

    return sts;
}

int
put_int_value(char *name, pmInDom indom, char *instance, int val)
{
    char valbuf[64];

    sprintf(valbuf, "%d", val);
    return put_str_value(name, indom, instance, valbuf);
}

int
put_ull_value(char *name, pmInDom indom, char *instance, unsigned long long val)
{
    char valbuf[64];

    sprintf(valbuf, "%llu", val);
    return put_str_value(name, indom, instance, valbuf);
}

/* split a string into fields and their lengths. Free fields[0] when done. */
int
strfields(const char *s, int len, char **fields, int *fieldlen, int maxfields)
{
    int i;
    char *p;
    char *p_end;

    if (!s || *s == '\0')
    	return 0;
    
    if ((p = strdup(s)) == NULL)
    	return 0;

    for (i=0, p_end = p+len; i < maxfields;) {
        fields[i] = p;
	fieldlen[i] = 0;
        while(*p && !isspace(*p) && p < p_end) {
            p++;
	    fieldlen[i]++;
	}
	i++;
	if (!*p)
	    break;
        *p++ ='\0';
	while (isspace(*p))
	    p++;
    }

    return i;
}

fields_t *
fields_new(const char *s, int len)
{
    int n = 1;
    const char *p;
    fields_t *f = (fields_t *)malloc(sizeof(fields_t));

    memset(f, 0, sizeof(fields_t));
    f->len = len;
    for (p=s; *p && p < s+len; p++) {
	if (isspace(*p))
	    n++;
    }
    /*
     * n is an upper bound, at least 1 (separator may repeat).
     * fields[0] is the actual buffer, allocated by strfields
     */
    f->fields = (char **)malloc(n * sizeof(char *));
    f->fieldlen = (int *)malloc(n * sizeof(int));
    f->nfields = strfields(s, len, f->fields, f->fieldlen, n);
    f->buf = f->fields[0];

    return f;
}

fields_t *
fields_dup(fields_t *f)
{
    int i;
    fields_t *copy;

    copy = malloc(sizeof(fields_t));
    memset(copy, 0, sizeof(fields_t));
    copy->len = f->len;
    copy->buf = (char *)malloc(f->len + 1);
    memcpy(copy->buf, f->buf, f->len);

    copy->nfields = f->nfields;
    copy->fields = (char **)malloc(f->nfields * sizeof(char *));
    copy->fieldlen = (int *)malloc(f->nfields * sizeof(int));

    copy->fields[0] = copy->buf;
    for (i=1; i < f->nfields; i++) {
    	copy->fieldlen[i] = f->fieldlen[i];
    	copy->fields[i] = copy->fields[0] + (f->fields[i] - f->fields[0]);
    }

    return copy;
}

void
fields_free(fields_t *f)
{
    free(f->buf);
    free(f->fields);
    free(f->fieldlen);
    free(f);
}

