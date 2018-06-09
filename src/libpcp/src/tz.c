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
 * Thread-safe notes
 *
 * These routines manipulate the environment and call lots of routines
 * like localtime(), asctime(), gmtime(), getenv(), setenv() ... all of
 * which are not thread-safe.
 *
 * We use the big lock to prevent concurrent execution.
 *
 * Need to call PM_INIT_LOCKS() in all the exposed routines because we
 * may be called before a context has been created, and missed the
 * lock initialization in pmNewContext().
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"

static char	*envtz;			/* buffer in env */
static int	envtzlen;

static char	*savetz = NULL;		/* real $TZ from env */

static int	nzone;			/* table of zones */
static int	curzone = -1;
static char	**zone;

#if !defined(HAVE_UNDERBAR_ENVIRON)
#define _environ environ
#endif

extern char **_environ;

/*
 * THREADSAFE note - always called with __pmLock_extcall already acquired
 */
static void
_pushTZ(void)
{
    char	*p;

    PM_ASSERT_IS_LOCKED(__pmLock_extcall);

    if (savetz) {
	/* don't need previous saved value any more */
	free(savetz);
	savetz = NULL;
    }

    p = getenv("TZ");		/* THREADSAFE */
    if (p != NULL)
	/* save current value */
	savetz = strdup(p);

    setenv("TZ", envtz, 1);		/* THREADSAFE */
    tzset();

    if (pmDebugOptions.context && pmDebugOptions.desperate)
	fprintf(stderr, "_pushTZ() envtz=\"%s\" savetz=\"%s\" after TZ=\"%s\"\n", envtz, savetz, getenv("TZ"));		/* THREADSAFE */
}

/*
 * THREADSAFE note - always called with __pmLock_extcall already acquired
 */
static void
_popTZ(void)
{
    PM_ASSERT_IS_LOCKED(__pmLock_extcall);

    if (savetz != NULL)
	setenv("TZ", savetz, 1);	/* THREADSAFE */
    else
	unsetenv("TZ");			/* THREADSAFE */
    tzset();

    if (pmDebugOptions.context && pmDebugOptions.desperate)
	fprintf(stderr, "_popTZ() savetz=\"%s\" after TZ=\"%s\"\n", savetz, getenv("TZ"));		/* THREADSAFE */
}

/*
 * Construct TZ=... subject to the constraint that the length of the
 * timezone part is not more than PM_TZ_MAXLEN bytes
 * Assumes TZ= is in the start of tzbuffer and this is not touched.
 * And finally set TZ in the environment.
 *
 * THREADSAFE note - always called with __pmLock_extcall already acquired
 */
static void
__pmSquashTZ(char *tzbuffer)
{
    time_t	now = time(NULL);
    struct tm	*t;
    char	*tzn;
#ifndef IS_MINGW
    time_t	offset; 

    PM_ASSERT_IS_LOCKED(__pmLock_extcall);

    tzset();
    t = localtime(&now);		/* THREADSAFE */

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
	struct tm	*gmt = gmtime(&now);		/* THREADSAFE */
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
	    pmsprintf(tzbuffer, PM_TZ_MAXLEN, "%*.*s%+d", len, len, tzn, hours);
	}
	else {
	    /* -6 for +HH:MM in worst case */
	    if (len > PM_TZ_MAXLEN-6) len = PM_TZ_MAXLEN-6;
	    pmsprintf(tzbuffer, PM_TZ_MAXLEN, "%*.*s%+d:%02d", len, len, tzn, hours, mins);
	}
    }
    else {
	strncpy(tzbuffer, tzn, PM_TZ_MAXLEN);
	tzbuffer[PM_TZ_MAXLEN] = '\0';
    }
    setenv("TZ", tzbuffer, 1);		/* THREADSAFE */

    return;

