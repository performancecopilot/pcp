/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2007 Aconex.  All Rights Reserved.
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

/*
 * Parse uniform metric and host specification syntax
 */

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

static void *
parseAlloc(const char *func, size_t need)
{
    void    *tmp;

    if ((tmp = malloc(need)) == NULL)
	__pmNoMem(func, need, PM_FATAL_ERR);
    return tmp;
}

static void
parseError(const char *func, const char *spec, const char *point, char *msg, char **rslt)
{
    int		need;
    const char	*p;
    char	*q;

    if (rslt == NULL)
	return;

    need = 2 * (int)strlen(spec) + 1 + 6 + (int)strlen(msg) + 2;
    *rslt = q = (char *)parseAlloc(func, need);
    for (p = spec; *p != '\0'; p++)
	*q++ = *p;
    *q++ = '\n';
    for (p = spec; p != point; p++)
	*q++ = isgraph((int)*p) ? ' ' : *p;
    sprintf(q, "^ -- %s\n", msg);	/* safe */
}

static void *
metricAlloc(size_t need)
{
    return parseAlloc("pmParseMetricSpec", need);
}

static void
metricError(const char *spec, const char *point, char *msg, char **rslt)
{
    parseError("pmParseMetricSpec", spec, point, msg, rslt);
}

