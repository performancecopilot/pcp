/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

/*
 * parse uniform metric spec syntax
 */

#ident "$Id: mspec.c,v 1.4 2004/08/02 07:11:43 kenmcd Exp $"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include "pmapi.h"
#include "impl.h"

/****************************************************************************
 * local functions
 ****************************************************************************/

/* memory allocation */
static void *
parseAlloc(size_t need)
{
    void    *tmp;

    if ((tmp = malloc(need)) == NULL)
	__pmNoMem("pmParseMetricSpec", need, PM_FATAL_ERR);
    return tmp;
}

/* Report syntax error */
static void
parseError(const char *spec, const char *point, char *msg, char **rslt)
{
    int		need = 2 * (int)strlen(spec) + 1 + 6 + (int)strlen(msg) + 2;
    const char	*p;
    char	*q;

    if ((q = (char *) malloc(need)) == NULL)
	__pmNoMem("pmParseMetricSpec", need, PM_FATAL_ERR);
    *rslt = q;
    for (p = spec; *p != '\0'; p++)
	*q++ = *p;
    *q++ = '\n';
    for (p = spec; p != point; p++)
	*q++ = isgraph((int)*p) ? ' ' : *p;
    sprintf(q, "^ -- %s\n", msg);	/* safe */
}


/****************************************************************************
 * exported functions
 ****************************************************************************/

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
    const char	    *a_end;
    const char	    *m_start = NULL;	/* metric name */
    const char	    *m_end;
    const char	    *i_start = NULL;	/* instance names */
    const char	    *i_end;
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
    mark = scan;

    /* delimit host name */
    while (*scan != ':' && ! isspace((int)*scan) && *scan != '\0' && *scan != '[') {
	if (*scan == '\\' && *(scan+1) != '\0')
	    scan++;
	scan++;
    }
    h_end = scan;
    while (isspace((int)*scan))
	scan++;
    if (*scan == ':') {
	h_start = mark;
	if (h_start == h_end) {
	    parseError(spec, scan, "host name expected", errmsg);
	    return PM_ERR_GENERIC;
	}
	scan++;
    }

    /* delimit archive name */
    else {
	scan = mark;
	while (*scan != '\0' && *scan != '[') {
	    if (*scan == '\\' && *(scan+1) != '\0')
		scan++;
	    else if  (*scan == '/') {
		a_start = mark;
		a_end = scan;
	    }
	    scan++;
	}
	if (a_start) {
	    if (a_start == a_end) {
		parseError(spec, a_start, "archive name expected", errmsg);
		return PM_ERR_GENERIC;
	    }
	    scan = a_end + 1;
	}
	else
	    scan = mark;
    }

    while (isspace((int)*scan))
	scan++;
    mark = scan;

    /* delimit metric name */
    m_start = scan;
    while (! isspace((int)*scan) && *scan != '\0' && *scan != '[') {
	if (*scan == ':' || *scan == '/' || *scan == ']' || *scan == ',') {
	    parseError(spec, scan, "unexpected character in metric name", errmsg);
	    return PM_ERR_GENERIC;
	}
	if (*scan == '\\' && *(scan+1) != '\0')
	    scan++;
	scan++;
    }
    m_end = scan;
    if (m_start == m_end) {
	parseError(spec, m_start, "performance metric name expected", errmsg);
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
		    parseError(spec, scan, "closing \" and ] expected", errmsg);
		else
		    parseError(spec, scan, "closing ] expected", errmsg);
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
	parseError(spec, scan, "unexpected extra characters", errmsg);
	return PM_ERR_GENERIC;
    }

    /* count instance names and make temporary copy */
    ninst = 0;
    if (i_start != NULL) {
	i_str = (char *) parseAlloc(i_end - i_start + 1);

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
			parseError(spec, scan, "closing \" expected (pmParseMetricSpec botch?)", errmsg);
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
    msp = (pmMetricSpec *)parseAlloc(length);

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
	msp->isarch = 0;
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