#else	/* IS_MINGW */
    /*
     * Use the native Win32 API to extract the timezone.  This is
     * a Windows timezone, we want the POSIX style but there's no
     * API, really.  What we've found works, is the same approach
     * the MSYS dll takes - we set TZ their way (below) and then
     * use tzset, then extract.  Note that the %Z and %z strftime
     * parameters do not contain abbreviated names/offsets (they
     * both contain Windows timezone, and both are the same with
     * no TZ).  Note also that putting the Windows name into the
     * environment as TZ does not do anything good (see the tzset
     * MSDN docs).
     */
#define is_upper(c) ((unsigned)(c) - 'A' <= 26)

    TIME_ZONE_INFORMATION tz;
    static const char wildabbr[] = "GMT";
    char tzbuf[256], tzoff[64];
    char *cp, *dst, *off;
    wchar_t *src;
    div_t d;

    GetTimeZoneInformation(&tz);
    dst = cp = tzbuf;
    off = tzoff;
    for (src = tz.StandardName; *src; src++)
	if (is_upper(*src)) *dst++ = *src;
    if (cp == dst) {
	/* In Asian Windows, tz.StandardName may not contain
	   the timezone name. */
	strcpy(cp, wildabbr);
	cp += strlen(wildabbr);
    }
    else
	cp = dst;
    d = div(tz.Bias + tz.StandardBias, 60);
    pmsprintf(cp, sizeof(tzbuf) - (cp - tzbuf), "%d", d.quot);
    pmsprintf(off, sizeof(tzoff) - (off - tzoff), "%d", d.quot);
    if (d.rem) {
	cp = strchr(cp, 0);
	pmsprintf(cp, sizeof(tzbuf) - (cp - tzbuf), ":%d", abs(d.rem));
	off = strchr(off, 0);
	pmsprintf(off, sizeof(tzoff) - (off - tzoff), ":%d", abs(d.rem));
    }
    if (tz.StandardDate.wMonth) {
	cp = strchr(cp, 0);
	dst = cp;
	for (src = tz.DaylightName; *src; src++)
	    if (is_upper(*src)) *dst++ = *src;
	if (cp == dst) {
	    /* In Asian Windows, tz.StandardName may not contain
	       the daylight name. */
	    strcpy(tzbuf, wildabbr);
	    cp += strlen(wildabbr);
	}
	else
	    cp = dst;
	d = div(tz.Bias+tz.DaylightBias, 60);
	pmsprintf(cp, sizeof(tzbuf) - (cp - tzbuf), "%d", d.quot);
	if (d.rem) {
	    cp = strchr(cp, 0);
	    pmsprintf(cp, sizeof(tzbuf) - (cp - tzbuf), ":%d", abs(d.rem));
	}
	cp = strchr(cp, 0);
	cp = strchr(cp, 0);
	pmsprintf(cp, sizeof(tzbuf) - (cp - tzbuf), ",M%d.%d.%d/%d",
		tz.DaylightDate.wMonth,
		tz.DaylightDate.wDay,
		tz.DaylightDate.wDayOfWeek,
		tz.DaylightDate.wHour);
	if (tz.DaylightDate.wMinute || tz.DaylightDate.wSecond) {
	    cp = strchr(cp, 0);
	    pmsprintf(cp, sizeof(tzbuf) - (cp - tzbuf), ":%d", tz.DaylightDate.wMinute);
	}
	if (tz.DaylightDate.wSecond) {
	    cp = strchr(cp, 0);
	    pmsprintf(cp, sizeof(tzbuf) - (cp - tzbuf), ":%d", tz.DaylightDate.wSecond);
	}
	cp = strchr(cp, 0);
	cp = strchr(cp, 0);
	pmsprintf(cp, sizeof(tzbuf) - (cp - tzbuf), ",M%d.%d.%d/%d",
		tz.StandardDate.wMonth,
		tz.StandardDate.wDay,
		tz.StandardDate.wDayOfWeek,
		tz.StandardDate.wHour);
	if (tz.StandardDate.wMinute || tz.StandardDate.wSecond) {
	    cp = strchr(cp, 0);
	    pmsprintf(cp, sizeof(tzbuf) - (cp - tzbuf), ":%d", tz.StandardDate.wMinute);
	}
	if (tz.StandardDate.wSecond) {
	    cp = strchr(cp, 0);
	    pmsprintf(cp, sizeof(tzbuf) - (cp - tzbuf), ":%d", tz.StandardDate.wSecond);
	}
    }
    if (pmDebugOptions.timecontrol)
	fprintf(stderr, "Win32 TZ=%s\n", tzbuf);

    pmsprintf(tzbuffer, PM_TZ_MAXLEN, "%s", tzbuf);
    setenv("TZ", tzbuffer, 1);		/* THREADSAFE */

    tzset();
    t = localtime(&now);		/* THREADSAFE */
    tzn = tzname[(t->tm_isdst > 0)];

    pmsprintf(tzbuffer, PM_TZ_MAXLEN, "%s%s", tzn, tzoff);
    setenv("TZ", tzbuffer, 1);		/* THREADSAFE */

    return;