int		/* 0 -> ok, PM_ERR_GENERIC -> error */
pmParseMetricSpec(
    const char *spec,		/* parse this string */
    int isarch,			/* default source: 0 -> host, 1 -> archive */
    char *source,		/* name of default host or archive */
    pmMetricSpec **rslt,	/* result allocated and returned here */
    char **errmsg)		/* error message */
{
    pmMetricSpec   *msp = NULL;
    const char	    *scan;
    const char	    *mark;
    const char	    *h_start = NULL;	/* host name */
    const char	    *h_end = NULL;
    const char	    *a_start = NULL;	/* archive name */
    const char	    *a_end = NULL;
    const char	    *m_start = NULL;	/* metric name */
    const char	    *m_end = NULL;
    const char	    *i_start = NULL;	/* instance names */
    const char	    *i_end = NULL;
    char	    *i_str = NULL;	/* temporary instance names */
    char	    *i_scan;
    int		    ninst;		/* number of instance names */
    char	    *push;
    const char	    *pull;
    int		    length;
    int		    i;
    int		    inquote = 0;	/* true if within quotes */

    scan = spec;
    while (isspace((int)*scan))
	scan++;

    /*
     * Options here are ...
     * [host:]metric[[instance list]]
     *     special case for PM_CONTEXT_LOCAL [@:]metric[[instance list]]
     * [archive/]metric[[instance list]]
     *
     * Find end of metric name first ([ or end of string) then scan
     * backwards for first ':' or '/'
     */
    mark = index(scan, (int)'[');
    if (mark == NULL) mark = &scan[strlen(scan)-1];
    while (mark >= scan) {
	if (*mark == ':') {
	    h_start = scan;
	    h_end = mark-1;
	    while (h_end >= scan && isspace((int)*h_end)) h_end--;
	    if (h_end < h_start) {
		metricError(spec, h_start, "host name expected", errmsg);
		return PM_ERR_GENERIC;
	    }
	    h_end++;
	    scan = mark+1;
	    break;
	}
	else if (*mark == '/') {
	    a_start = scan;
	    a_end = mark-1;
	    while (a_end >= scan && isspace((int)*a_end)) a_end--;
	    if (a_end < a_start) {
		metricError(spec, a_start, "archive name expected", errmsg);
		return PM_ERR_GENERIC;
	    }
	    a_end++;
	    scan = mark+1;
	    break;
	}
	mark--;
    }

    while (isspace((int)*scan))
	scan++;
    mark = scan;

    /* delimit metric name */
    m_start = scan;
    while (! isspace((int)*scan) && *scan != '\0' && *scan != '[') {
	if (*scan == ']' || *scan == ',') {
	    metricError(spec, scan, "unexpected character in metric name", errmsg);
	    return PM_ERR_GENERIC;
	}
	if (*scan == '\\' && *(scan+1) != '\0')
	    scan++;
	scan++;
    }
    m_end = scan;
    if (m_start == m_end) {
	metricError(spec, m_start, "performance metric name expected", errmsg);
	return PM_ERR_GENERIC;
    }

    while (isspace((int)*scan))
	scan++;

    /* delimit instance names */
    if (*scan == '[') {
	scan++;
	while (isspace((int)*scan))
	    scan++;
	i_start = scan;
	for ( ; ; ) {
	    if (*scan == '\0') {
		if (inquote)
		    metricError(spec, scan, "closing \" and ] expected", errmsg);
		else
		    metricError(spec, scan, "closing ] expected", errmsg);
		return PM_ERR_GENERIC;
	    }
	    if (*scan == '\\' && *(scan+1) != '\0')
		scan++;
	    else if (*scan == '"')
	         inquote = 1 - inquote;
	    else if (!inquote && *scan == ']')
		break;
	    scan++;
	}
	i_end = scan;
	scan++;
    }

    /* check for rubbish at end of string */
    while (isspace((int)*scan))
	scan++;
    if (*scan != '\0') {
	metricError(spec, scan, "unexpected extra characters", errmsg);
	return PM_ERR_GENERIC;
    }

    /* count instance names and make temporary copy */
    ninst = 0;
    if (i_start != NULL) {
	i_str = (char *) metricAlloc(i_end - i_start + 1);

	/* count and copy instance names */
	scan = i_start;
	i_scan = i_str;
	while (scan < i_end) {

	    /* copy single instance name */
	    ninst++;
	    if (*scan == '"') {
		scan++;
		for (;;) {
		    if (scan >= i_end) {
			metricError(spec, scan, "closing \" expected (pmParseMetricSpec botch?)", errmsg);
			if (msp)
			    pmFreeMetricSpec(msp);
			if (i_str)
			    free(i_str);
			return PM_ERR_GENERIC;
		    }
		    if (*scan == '\\')
			scan++;
		    else if (*scan == '"')
			break;
		    *i_scan++ = *scan++;
		}
		scan++;
	    }
	    else {
		while (! isspace((int)*scan) && *scan != ',' && scan < i_end) {
		    if (*scan == '\\')
			scan++;
		    *i_scan++ = *scan++;
		}
	    }
	    *i_scan++ = '\0';

	    /* skip delimiters */
	    while ((isspace((int)*scan) || *scan == ',') && scan < i_end)
		scan++;
	}
	i_start = i_str;
	i_end = i_scan;
    }

    /* single memory allocation for result structure */
    length = (int)(sizeof(pmMetricSpec) +
             ((ninst > 1) ? (ninst - 1) * sizeof(char *) : 0) +
	     ((h_start) ? h_end - h_start + 1 : 0) +
	     ((a_start) ? a_end - a_start + 1 : 0) +
	     ((m_start) ? m_end - m_start + 1 : 0) +
	     ((i_start) ? i_end - i_start + 1 : 0));
    msp = (pmMetricSpec *)metricAlloc(length);

    /* strings follow pmMetricSpec proper */
    push = ((char *) msp) +
	   sizeof(pmMetricSpec) + 
	   ((ninst > 1) ? (ninst - 1) * sizeof(char *) : 0);

    /* copy metric name */
    msp->metric = push;
    pull = m_start;
    while (pull < m_end) {
	if (*pull == '\\' && (pull+1) < m_end)
	    pull++;
	*push++ = *pull++;
    }
    *push++ = '\0';

    /* copy host name */
    if (h_start != NULL) {
	if (h_end - h_start == 1 && *h_start == '@') {
	    /* PM_CONTEXT_LOCAL special case */
	    msp->isarch = 2;
	}
	else {
	    /* PM_CONTEXT_HOST */
	    msp->isarch = 0;
	}
	msp->source = push;
	pull = h_start;
	while (pull < h_end) {
	    if (*pull == '\\' && (pull+1) < h_end)
		pull++;
	    *push++ = *pull++;
	}
	*push++ = '\0';
    }

    /* copy archive name */
    else if (a_start != NULL) {
	msp->isarch = 1;
	msp->source = push;
	pull = a_start;
	while (pull < a_end) {
	    if (*pull == '\\' && (pull+1) < a_end)
		pull++;
	    *push++ = *pull++;
	}
	*push++ = '\0';
    }

    /* take default host or archive */
    else {
	msp->isarch = isarch;
	msp->source = source;
    }

    /* instance names */
    msp->ninst = ninst;
    pull = i_start;
    for (i = 0; i < ninst; i++) {
	msp->inst[i] = push;
	do
	    *push++ = *pull;
	while (*pull++ != '\0');
    }

    if (i_str)
	free(i_str);
    *rslt = msp;
    return 0;
}

void
pmFreeMetricSpec(pmMetricSpec *spec)
{
    free(spec);
}


static void
hostError(const char *spec, const char *point, char *msg, char **rslt)
{
    parseError("pmParseHostSpec", spec, point, msg, rslt);
}

static char *
hostStrndup(const char *name, int namelen)
{
    char *s = malloc(namelen + 1);
    strncpy(s, name, namelen);
    s[namelen] = '\0';
    return s;
}

