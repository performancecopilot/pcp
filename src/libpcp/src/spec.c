/*
 * Copyright (c) 2013-2015 Red Hat.
 * Copyright (c) 2007 Aconex.  All Rights Reserved.
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
 */

/*
 * Parse uniform metric and host specification syntax
 */

#include <ctype.h>
#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

static void *
parseAlloc(const char *func, size_t need)
{
    void    *tmp;

    if ((tmp = malloc(need)) == NULL)
	pmNoMem(func, need, PM_FATAL_ERR);
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
    pmsprintf(q, need - (q - *rslt), "^ -- %s\n", msg);
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
 *      pcp -h 192.168.122.1:44321
 *      pcp -h [fe80::5eff:35ff:fe07:55ca]:44321,4321@[fe80::5eff:35ff:fe07:55cc]:44322
 *      pcp -h [fe80::5eff:35ff:fe07:55ca]:44321
 *
 * Basic algorithm:
 *	look for first colon, @ or null; preceding text is hostname
 *	 if colon, look for comma, @ or null, preceding text is port
 *	  while comma, look for comma, @ or null, preceding text is next port
 *	if @, start following host specification at the following character,
 *	 by returning to the start and repeating the above for the next chunk.
 * Note:
 *      IPv6 addresses contain colons and, so, must be separated from the
 *      rest of the spec somehow. A common notation among ipv6-enabled
 *      applications is to enclose the address within brackets, as in
 *      [fe80::5eff:35ff:fe07:55ca]:44321. We keep it simple, however,
 *      and allow any host spec to be enclosed in brackets.
 * Note:
 *	Currently only two hosts are useful, but ability to handle more than
 *	one optional proxy host is there (i.e. proxy ->proxy ->... ->pmcd),
 *	in case someone implements the pmproxy->pmproxy protocol extension.
 */
static int      /* 0 -> ok, PM_ERR_GENERIC -> error message is set */
parseHostSpec(
    const char *spec,
    char **position,            /* parse this string, return end char */
    pmHostSpec **rslt,          /* result allocated and returned here */
    int *count,
    char **errmsg)              /* error message */
{
    pmHostSpec *hsp = NULL;
    const char *s, *start, *next;
    int nhosts = 0, sts = 0;

    for (s = start = *position; s != NULL; s++) {
	/* Allow the host spec to be enclosed in brackets. */
	if (s == start && *s == '[') {
	    for (s++; *s != ']' && *s != '\0'; s++)
		;
	    if (*s != ']') {
		hostError(spec, s, "missing closing ']' for host spec", errmsg);
		sts = PM_ERR_GENERIC;
		goto fail;
	    }
	    next = s + 1; /* past the trailing ']' */
	    if (*next != ':' && *next != '@' && *next != '\0' && *next != '/' && *next != '?') {
		hostError(spec, next, "extra characters after host spec", errmsg);
		sts = PM_ERR_GENERIC;
		goto fail;
	    }
	    start++; /* past the initial '[' */
	}
	else
	    next = s;
	if (*next == ':' || *next == '@' || *next == '\0' || *next == '/' || *next == '?') {
	    if (s == *position)
		break;
	    else if (s == start)
		continue;
	    hsp = hostAdd(hsp, &nhosts, start, s - start);
	    if (hsp == NULL) {
		sts = -ENOMEM;
		goto fail;
	    }
	    s = next;
	    if (*s == ':') {
		for (++s, start = s; s != NULL; s++) {
		    if (*s == ',' || *s == '@' || *s == '\0' || *s == '/' || *s == '?') {
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
			if (*s == '@' || *s == '\0' || *s == '/' || *s == '?')
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
    *position = (char *)s;
    *count = nhosts;
    *rslt = hsp;
    return 0;

fail:
    __pmFreeHostSpec(hsp, nhosts);
    *rslt = NULL;
    *count = 0;
    return sts;
}

/*
 * Parse a socket path.
 * Accept anything up to, but not including the first ':', '?' or the end of the spec.
 */
static int      /* 0 -> ok, PM_ERR_GENERIC -> error message is set */
parseSocketPath(
    const char *spec,
    char **position,            /* parse this string, return end char */
    pmHostSpec **rslt)          /* result allocated and returned here */
{
    pmHostSpec *hsp = NULL;
    const char *s, *start, *path;
    char absolute_path[MAXPATHLEN];
    size_t len;
    int nhosts = 0, delimited = 0;

    /* Scan to the end of the string or to the first delimiter. */
    for (s = start = *position; s != NULL; s++) {
	if (*s == '\0')
	    break;
	if (*s == ':' || *s == '?') {
	    delimited = 1;
	    break;
	}
    }

    /* If the path is empty, then provide the default. */
    if (s == start || (delimited && s == start + 1)) {
	path = __pmPMCDLocalSocketDefault();
	len = strlen(path);
    }
    else {
	path = start;
	len = s - start;
	if (len >= MAXPATHLEN)
	    len = MAXPATHLEN - 1;
    }

    /*
     * Make sure that the path is absolute. parseProtocolSpec() removes the
     * (optional) "//" from "local://some/path".
     */
    if (*path != pmPathSeparator()) {
	absolute_path[0] = pmPathSeparator();
	strncpy(absolute_path + 1, path, len);
	if (len < sizeof(absolute_path)-1)
	    absolute_path[++len] = '\0';
	absolute_path[sizeof(absolute_path)-1] = '\0';
	path = absolute_path;
    }

    /* Add the path as the only member of the host list. */
    hsp = hostAdd(hsp, &nhosts, path, len);
    if (hsp == NULL) {
	__pmFreeHostSpec(hsp, nhosts);
	*rslt = NULL;
	return -ENOMEM;
    }

    *position = (char *)s;
    *rslt = hsp;
    return 0;
}

int
__pmParseHostSpec(
    const char *spec,           /* parse this string */
    pmHostSpec **rslt,          /* result allocated and returned here */
    int *count,			/* number of host specs returned here */
    char **errmsg)              /* error message */
{
    char *s = (char *)spec;
    int sts;

    if ((sts = parseHostSpec(spec, &s, rslt, count, errmsg)) < 0)
	return sts;

    if (*s == '\0')
	return 0;

    hostError(spec, s, "unexpected terminal character", errmsg);
    __pmFreeHostSpec(*rslt, *count);
    *rslt = NULL;
    *count = 0;
    return PM_ERR_GENERIC;
}

static int
unparseHostSpec(pmHostSpec *hostp, int count, char *string, size_t size, int prefix)
{
    int off = 0, len = size;	/* offset in string and space remaining */
    int i, j, sts;

    for (i = 0; i < count; i++) {
	if (i > 0) {
	    if ((sts = pmsprintf(string + off, len, "@")) >= size) {
		off = -E2BIG;
		goto done;
	    }
	    len -= sts; off += sts;
	}

	if (prefix && hostp[i].nports == PM_HOST_SPEC_NPORTS_LOCAL) {
	    if ((sts = pmsprintf(string + off, len, "local:/%s", hostp[i].name + 1)) >= size) {
		off = -E2BIG;
		goto done;
	    }
	}
	else if (prefix && hostp[i].nports == PM_HOST_SPEC_NPORTS_UNIX) {
	    if ((sts = pmsprintf(string + off, len, "unix:/%s", hostp[i].name + 1)) >= size) {
		off = -E2BIG;
		goto done;
	    }
	}
	else {
	    if ((sts = pmsprintf(string + off, len, "%s", hostp[i].name)) >= size) {
		off = -E2BIG;
		goto done;
	    }
	}
	len -= sts; off += sts;

	for (j = 0; j < hostp[i].nports; j++) {
	    if ((sts = pmsprintf(string + off, len,
			    "%c%u", (j == 0) ? ':' : ',',
			    hostp[i].ports[j])) >= size) {
		off = -E2BIG;
		goto done;
	    }
	    len -= sts; off += sts;
	}
    }

done:
    if (pmDebugOptions.context) {
	fprintf(stderr, "__pmUnparseHostSpec([name=%s ports=%p nport=%d], count=%d, ...) -> ", hostp->name, hostp->ports, hostp->nports, count);
	if (off < 0) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    pmErrStr_r(off, errmsg, sizeof(errmsg));
	    fprintf(stderr, "%s\n", errmsg);
	}
	else
	    fprintf(stderr, "%d \"%s\"\n", off, string);
    }
    return off;
}

int
__pmUnparseHostSpec(pmHostSpec *hostp, int count, char *string, size_t size)
{
    return unparseHostSpec(hostp, count, string, size, 1);
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

static __pmHashWalkState
attrHashNodeDel(const __pmHashNode *tp, void *cp)
{
    (void)cp;
    if (tp->data)
	free(tp->data);
    return PM_HASH_WALK_DELETE_NEXT;
}

void
__pmFreeAttrsSpec(__pmHashCtl *attrs)
{
    __pmHashWalkCB(attrHashNodeDel, NULL, attrs);
}

void
__pmFreeHostAttrsSpec(pmHostSpec *hosts, int count, __pmHashCtl *attrs)
{
    __pmFreeHostSpec(hosts, count);
    __pmFreeAttrsSpec(attrs);
}

#define PCP_PROTOCOL_NAME	"pcp"
#define PCP_PROTOCOL_PREFIX	PCP_PROTOCOL_NAME ":"
#define PCP_PROTOCOL_SIZE	(sizeof(PCP_PROTOCOL_NAME)-1)
#define PCP_PROTOCOL_PREFIXSZ	(sizeof(PCP_PROTOCOL_PREFIX)-1)
#define PCPS_PROTOCOL_NAME	"pcps"
#define PCPS_PROTOCOL_PREFIX	PCPS_PROTOCOL_NAME ":"
#define PCPS_PROTOCOL_SIZE	(sizeof(PCPS_PROTOCOL_NAME)-1)
#define PCPS_PROTOCOL_PREFIXSZ	(sizeof(PCPS_PROTOCOL_PREFIX)-1)
#define LOCAL_PROTOCOL_NAME	"local"
#define LOCAL_PROTOCOL_PREFIX	LOCAL_PROTOCOL_NAME ":"
#define LOCAL_PROTOCOL_SIZE	(sizeof(LOCAL_PROTOCOL_NAME)-1)
#define LOCAL_PROTOCOL_PREFIXSZ	(sizeof(LOCAL_PROTOCOL_PREFIX)-1)
#define UNIX_PROTOCOL_NAME	"unix"
#define UNIX_PROTOCOL_PREFIX	UNIX_PROTOCOL_NAME ":"
#define UNIX_PROTOCOL_SIZE	(sizeof(UNIX_PROTOCOL_NAME)-1)
#define UNIX_PROTOCOL_PREFIXSZ	(sizeof(UNIX_PROTOCOL_PREFIX)-1)

static int
parseProtocolSpec(
    const char *spec,           /* the original, complete string to parse */
    char **position,
    int *attribute,
    char **value,
    char **errmsg)
{
    char *protocol = NULL;
    char *s = *position;

    /* optionally extract protocol specifier */
    if (strncmp(s, PCP_PROTOCOL_PREFIX, PCP_PROTOCOL_PREFIXSZ) == 0) {
	protocol = PCP_PROTOCOL_NAME;
	s += PCP_PROTOCOL_PREFIXSZ;
	*attribute = PCP_ATTR_PROTOCOL;
    } else if (strncmp(s, PCPS_PROTOCOL_PREFIX, PCPS_PROTOCOL_PREFIXSZ) == 0) {
	protocol = PCPS_PROTOCOL_NAME;
	s += PCPS_PROTOCOL_PREFIXSZ;
	*attribute = PCP_ATTR_PROTOCOL;
    } else if (strncmp(s, LOCAL_PROTOCOL_PREFIX, LOCAL_PROTOCOL_PREFIXSZ) == 0) {
	protocol = LOCAL_PROTOCOL_NAME;
	s += LOCAL_PROTOCOL_PREFIXSZ;
	*attribute = PCP_ATTR_LOCAL;
    } else if (strncmp(s, UNIX_PROTOCOL_PREFIX, UNIX_PROTOCOL_PREFIXSZ) == 0) {
	protocol = UNIX_PROTOCOL_NAME;
	s += UNIX_PROTOCOL_PREFIXSZ;
	*attribute = PCP_ATTR_UNIXSOCK;
    }

    /* optionally skip over slash-delimiters */
    if (protocol) {
	while (*s == '/')
	    s++;
	if ((*value = strdup(protocol)) == NULL)
	    return -ENOMEM;
    } else {
	*value = NULL;
	*attribute = PCP_ATTR_NONE;
    }

    *position = s;
    return 0;
}

__pmAttrKey
__pmLookupAttrKey(const char *attribute, size_t size)
{
    if (size == sizeof("compress") &&
	strncmp(attribute, "compress", size) == 0)
	return PCP_ATTR_COMPRESS;
    if ((size == sizeof("userauth") &&
	strncmp(attribute, "userauth", size) == 0) ||
        (size == sizeof("authorise") &&
	(strncmp(attribute, "authorise", size) == 0 ||
	strncmp(attribute, "authorize", size) == 0)))
	return PCP_ATTR_USERAUTH;
    if ((size == sizeof("user") &&
	strncmp(attribute, "user", size) == 0) ||
	(size == sizeof("username") &&
	strncmp(attribute, "username", size) == 0))
	return PCP_ATTR_USERNAME;
    if (size == sizeof("realm") &&
	strncmp(attribute, "realm", size) == 0)
	return PCP_ATTR_REALM;
    if ((size == sizeof("authmeth") &&
	strncmp(attribute, "authmeth", size) == 0) ||
	(size == sizeof("method") &&
	strncmp(attribute, "method", size) == 0))
	return PCP_ATTR_METHOD;
    if ((size == sizeof("pass") &&
	strncmp(attribute, "pass", size) == 0) ||
	(size == sizeof("password") &&
	strncmp(attribute, "password", size) == 0))
	return PCP_ATTR_PASSWORD;
    if ((size == sizeof("unix") &&
	strncmp(attribute, "unix", size) == 0) ||
	(size == sizeof("unixsock") &&
	strncmp(attribute, "unixsock", size) == 0))
	return PCP_ATTR_UNIXSOCK;
    if ((size == sizeof("local") &&
	 strncmp(attribute, "local", size) == 0))
	return PCP_ATTR_LOCAL;
    if ((size == sizeof("uid") &&
	strncmp(attribute, "uid", size) == 0) ||
	(size == sizeof("userid") &&
	strncmp(attribute, "userid", size) == 0))
	return PCP_ATTR_USERID;
    if ((size == sizeof("gid") &&
	strncmp(attribute, "gid", size) == 0) ||
	(size == sizeof("groupid") &&
	strncmp(attribute, "groupid", size) == 0))
	return PCP_ATTR_GROUPID;
    if ((size == sizeof("pid") &&
	strncmp(attribute, "pid", size) == 0) ||
	(size == sizeof("processid") &&
	strncmp(attribute, "processid", size) == 0))
	return PCP_ATTR_PROCESSID;
    if (size == sizeof("secure") &&
	strncmp(attribute, "secure", size) == 0)
	return PCP_ATTR_SECURE;
    if (size == sizeof("container") &&
	strncmp(attribute, "container", size) == 0)
	return PCP_ATTR_CONTAINER;
    if (size == sizeof("exclusive") &&
	strncmp(attribute, "exclusive", size) == 0)	/* deprecated */
	return PCP_ATTR_EXCLUSIVE;
    return PCP_ATTR_NONE;
}

/*
 * Parse the attributes component of a PCP connection string.
 * Optionally, an initial attribute:value pair can be passed
 * in as well to add to the parsed set.
 */
static int
parseAttributeSpec(
    const char *spec,           /* the original, complete string to parse */
    char **position,            /* parse from here onward and update at end */
    int attribute,
    char *value,
    __pmHashCtl *attributes,
    char **errmsg)
{
    char *s, *start, *v = NULL;
    char buffer[32];	/* must be large enough to hold largest attr name */
    int buflen, attr, len, sts;

    if (attribute != PCP_ATTR_NONE)
	if ((sts = __pmHashAdd(attribute, (void *)value, attributes)) < 0)
	    return sts;

    for (s = start = *position; s != NULL; s++) {
	/* parse: foo=bar&moo&goo=blah ... go! */
	if (*s == '\0' || *s == '/' || *s == '&') {
	    if ((*s == '\0' || *s == '/') && s == start)
		break;
	    len = v ? (v - start - 1) : (s - start);
	    buflen = (len < sizeof(buffer)-1) ? len : sizeof(buffer)-1;
	    strncpy(buffer, start, buflen);
	    buffer[buflen] = '\0';
	    attr = __pmLookupAttrKey(buffer, buflen+1);
	    if (attr != PCP_ATTR_NONE) {
		char *val = NULL;

		if (v && (val = strndup(v, s - v)) == NULL) {
		    sts = -ENOMEM;
		    goto fail;
		}
		if ((sts = __pmHashAdd(attr, (void *)val, attributes)) < 0) {
		    free(val);
		    goto fail;
		}
	    }
	    v = NULL;
	    if (*s == '\0' || *s == '/')
		break;
	    start = s + 1;	/* start of attribute name */
	    continue;
	}
	if (*s == '=') {
	   v = s + 1;	/* start of attribute value */
	}
    }

    *position = s;
    return 0;

fail:
    if (attribute != PCP_ATTR_NONE)	/* avoid double free in caller */
	__pmHashDel(attribute, (void *)value, attributes);
    __pmFreeAttrsSpec(attributes);
    return sts;
}

/*
 * Finally, bring it all together to handle parsing full connection URLs:
 *
 * pcp://oss.sgi.com:45892?user=otto&pass=blotto&compress=true
 * pcps://oss.sgi.com@proxy.org:45893?user=jimbo&pass=jones&compress=true
 * local://path/to/socket:?user=jimbo&pass=jones
 * unix://path/to/socket
 */
int
__pmParseHostAttrsSpec(
    const char *spec,           /* the original, complete string to parse */
    pmHostSpec **host,          /* hosts result allocated and returned here */
    int *count,
    __pmHashCtl *attributes,
    char **errmsg)              /* error message */
{
    char *value = NULL, *s = (char *)spec;
    int sts, attr;

    *count = 0;			/* ensure this initialised for fail: code */

    /* parse optional protocol section */
    if ((sts = parseProtocolSpec(spec, &s, &attr, &value, errmsg)) < 0)
	return sts;

    if (attr == PCP_ATTR_LOCAL || attr == PCP_ATTR_UNIXSOCK) {
	/* We are looking for a socket path. */
	if ((sts = parseSocketPath(spec, &s, host)) < 0)
	    goto fail;
	*count = 1;
	host[0]->nports = (attr == PCP_ATTR_LOCAL) ?
	    PM_HOST_SPEC_NPORTS_LOCAL : PM_HOST_SPEC_NPORTS_UNIX;
    }
    else {
	/* We are looking for a host spec. */
	if ((sts = parseHostSpec(spec, &s, host, count, errmsg)) < 0)
	    goto fail;
    }

    /* skip over an attributes delimiter */
    if (*s == '?') {
	s++;	/* optionally skip over the question mark */
    } else if (*s != '\0' && *s != '/') {
	hostError(spec, s, "unexpected terminal character", errmsg);
	sts = PM_ERR_GENERIC;
	goto fail;
    }

    /* parse optional attributes section */
    if ((sts = parseAttributeSpec(spec, &s, attr, value, attributes, errmsg)) < 0)
	goto fail;

    return 0;

fail:
    if (value)
	free(value);
    if (*count)
	__pmFreeHostSpec(*host, *count);
    *count = 0;
    *host = NULL;
    return sts;
}

static int
unparseAttribute(__pmHashNode *node, char *string, size_t size)
{
    return __pmAttrStr_r(node->key, node->data, string, size);
}

int
__pmAttrKeyStr_r(__pmAttrKey key, char *string, size_t size)
{
    switch (key) {
    case PCP_ATTR_PROTOCOL:
	return pmsprintf(string, size, "protocol");
    case PCP_ATTR_COMPRESS:
	return pmsprintf(string, size, "compress");
    case PCP_ATTR_USERAUTH:
	return pmsprintf(string, size, "userauth");
    case PCP_ATTR_USERNAME:
	return pmsprintf(string, size, "username");
    case PCP_ATTR_AUTHNAME:
	return pmsprintf(string, size, "authname");
    case PCP_ATTR_PASSWORD:
	return pmsprintf(string, size, "password");
    case PCP_ATTR_METHOD:
	return pmsprintf(string, size, "method");
    case PCP_ATTR_REALM:
	return pmsprintf(string, size, "realm");
    case PCP_ATTR_SECURE:
	return pmsprintf(string, size, "secure");
    case PCP_ATTR_UNIXSOCK:
	return pmsprintf(string, size, "unixsock");
    case PCP_ATTR_LOCAL:
	return pmsprintf(string, size, "local");
    case PCP_ATTR_USERID:
	return pmsprintf(string, size, "userid");
    case PCP_ATTR_GROUPID:
	return pmsprintf(string, size, "groupid");
    case PCP_ATTR_PROCESSID:
	return pmsprintf(string, size, "processid");
    case PCP_ATTR_CONTAINER:
	return pmsprintf(string, size, "container");
    case PCP_ATTR_EXCLUSIVE:
	return pmsprintf(string, size, "exclusive");	/* deprecated */
    case PCP_ATTR_NONE:
    default:
	break;
    }
    return 0;
}

int
__pmAttrStr_r(__pmAttrKey key, const char *data, char *string, size_t size)
{
    char name[16];	/* must be sufficient to hold any key name (above) */
    int sts;

    if ((sts = __pmAttrKeyStr_r(key, name, sizeof(name))) <= 0)
	return sts;

    switch (key) {
    case PCP_ATTR_PROTOCOL:
    case PCP_ATTR_USERNAME:
    case PCP_ATTR_PASSWORD:
    case PCP_ATTR_METHOD:
    case PCP_ATTR_REALM:
    case PCP_ATTR_SECURE:
    case PCP_ATTR_USERID:
    case PCP_ATTR_GROUPID:
    case PCP_ATTR_PROCESSID:
    case PCP_ATTR_CONTAINER:
	return pmsprintf(string, size, "%s=%s", name, data ? data : "");

    case PCP_ATTR_UNIXSOCK:
    case PCP_ATTR_LOCAL:
    case PCP_ATTR_COMPRESS:
    case PCP_ATTR_USERAUTH:
    case PCP_ATTR_EXCLUSIVE:	/* deprecated */
	return pmsprintf(string, size, "%s", name);

    case PCP_ATTR_NONE:
    default:
	break;
    }
    return 0;
}

int
__pmUnparseHostAttrsSpec(
    pmHostSpec *hosts,
    int count,
    __pmHashCtl *attrs,
    char *string,
    size_t size)
{
    __pmHashNode *node;
    int off = 0, len = size;	/* offset in string and space remaining */
    int sts, first;

    if ((node = __pmHashSearch(PCP_ATTR_PROTOCOL, attrs)) != NULL) {
	if ((sts = pmsprintf(string, len, "%s://", (char *)node->data)) >= len)
	    return -E2BIG;
	len -= sts; off += sts;
    }
    else if (__pmHashSearch(PCP_ATTR_UNIXSOCK, attrs) != NULL) {
	if ((sts = pmsprintf(string, len, "unix:/")) >= len)
	    return -E2BIG;
	len -= sts; off += sts;
    }
    else if (__pmHashSearch(PCP_ATTR_LOCAL, attrs) != NULL) {
	if ((sts = pmsprintf(string, len, "local:/")) >= len)
	    return -E2BIG;
	len -= sts; off += sts;
    }

    if ((sts = unparseHostSpec(hosts, count, string + off, len, 0)) >= len)
	return sts;
    len -= sts; off += sts;

    first = 1;
    for (node = __pmHashWalk(attrs, PM_HASH_WALK_START);
	 node != NULL;
	 node = __pmHashWalk(attrs, PM_HASH_WALK_NEXT)) {
	if (node->key == PCP_ATTR_PROTOCOL ||
	    node->key == PCP_ATTR_UNIXSOCK || node->key == PCP_ATTR_LOCAL)
	    continue;
	if ((sts = pmsprintf(string + off, len, "%c", first ? '?' : '&')) >= len)
	    return -E2BIG;
	len -= sts; off += sts;
	first = 0;

	if ((sts = unparseAttribute(node, string + off, len)) >= len)
	    return -E2BIG;
	len -= sts; off += sts;
    }

    return off;
}