#endif
}

/*
 * __pmTimezone: work out local timezone
 */
char *
__pmTimezone(void)
{
    static char *tzbuffer = NULL;
    char *tz;

    PM_LOCK(__pmLock_extcall);

    tz = getenv("TZ");		/* THREADSAFE */

    if (tzbuffer == NULL) {
	/*
	 * size is PM_TZ_MAXLEN + null byte terminator
	 */
	tzbuffer = (char *)malloc(PM_TZ_MAXLEN+1);
	if (tzbuffer == NULL) {
	    /* not much we can do here ... */
	    PM_UNLOCK(__pmLock_extcall);
	    return NULL;
	}
	tzbuffer[0] = '\0';
    }

    if (tz == NULL || tz[0] == ':') {
	/* NO TZ in the environment - invent one. If TZ starts with a colon,
	 * it's an Olson-style TZ and it does not supported on all IRIXes, so
	 * squash it into a simple one (pv#788431). */
	__pmSquashTZ(tzbuffer);
	tz = tzbuffer;
    } else if (strlen(tz) > PM_TZ_MAXLEN) {
	/* TZ is too long to fit into the internal PCP timezone structs
	 * let's try to sqash it a bit */
	char *tb;

	if ((tb = strdup(tz)) == NULL) {
	    /* sorry state of affairs, go squash w/out copying buffer */
	    __pmSquashTZ(tzbuffer);
	    tz = tzbuffer;
	}
	else {
	    char *ptz = tz;
	    char *zeros;
	    char *end = tb;

	    while ((zeros = strstr(ptz, ":00")) != NULL) {
		strncpy(end, ptz, zeros-ptz);
		end += zeros-ptz;
		*end = '\0';
		ptz = zeros+3;
	    }

	    if (strlen(tb) > PM_TZ_MAXLEN) { 
		/* Still too long - let's pretend it's Olson */
		__pmSquashTZ(tzbuffer);
		tz = tzbuffer;
	    } else {
		strcpy(tzbuffer, tb);
		setenv("TZ", tzbuffer, 1);		/* THREADSAFE */
		tz = tzbuffer;
	    }

	    free(tb);
	}
    }

    PM_UNLOCK(__pmLock_extcall);
    return tz;
}

/*
 * buffer should be at least PM_TZ_MAXLEN bytes long
 */
char *
__pmTimezone_r(char *buf, int buflen)
{
    strncpy(buf, __pmTimezone(), (size_t)buflen);
    buf[buflen-1] = '\0';
    return buf;
}

int
pmUseZone(const int tz_handle)
{
    PM_LOCK(__pmLock_extcall);

    if (tz_handle < 0 || tz_handle >= nzone) {
	PM_UNLOCK(__pmLock_extcall);
	return -1;
    }

    curzone = tz_handle;
    strcpy(envtz, zone[curzone]);

    if (pmDebugOptions.context)
	fprintf(stderr, "pmUseZone(%d) tz=%s\n", curzone, zone[curzone]);

    PM_UNLOCK(__pmLock_extcall);
    return 0;
}

