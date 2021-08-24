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

    if (pmDebugOptions.context)
	fprintf(stderr, "__pmSquashTZ(%s)", tzbuffer);

    tzset();
    t = localtime(&now);		/* THREADSAFE */

#ifdef HAVE_ALTZONE
    offset = (t->tm_isdst > 0) ? altzone : timezone;
#elif defined HAVE_STRFTIME_z
    {
	char tzoffset[6]; /* +1200\0 */

	strftime(tzoffset, sizeof (tzoffset), "%z", t);
	offset = -strtol(tzoffset, NULL, 10);
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
	int mins = abs((int)((offset % 3600) / 60));
	int len = (int)strlen(tzn);

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
    if (pmDebugOptions.context)
	fprintf(stderr, " -> %s\n", tzbuffer);
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

    if (pmDebugOptions.context)
	fprintf(stderr, "__pmSquashTZ(%s)", tzbuffer);

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
    if (pmDebugOptions.context)
	fprintf(stderr, " -> %s\n", tzbuffer);
    setenv("TZ", tzbuffer, 1);		/* THREADSAFE */

    return;
#endif
}

#define ZONEINFO "/zoneinfo/"
#define LOCALTIME "/etc/localtime"

/*
 * Plan B
 *
 * Get the size for /etc/localtime if possible.
 *
 * Descend /usr/share/zoneinfo looking for a file with the same size
 * and then compare file contents ... if equal, consider it a match.
 *
 * If more than one file matches, pick the first but emit a warning.
 */
static char *
getzoneinfo_plan_b(void)
{
    FILE	*fp;		/* find ... pipe */
    FILE	*f1;		/* /etc/localtime */
    FILE	*f2;		/* candidate file */
    int		c1;
    int		c2;
    char	*p;
    char	*path = NULL;
    struct stat	sbuf;		/* sbuf.st_size is all that matters */
    char	tmp[MAXPATHLEN+2];	/* shell command and line buffer */

    if ((f1 = fopen(LOCALTIME, "r")) == NULL) {
	fprintf(stderr, "getzoneinfo_plan_b: cannot open %s: %s\n", LOCALTIME, pmErrStr(-oserror()));
	return NULL;
    }

    if (fstat(fileno(f1), &sbuf) < 0) {
	fprintf(stderr, "getzoneinfo_plan_b: cannot stat %s: %s\n", LOCALTIME, pmErrStr(-oserror()));
	fclose(f1);
	return NULL;
    }
    sprintf(tmp, "find /usr/share/zoneinfo -type f -a -size %lldc", (long long)sbuf.st_size);
    if ((fp = popen(tmp, "r")) == NULL) {
	fprintf(stderr, "getzoneinfo_plan_b: pipe(%s) failed: %s\n", tmp, pmErrStr(-oserror()));
	fclose(f1);
	return NULL;
    }
    /* start at reading at tmp[1], so ':' can be inserted at tmp[0] */
    while (fgets(&tmp[1], sizeof(tmp)-1, fp) != NULL) {
	/* strip \n at end of line */
	for (p = &tmp[1]; *p != '\n'; p++)
	    ;
	*p = '\0';
	if ((f2 = fopen(&tmp[1], "r")) == NULL) {
	    fprintf(stderr, "getzoneinfo_plan_b: cannot open %s: %s\n", &tmp[1], pmErrStr(-oserror()));
	    fclose(f1);
	    pclose(fp);
	    if (path != NULL)
		free(path);
	    return NULL;
	}
	rewind(f1);

	for ( ; ; ) {
	    c1 = fgetc(f1);
	    c2 = fgetc(f2);
	    if (c1 == EOF && c2 == EOF) {
		/* contents match */
		if (path == NULL) {
		    tmp[0] = ':';
		    path = strdup(tmp);
		    if (path == NULL) {
			fprintf(stderr, "getzoneinfo_plan_b: match %s but strdup failed\n", &tmp[1]);
			fclose(f2);
			fclose(f1);
			pclose(fp);
			return NULL;
		    }
		}
		else {
		    /*
		     * Duplicates ... pick shortest path, as this will favour, for
		     * example, Australia/Melbourne over posix/Australia/Melbourne
		     */
		    if (strlen(&path[1]) <= strlen(&tmp[1]))
			fprintf(stderr, "getzoneinfo_plan_b: Warning: match %s and %s, choosing first one\n", &path[1], &tmp[1]);
		    else {
			fprintf(stderr, "getzoneinfo_plan_b: Warning: match %s and %s, choosing second one\n", &path[1], &tmp[1]);
			free(path);
			tmp[0] = ':';
			path = strdup(tmp);
			if (path == NULL) {
			    fprintf(stderr, "getzoneinfo_plan_b: strdup failed\n");
			    fclose(f2);
			    fclose(f1);
			    pclose(fp);
			    return NULL;
			}
		    }
		}
		break;
	    }
	    if (c1 != c2)
		break;
	}
	fclose(f2);
    }
    fclose(f1);
    pclose(fp);

    return path;
}

