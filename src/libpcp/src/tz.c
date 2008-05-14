/*
 * Copyright (c) 1995-2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static char	*envtz = NULL;		/* buffer in env */
static int	envtzlen = 0;

static char	*savetz = NULL;		/* real $TZ from env */
static char	**savetzp;

static int	nzone;				/* table of zones */
static int	curzone = -1;
static char	**zone = NULL;

#if !defined(HAVE_UNDERBAR_ENVIRON)
#define _environ environ
#endif

extern char **_environ;

static void
_pushTZ(void)
{
    char	**p;

    savetzp = NULL;
    for (p = _environ; *p != NULL; p++) {
	if (strncmp(*p, "TZ=", 3) == 0) {
	    savetz = *p;
	    *p = envtz;
	    savetzp = p;
	    break;
	}
    }
    if (*p == NULL)
	putenv(envtz);
    tzset();
}

static void
_popTZ(void)
{
    if (savetzp != NULL)
	*savetzp = savetz;
    else
	putenv("TZ=");
    tzset();
}

/*
 * Construct TZ=... subject to the constraint that the length of the
 * timezone part is not more than PM_TZ_MAXLEN bytes
 * Assumes TZ= is in the start of tzbuffer and this is not touched.
 * And finally set TZ in the environment.
 */
static char *
__pmSquashTZ(char *tzbuffer)
{
    time_t	now = time(NULL);
    struct tm	*t;
    time_t	offset; 
    char	*tzn;

    tzset();
    t = localtime(&now);

#ifdef HAVE_ALTZONE
    offset = (t->tm_isdst > 0) ? altzone : timezone;
#elif defined HAVE_STRFTIME_z
    {
	char tzoffset[6]; /* +1200\0 */

	strftime (tzoffset, sizeof (tzoffset), "%z", t);
	offset = -strtol (tzoffset, NULL, 10);
	offset = ((offset/100) * 3600) + ((offset%100) * 60);
    }
#else
    {
	struct tm	*gmt = gmtime(&now);
	offset = (gmt->tm_hour - t->tm_hour) * 3600 +
		 (gmt->tm_min - t->tm_min) * 60;
    }
#endif

    tzn = tzname[(t->tm_isdst > 0)];

    if (offset != 0) {
	int hours = offset / 3600;
	int mins = abs ((offset % 3600) / 60);
	int len = (int) strlen(tzn);

	if (mins == 0) {
	    /* -3 for +HH in worst case */
	    if (len > PM_TZ_MAXLEN-3) len = PM_TZ_MAXLEN-3;
	    snprintf(tzbuffer+3, PM_TZ_MAXLEN, "%*.*s%+d", len, len, tzn, hours);
	}
	else {
	    /* -6 for +HH:MM in worst case */
	    if (len > PM_TZ_MAXLEN-6) len = PM_TZ_MAXLEN-6;
	    snprintf(tzbuffer+3, PM_TZ_MAXLEN, "%*.*s%+d:%02d", len, len, tzn, hours, mins);
	}
    }
    else {
	strncpy(tzbuffer+3, tzn, PM_TZ_MAXLEN);
	tzbuffer[PM_TZ_MAXLEN+4-1] = '\0';
    }

    putenv(tzbuffer);
    return tzbuffer+3;
}

/*
 * __pmTimezone: work out local timezone
 */
char *
__pmTimezone(void)
{
    static char tzbuffer[PM_TZ_MAXLEN+4] = "TZ=";
    char * tz = getenv("TZ");

    if (tz == NULL || tz[0] == ':') {
	/* NO TZ in the environment - invent one. If TZ starts with a colon,
	 * it's an Olson-style TZ and it does not supported on all IRIXes, so
	 * squash it into a simple one (pv#788431). */
	tz = __pmSquashTZ(tzbuffer);
    } else if (strlen(tz) > PM_TZ_MAXLEN) {
	/* TZ is too long to fit into the internal PCP timezone structs
	 * let's try to sqash it a bit */
	char *tb;

	if ((tb = strdup(tz)) == NULL) {
	    /* sorry state of affairs, go squash w/out malloc */
	    tz = __pmSquashTZ(tzbuffer);
	}
	else {
	    char *ptz = tz;
	    char *zeros;
	    char *end = tb;

	    while ((zeros = strstr(ptz, ":00")) != NULL) {
		strncpy (end, ptz, zeros-ptz);
		end += zeros-ptz;
		*end = '\0';
		ptz = zeros+3;
	    }

	    if (strlen(tb) > PM_TZ_MAXLEN) { 
		/* Still too long - let's pretend it's Olson */
		tz=__pmSquashTZ(tzbuffer);
	    } else {
		strcpy (tzbuffer+3, tb);
		putenv (tzbuffer);
		tz = tzbuffer+3;
	    }

	    free (tb);
	}
    }

    return tz;
}