static pmHostSpec *
hostAdd(pmHostSpec *specp, int *count, const char *name, int namelen)
{
    int n = *count;
    char *host;

    host = hostStrndup(name, namelen);
    if (!host || (specp = realloc(specp, sizeof(pmHostSpec) * (n+1))) == NULL) {
	if (host != NULL)
	    free(host);
	*count = 0;
	return NULL;
    }
    specp[n].name = host;
    specp[n].ports = NULL;
    specp[n].nports = 0;

    *count = n + 1;
    return specp;
}

int
__pmAddHostPorts(pmHostSpec *specp, int *ports, int nports)
{
    int *portlist;

    if ((portlist = malloc(sizeof(int) * (specp->nports + nports))) == NULL)
	return -ENOMEM;
    if (specp->nports > 0) {
	memcpy(portlist, specp->ports, sizeof(int) * specp->nports);
	free(specp->ports);
    }
    memcpy(&portlist[specp->nports], ports, sizeof(int) * nports);
    specp->ports = portlist;
    specp->nports = specp->nports + nports;
    return 0;
}

void
__pmDropHostPort(pmHostSpec *specp)
{
    specp->nports--;
    memmove(&specp->ports[0], &specp->ports[1], specp->nports*sizeof(int));
}

/* 
 * Parse a host specification, with optional ports and proxy host(s).
 * Examples:
 *	pcp -h app1.aconex.com:44321,4321@firewall.aconex.com:44322
 *	pcp -h app1.aconex.com:44321@firewall.aconex.com:44322
 *	pcp -h app1.aconex.com:44321@firewall.aconex.com
 *	pcp -h app1.aconex.com@firewall.aconex.com
 *	pcp -h app1.aconex.com:44321
 *
 * Basic algorithm:
 *	look for first colon, @ or null; preceding text is hostname
 *	 if colon, look for comma, @ or null, preceding text is port
 *	  while comma, look for comma, @ or null, preceding text is next port
 *	if @, start following host specification at the following character,
 *	 by returning to the start and repeating the above for the next chunk.
 * Note:
 *	Currently only two hosts are useful, but ability to handle more than
 *	one optional proxy host is there (i.e. proxy ->proxy ->... ->pmcd),
 *	in case someone implements the pmproxy->pmproxy protocol extension.
 */

int             /* 0 -> ok, PM_ERR_GENERIC -> error */
__pmParseHostSpec(
    const char *spec,           /* parse this string */
    pmHostSpec **rslt,          /* result allocated and returned here */
    int *count,
    char **errmsg)              /* error message */
{
    pmHostSpec *hsp = NULL;
    const char *s, *start;
    int nhosts = 0, sts = 0;

    for (s = start = spec; s != NULL; s++) {
	if (*s == ':' || *s == '@' || *s == '\0') {
	    if (s == start)
		continue;
	    hsp = hostAdd(hsp, &nhosts, start, s - start);
	    if (hsp == NULL) {
		sts = -ENOMEM;
		goto fail;
	    }
	    if (*s == ':') {
		for (++s, start = s; s != NULL; s++) {
		    if (*s == ',' || *s == '@' || *s == '\0') {
			if (s - start < 1) {
			    hostError(spec, s, "missing port", errmsg);
			    sts = PM_ERR_GENERIC;
			    goto fail;
			}
			int port = atoi(start);
			sts = __pmAddHostPorts(&hsp[nhosts-1], &port, 1);
			if (sts < 0)
			    goto fail;
			start = s + 1;
			if (*s == '@' || *s == '\0')
			    break;
			continue;
		    }
		    if (isdigit((int)*s))
			continue;
		    hostError(spec, s, "non-numeric port", errmsg);
		    sts = PM_ERR_GENERIC;
		    goto fail;
		}
	    }
	    if (*s == '@') {
		start = s+1;
		continue;
	    }
	    break;
	}
    }
    *count = nhosts;
    *rslt = hsp;
    return 0;

fail:
    __pmFreeHostSpec(hsp, nhosts);
    *rslt = NULL;
    *count = 0;
    return sts;
}

void
__pmUnparseHostSpec(pmHostSpec *hostp, int count, char **specp, int specsz)
{
    int i, j, sz = 0;

    for (i = 0; i < count; i++) {
	if (i > 0)
	    sz += snprintf(*specp, specsz, "@");
	sz += snprintf(*specp, specsz, "%s", hostp[0].name);
	for (j = 0; j < hostp[i].nports; j++)
	    sz += snprintf((*specp) + sz, specsz - sz,
			    "%c%u", (j == 0) ? ':' : ',', hostp[i].ports[j]);
    }
}

void
__pmFreeHostSpec(pmHostSpec *specp, int count)
{
    int i;

    for (i = 0; i < count; i++) {
	free(specp[i].name);
	specp[i].name = NULL;
	if (specp[i].nports > 0)
	    free(specp[i].ports);
	specp[i].ports = NULL;
	specp[i].nports = 0;
    }
    if (specp && count)
	free(specp);
}