/*
 * Get the local timezone tzfile identification
 *
 * We'd like this to return something like ":Australia/Melbourne"
 * that can be used as a $TZ setting.
 *
 * Plan A
 *   If /etc/localtime is a symbolic link, get the pathname it points
 *   to and strip anything up to (and including) the string "/zoneinfo/".
 *
 * Return NULL on failure.
 */
static char *
getzoneinfo(void)
{
    ssize_t	sts;
    char	*buf;
    char	*tmp_buf;
    char	*p;
    char	*q;

    /* +1 for : +1 for NULL */
    buf = (char *)calloc(1, MAXPATHLEN+2);
    if (buf == NULL)
	return NULL;

    sts = readlink(LOCALTIME, &buf[1], MAXPATHLEN);
    if (sts < 0) {
	/*
	 * Hmm, not a symlink. Now try Plan B.
	 */
	free(buf);
	buf = getzoneinfo_plan_b();
	if (buf == NULL)
	    return NULL;
    }
    else
	buf[sts+1] = '\0';

    /* try to find prefix .../zoneinfo/... */
    p = strstr(buf, ZONEINFO);
    if (p != NULL) {
	/* found it! */
	q = &p[strlen(ZONEINFO)-1];
	tmp_buf = strdup(q);
	if (tmp_buf != NULL) {
	    free(buf);
	    buf = tmp_buf;
	}
    }
    else {
	/* no prefix, truncate ... nice to have, not necessary */
	tmp_buf = realloc(buf, sts+2);
	if (tmp_buf != NULL)
	    buf = tmp_buf;
    }
    buf[0] = ':';

    return buf;
}