int
pmNewZone(const char *tz)
{
    int		len;
    int		hack = 0;
    int		sts;

    PM_LOCK(__pmLock_extcall);

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
    strcpy(envtz, tz);
    if (hack)
	/* see above */
	strcat(envtz, "+0");

    curzone = nzone++;
    zone = (char **)realloc(zone, nzone * sizeof(char *));
    if (zone == NULL) {
	PM_UNLOCK(__pmLock_extcall);
	pmNoMem("pmNewZone", nzone * sizeof(char *), PM_FATAL_ERR);
	/* NOTREACHED */
    }
    zone[curzone] = strdup(envtz);

    if (pmDebugOptions.context)
	fprintf(stderr, "pmNewZone(%s) -> %d\n", zone[curzone], curzone);
    sts = curzone;

    PM_UNLOCK(__pmLock_extcall);
    return sts;
}

int
pmNewContextZone(void)
{
    __pmContext	*ctxp;
    int		sts;

    ctxp = __pmHandleToPtr(pmWhichContext());
    if (ctxp == NULL) 
	return PM_ERR_NOCONTEXT;

    if (ctxp->c_type == PM_CONTEXT_ARCHIVE) {
	sts = pmNewZone(ctxp->c_archctl->ac_log->l_label.ill_tz);
	PM_UNLOCK(ctxp->c_lock);
    }
    else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	char	tzbuf[PM_TZ_MAXLEN];
	/* from env, not PMCD */
	PM_UNLOCK(ctxp->c_lock);
	__pmTimezone_r(tzbuf, sizeof(tzbuf));
	sts = pmNewZone(tzbuf);
    }
    else {
	/* assume PM_CONTEXT_HOST */
	char		*name = "pmcd.timezone";
	pmID		pmid;
	pmResult	*rp;

	PM_UNLOCK(ctxp->c_lock);
	if ((sts = pmLookupName(1, &name, &pmid)) < 0)
	    return sts;
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
#if !defined(IS_SOLARIS) && !defined(IS_MINGW)
    struct tm	tbuf;
#endif

    PM_LOCK(__pmLock_extcall);
    if (curzone >= 0) {
	_pushTZ();
#if defined(IS_SOLARIS) || defined(IS_MINGW)
	strcpy(buf, asctime(localtime(clock)));		/* THREADSAFE */
#else
	asctime_r(localtime_r(clock, &tbuf), buf);
#endif
	_popTZ();
    }
    else {
#if defined(IS_SOLARIS) || defined(IS_MINGW)
	strcpy(buf, asctime(localtime(clock)));		/* THREADSAFE */
#else
	asctime_r(localtime_r(clock, &tbuf), buf);
#endif
    }

    PM_UNLOCK(__pmLock_extcall);
    return buf;
}

struct tm *
pmLocaltime(const time_t *clock, struct tm *result)
{
#if defined(IS_SOLARIS) || defined(IS_MINGW)
    struct tm	*tmp;
#endif

    PM_LOCK(__pmLock_extcall);

    if (curzone >= 0) {
	_pushTZ();
#if defined(IS_SOLARIS) || defined(IS_MINGW)
	tmp = localtime(clock);		/* THREADSAFE */
        memcpy(result, tmp, sizeof(*result));
#else
	localtime_r(clock, result);
#endif
	_popTZ();
    }
    else {
#if defined(IS_SOLARIS) || defined(IS_MINGW)
	tmp = localtime(clock);		/* THREADSAFE */
        memcpy(result, tmp, sizeof(*result));
#else
	localtime_r(clock, result);
#endif
    }

    PM_UNLOCK(__pmLock_extcall);
    return result;
}

time_t
__pmMktime(struct tm *timeptr)
{
    time_t	ans;

    PM_LOCK(__pmLock_extcall);

    if (curzone >= 0) {
	_pushTZ();
	ans = mktime(timeptr);
	_popTZ();
    }
    else
	ans = mktime(timeptr);

    PM_UNLOCK(__pmLock_extcall);
    return ans;
}

int
pmWhichZone(char **tz)
{
    int		sts;

    PM_LOCK(__pmLock_extcall);

    if (curzone >= 0)
	*tz = zone[curzone];
    sts = curzone;

    PM_UNLOCK(__pmLock_extcall);
    return sts;
}