int
pmUseZone(const int tz_handle)
{
    if (tz_handle < 0 || tz_handle >= nzone)
	return -1;
    
    curzone = tz_handle;
    strcpy(&envtz[3], zone[curzone]);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmUseZone(%d) tz=%s\n", curzone, zone[curzone]);
#endif

    return 0;
}

int
pmNewZone(const char *tz)
{
    int		len;
    int		hack = 0;

    len = (int)strlen(tz);
    if (len == 3) {
	/*
	 * things like TZ=GMT may be broken in libc, particularly
	 * in _ltzset() of time_comm.c, where changes to TZ are
	 * sometimes not properly reflected.
	 * TZ=GMT+0 avoids the problem.
	 */
	len += 2;
	hack = 1;
    }

    if (len+4 > envtzlen) {
	/* expand buffer for env */
	if (envtz != NULL)
	    free(envtz);
	envtzlen = len+4;
	envtz = (char *)malloc(envtzlen);
	strcpy(envtz, "TZ=");
    }
    strcpy(&envtz[3], tz);
    if (hack)
	/* see above */
	strcpy(&envtz[6], "+0");

    curzone = nzone++;
    zone = (char **)realloc(zone, nzone * sizeof(char *));
    if (zone == NULL) {
	__pmNoMem("pmNewZone", nzone * sizeof(char *), PM_FATAL_ERR);
    }
    zone[curzone] = strdup(&envtz[3]);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmNewZone(%s) -> %d\n", zone[curzone], curzone);
#endif

    return curzone;
}

int
pmNewContextZone(void)
{
    __pmContext	*ctxp;
    int		sts;
    static pmID	pmid = 0;
    pmResult	*rp;

    if ((ctxp = __pmHandleToPtr(pmWhichContext())) == NULL)
	return PM_ERR_NOCONTEXT;

    if (ctxp->c_type == PM_CONTEXT_ARCHIVE)
	sts = pmNewZone(ctxp->c_archctl->ac_log->l_label.ill_tz);
    else if (ctxp->c_type == PM_CONTEXT_LOCAL)
	/* from env, not PMCD */
	sts = pmNewZone(__pmTimezone());
    else {
	/* assume PM_CONTEXT_HOST */
	if (pmid == 0) {
	    char	*name = "pmcd.timezone";
	    if ((sts = pmLookupName(1, &name, &pmid)) < 0)
		return sts;
	}
	if ((sts = pmFetch(1, &pmid, &rp)) >= 0) {
	    if (rp->vset[0]->numval == 1 && 
		(rp->vset[0]->valfmt == PM_VAL_DPTR || rp->vset[0]->valfmt == PM_VAL_SPTR))
		sts = pmNewZone((char *)rp->vset[0]->vlist[0].value.pval->vbuf);
	    else
		sts = PM_ERR_VALUE;
	    pmFreeResult(rp);
	}
    }
    
    return sts;
}

char *
pmCtime(const time_t *clock, char *buf)
{
#ifndef IS_SOLARIS
    static struct tm	tbuf;
#endif
    if (curzone >= 0) {
	_pushTZ();
#ifdef IS_SOLARIS
	strcpy(buf, asctime(localtime(clock)));
#else
	asctime_r(localtime_r(clock, &tbuf), buf);
#endif
	_popTZ();
    }
    else {
#ifdef IS_SOLARIS
	strcpy(buf, asctime(localtime(clock)));
#else
	asctime_r(localtime_r(clock, &tbuf), buf);
#endif
    }

    return buf;
}

struct tm *
pmLocaltime(const time_t *clock, struct tm *result)
{
#ifdef IS_SOLARIS
    struct tm	*tmp;
#endif
    if (curzone >= 0) {
	_pushTZ();
#ifdef IS_SOLARIS
	tmp = localtime(clock);
        memcpy(result, tmp, sizeof(*result));
#else
	localtime_r(clock, result);
#endif
	_popTZ();
    }
    else {
#ifdef IS_SOLARIS
	tmp = localtime(clock);
        memcpy(result, tmp, sizeof(*result));
#else
	localtime_r(clock, result);
#endif
    }

    return result;
}

time_t
__pmMktime(struct tm *timeptr)
{
    time_t	ans;

    if (curzone >= 0) {
	_pushTZ();
	ans = mktime(timeptr);
	_popTZ();
    }
    else
	ans = mktime(timeptr);

    return ans;
}

int
pmWhichZone(char **tz)
{
    if (curzone >= 0)
	*tz = zone[curzone];

    return curzone;
}