char *
__pmZoneinfo(void)
{
    return getzoneinfo();
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
	/*
	 * NO TZ in the environment - invent one. If TZ starts with a colon,
	 * it's a file-based TZ and it is not supported on all platforms, so
	 * squash it into a simple one.
	 */
	__pmSquashTZ(tzbuffer);
	tz = tzbuffer;
    } else if (strlen(tz) > PM_TZ_MAXLEN) {
	/*
	 * TZ is too long to fit into the internal PCP timezone structs
	 * let's try to squash it a bit.
	 * */
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
		/* Still too long - let's pretend it's file-based */
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
	sts = pmNewZone(ctxp->c_archctl->ac_log->l_label.timezone);
	PM_UNLOCK(ctxp->c_lock);
    }
    else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	char	tzbuf[PM_TZ_MAXLEN];
	char	*tz, *info;

	/* from env, not PMCD */
	PM_UNLOCK(ctxp->c_lock);
	if ((info = tz = __pmZoneinfo()) == NULL)
	    tz = __pmTimezone_r(tzbuf, sizeof(tzbuf));
	sts = pmNewZone(tz);
	if (info != NULL)
	    free(info);
    }
    else {
	/* assume PM_CONTEXT_HOST */
	const char	*names[] = { "pmcd.timezone", "pmcd.zoneinfo" };
	pmID		pmids[2];
	pmResult	*rp;
	pmValueSet	*tzp, *zip;

	PM_UNLOCK(ctxp->c_lock);
	if ((sts = pmLookupName(2, names, pmids)) < 0) {
	    /* as long as one of the metrics is in the PMNS we're ok */
	    if (pmids[0] == PM_ID_NULL && pmids[1] == PM_ID_NULL)
		return sts;
	}
	if ((sts = pmFetch(2, pmids, &rp)) >= 0) {
	    tzp = rp->vset[0];
	    zip = rp->vset[1];
	    if (zip->numval == 1 &&
		(zip->valfmt == PM_VAL_DPTR || zip->valfmt == PM_VAL_SPTR))
		sts = pmNewZone((char *)zip->vlist[0].value.pval->vbuf);
	    else if (tzp->numval == 1 &&
		(tzp->valfmt == PM_VAL_DPTR || tzp->valfmt == PM_VAL_SPTR))
		sts = pmNewZone((char *)tzp->vlist[0].value.pval->vbuf);
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
#else
    time_t	epoch = 0;
    struct tm	*tmp;
    char	*ap;
#endif

    PM_LOCK(__pmLock_extcall);
    if (curzone >= 0) {
	_pushTZ();
#if defined(IS_SOLARIS) || defined(IS_MINGW)
	tmp = localtime(clock);			/* THREADSAFE */
	if (tmp == NULL) {
	    if (pmDebugOptions.context && pmDebugOptions.desperate)
		fprintf(stderr, "pmCtime: localtime(%" FMT_INT64 ") after _pushTZ() failed\n", (__int64_t)(*clock));
	    tmp = localtime(&epoch);		/* THREADSAFE */
	}
	ap = asctime(tmp);			/* THREADSAFE */
	if (ap == NULL) {
	    if (pmDebugOptions.context && pmDebugOptions.desperate)
		fprintf(stderr, "pmCtime: asctime(%02d:%02d:%02d %02d/%02d/%04d) failed\n",
			(int)tmp->tm_hour, (int)tmp->tm_min, (int)tmp->tm_sec,
			(int)tmp->tm_mday, (int)tmp->tm_mon+1, (int)tmp->tm_year+1900);
	    ap = "???";
	}
	strcpy(buf, ap);
#else
	asctime_r(localtime_r(clock, &tbuf), buf);
#endif
	_popTZ();
    }
    else {
#if defined(IS_SOLARIS) || defined(IS_MINGW)
	tmp = localtime(clock);			/* THREADSAFE */
	if (tmp == NULL) {
	    if (pmDebugOptions.context && pmDebugOptions.desperate)
		fprintf(stderr, "pmCtime: localtime(%" FMT_INT64 ") failed\n", (__int64_t)(*clock));
	    tmp = localtime(&epoch);		/* THREADSAFE */
	}
	ap = asctime(tmp);			/* THREADSAFE */
	if (ap == NULL) {
	    if (pmDebugOptions.context && pmDebugOptions.desperate)
		fprintf(stderr, "pmCtime: asctime(%02d:%02d:%02d %02d/%02d/%04d) failed\n",
			(int)tmp->tm_hour, (int)tmp->tm_min, (int)tmp->tm_sec,
			(int)tmp->tm_mday, (int)tmp->tm_mon+1, (int)tmp->tm_year+1900);
	    ap = "???";
	}
	strcpy(buf, ap);
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
    time_t	epoch = 0;
#endif

    PM_LOCK(__pmLock_extcall);

    if (curzone >= 0) {
	_pushTZ();
#if defined(IS_SOLARIS) || defined(IS_MINGW)
	tmp = localtime(clock);		/* THREADSAFE */
	if (tmp == NULL) {
	    if (pmDebugOptions.context && pmDebugOptions.desperate)
		fprintf(stderr, "pmLocaltime: localtime(%" FMT_INT64 ") after _pushTZ() failed\n", (__int64_t)(*clock));
	    tmp = localtime(&epoch);		/* THREADSAFE */
	}
        memcpy(result, tmp, sizeof(*result));
#else
	localtime_r(clock, result);
#endif
	_popTZ();
    }
    else {
#if defined(IS_SOLARIS) || defined(IS_MINGW)
	tmp = localtime(clock);		/* THREADSAFE */
	if (tmp == NULL) {
	    if (pmDebugOptions.context && pmDebugOptions.desperate)
		fprintf(stderr, "pmLocaltime: localtime(%" FMT_INT64 ") failed\n", (__int64_t)(*clock));
	    tmp = localtime(&epoch);		/* THREADSAFE */
	}
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
