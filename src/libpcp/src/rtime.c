/*
 * Copyright (c) 2014-2015 Red Hat.
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <limits.h>
#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "internal.h"


#define CODE3(a,b,c) ((__uint32_t)(a)|((__uint32_t)(b)<<8)|((__uint32_t)(c)<<16))
#define whatmsg  "unexpected value"
#define moremsg  "more information expected"
#define alignmsg "alignment specified by -A switch could not be applied"

#define N_WDAYS	7
static const __uint32_t wdays[N_WDAYS] = {
    CODE3('s', 'u', 'n'),
    CODE3('m', 'o', 'n'),
    CODE3('t', 'u', 'e'),
    CODE3('w', 'e', 'd'),
    CODE3('t', 'h', 'u'),
    CODE3('f', 'r', 'i'),
    CODE3('s', 'a', 't')
};

#define N_MONTHS 12
static const __uint32_t months[N_MONTHS] = {
    CODE3('j', 'a', 'n'),
    CODE3('f', 'e', 'b'),
    CODE3('m', 'a', 'r'),
    CODE3('a', 'p', 'r'),
    CODE3('m', 'a', 'y'),
    CODE3('j', 'u', 'n'),
    CODE3('j', 'u', 'l'),
    CODE3('a', 'u', 'g'),
    CODE3('s', 'e', 'p'),
    CODE3('o', 'c', 't'),
    CODE3('n', 'o', 'v'),
    CODE3('d', 'e', 'c')
};

#define N_AMPM	2
static const __uint32_t ampm[N_AMPM] = {
    CODE3('a', 'm',  0 ),
    CODE3('p', 'm',  0 )
};

static const struct {
    char	*name;		/* pmParseInterval scale name */
    int		len;		/* length of scale name */
    int		scale;		/* <0 -divisor, else multiplier */
} int_tab[] = {
    { "millisecond",   11, -1000 }, 
    { "second",		6,     1 },
    { "minute",		6,    60 },
    { "hour",		4,  3600 },
    { "day",		3, 86400 },
    { "msec",		4, -1000 },
    { "sec",		3,     1 },
    { "min",		3,    60 },
    { "hr",		2,  3600 },
    { "s",		1,     1 },
    { "m",		1,    60 },
    { "h",		1,  3600 },
    { "d",		1, 86400 },
};
static const int	numint = sizeof(int_tab) / sizeof(int_tab[0]);

#define NO_OFFSET	0
#define PLUS_OFFSET	1
#define NEG_OFFSET	2


/* Compare struct timevals */
static int	/* 0 -> equal, -1 -> tv1 < tv2, 1 -> tv1 > tv2 */
tvcmp(struct timeval tv1, struct timeval tv2)
{
    if (tv1.tv_sec < tv2.tv_sec)
	return -1;
    if (tv1.tv_sec > tv2.tv_sec)
	return 1;
    if (tv1.tv_usec < tv2.tv_usec)
	return -1;
    if (tv1.tv_usec > tv2.tv_usec)
	return 1;
    return 0;
}

/* Recognise three character string as one of the given codes.
   return: 1 == ok, 0 <= *rslt <= ncodes-1, *spec points to following char
	   0 == not found, *spec updated to strip blanks */
static int
parse3char(const char **spec, const __uint32_t *codes, int ncodes, int *rslt)
{
    const char  *scan = *spec;
    __uint32_t  code = 0;
    int	    	i;

    while (isspace((int)*scan))
	scan++;
    *spec = scan;
    if (! isalpha((int)*scan))
	return 0;
    for (i = 0; i <= 16; i += 8) {
	code |= (tolower((int)*scan) << i);
	scan++;
	if (! isalpha((int)*scan))
	    break;
    }
    for (i = 0; i < ncodes; i++) {
	if (code == codes[i]) {
	    *spec = scan;
	    *rslt = i;
	    return 1;
	}
    }
    return 0;
}

/* Recognise single char (with optional leading space) */
static int
parseChar(const char **spec, char this)
{
    const char	*scan = *spec;

    while (isspace((int)*scan))
	scan++;
    *spec = scan;
    if (*scan != this)
	return 0;
    *spec = scan + 1;
    return 1;
}

/* Recognise integer in range min..max
   return: 1 == ok, min <= *rslt <= max, *spec points to following char
	   0 == not found, *spec updated to strip blanks  */
static int
parseInt(const char **spec, int min, int max, int *rslt)
{
    const char	*scan = *spec;
    char	*tmp;
    long	r;

    while (isspace((int)*scan))
	scan++;
    tmp = (char *)scan;
    r = strtol(scan, &tmp, 10);
    *spec = tmp;
    if (scan == *spec || r < min || r > max) {
	*spec = scan;
	return 0;
    }
    *rslt = (int)r;
    return 1;
}

/* Recognise double precision float in the range min..max
   return: 1 == ok, min <= *rslt <= max, *spec points to following char
	   0 == not found, *spec updated to strip blanks  */
static double
parseDouble(const char **spec, double min, double max, double *rslt)
{
    const char	*scan = *spec;
    char	*tmp;
    double	r;

    while (isspace((int)*scan))
	scan++;
    tmp = (char *)scan;
    r = strtod(scan, &tmp);
    *spec = tmp;
    if (scan == *spec || r < min || r > max) {
	*spec = scan;
	return 0;
    }
    *rslt = r;
    return 1;
}

/* Construct error message buffer for syntactic error */
static void
parseError(const char *spec, const char *point, char *msg, char **rslt)
{
    int		need = 2 * (int)strlen(spec) + (int)strlen(msg) + 8;
    const char	*p;
    char	*q;

    if ((*rslt = malloc(need)) == NULL)
	__pmNoMem("__pmParseTime", need, PM_FATAL_ERR);
    q = *rslt;

    for (p = spec; *p != '\0'; p++)
	*q++ = *p;
    *q++ = '\n';
    for (p = spec; p != point; p++)
	*q++ = isgraph((int)*p) ? ' ' : *p;
    sprintf(q, "^ -- ");
    q += 5;
    for (p = msg; *p != '\0'; p++)
	*q++ = *p;
    *q++ = '\n';
    *q = '\0';
}


int		/* 0 -> ok, -1 -> error */
pmParseInterval(
    const char *spec,		/* interval to parse */
    struct timeval *rslt,	/* result stored here */
    char **errmsg)		/* error message */
{
    const char	*scan = spec;
    double	d;
    double	sec = 0.0;
    int		i;
    const char	*p;
    int		len;

    if (scan == NULL || *scan == '\0') {
	const char *empty = "";
	parseError(empty, empty, "Null or empty specification", errmsg);
	return -1;
    }

    /* parse components of spec */
    while (*scan != '\0') {
	if (! parseDouble(&scan, 0.0, (double)INT_MAX, &d))
	    break;
	while (*scan != '\0' && isspace((int)*scan))
	    scan++;
	if (*scan == '\0') {
	    /* no scale, seconds is the default */
	    sec += d;
	    break;
	}
	for (p = scan; *p && isalpha((int)*p); p++)
	    ;
	len = (int)(p - scan);
	if (len == 0)
	    /* no scale, seconds is the default */
	    sec += d;
	else {
	    if (len > 1 && (p[-1] == 's' || p[-1] == 'S'))
		/* ignore any trailing 's' */
		len--;
	    for (i = 0; i < numint; i++) {
		if (len != int_tab[i].len)
		    continue;
		if (strncasecmp(scan, int_tab[i].name, len) == 0)
		    break;
	    }
	    if (i == numint)
		break;
	    if (int_tab[i].scale < 0)
		sec += d / (-int_tab[i].scale);
	    else
		sec += d * int_tab[i].scale;
	}
	scan = p;
    }

    /* error detection */
    if (*scan != '\0') {
	parseError(spec, scan, whatmsg, errmsg);
	return -1;
    }

    /* convert into seconds and microseconds */
    __pmtimevalFromReal(sec, rslt);
    return 0;
}


int		/* 0 -> ok, -1 -> error */
__pmParseCtime(
    const char *spec,		/* ctime string to parse */
    struct tm *rslt,		/* result stored here */
    char **errmsg)		/* error message */
{
    struct tm	tm;
    double	d;
    const char	*scan = spec;
    int		pm = -1;
    int		ignored = -1;
    int		dflt;

    memset(&tm, -1, sizeof(tm));
    tm.tm_wday = NO_OFFSET;

    /* parse time spec */
    parse3char(&scan, wdays, N_WDAYS, &ignored);
    parse3char(&scan, months, N_MONTHS, &tm.tm_mon);

    parseInt(&scan, 0, 31, &tm.tm_mday);
    parseInt(&scan, 0, 23, &tm.tm_hour);
    if (tm.tm_mday == 0 && tm.tm_hour != -1) {
	tm.tm_mday = -1;
    }
    if (tm.tm_hour == -1 && tm.tm_mday >= 0 && tm.tm_mday <= 23 &&
	    (tm.tm_mon == -1 || *scan == ':')) {
	tm.tm_hour = tm.tm_mday;
	tm.tm_mday = -1;
    }
    if (parseChar(&scan, ':')) {
	if (tm.tm_hour == -1)
	    tm.tm_hour = 0;
	tm.tm_min = 0;				/* for moreError checking */
	parseInt(&scan, 0, 59, &tm.tm_min);
	if (parseChar(&scan, ':')) {
	    if (parseDouble(&scan, 0.0, 61.0, &d)) {
		tm.tm_sec = (time_t)d;
		tm.tm_yday = (int)((long double)1000000 * (long double)(d - tm.tm_sec));
	    }
	}
    }
    if (parse3char(&scan, ampm, N_AMPM, &pm)) {
	if (tm.tm_hour > 12 || tm.tm_hour == -1)
	    scan -= 2;
	else {
	    if (pm) {
		if (tm.tm_hour < 12)
		    tm.tm_hour += 12;
	    }
	    else {
		if (tm.tm_hour == 12)
		    tm.tm_hour = 0;
	    }
	}
    }
    /*
     * parse range forces tm_year to be >= 1900, so this is Y2K safe
     */
    if (parseInt(&scan, 1900, 9999, &tm.tm_year))
	tm.tm_year -= 1900;

    /*
     * error detection and reporting
     *
     * in the code below, tm_year is either years since 1900 or
     * -1 (a sentinel), so this is is Y2K safe
     */
    while (isspace((int)*scan))
	scan++;
    if (*scan != '\0') {
	parseError(spec, scan, whatmsg, errmsg);
	return -1;
    }
    if ((ignored != -1 && tm.tm_mon == -1 && tm.tm_mday == -1) ||
        (tm.tm_hour != -1 && tm.tm_min == -1 && tm.tm_mday == -1 &&
	    tm.tm_mon == -1 && tm.tm_year == -1)) {
	parseError(spec, scan, moremsg, errmsg);
	return -1;
    }

    /* fill in elements of tm from spec */
    dflt = (tm.tm_year != -1);
    if (tm.tm_mon != -1)
	dflt = 1;
    else if (dflt)
	tm.tm_mon = 0;
    if (tm.tm_mday != -1)
	dflt = 1;
    else if (dflt)
	tm.tm_mday = 1;
    if (tm.tm_hour != -1)
	dflt = 1;
    else if (dflt)
	tm.tm_hour = 0;
    if (tm.tm_min != -1)
	dflt = 1;
    else if (dflt)
	tm.tm_min = 0;
    if (tm.tm_sec == -1 && dflt) {
	tm.tm_sec = 0;
	tm.tm_yday = 0;
    }

    *rslt = tm;
    return 0;
}


int	/* 0 ok, -1 error */
__pmConvertTime(
    struct tm *tmin,		/* absolute or +ve or -ve offset time */
    struct timeval *origin,	/* defaults and origin for offset */
    struct timeval *rslt)	/* result stored here */
{
    time_t	    t;
    struct timeval  tval = *origin;
    struct tm	    tm;

    /* positive offset interval */
    if (tmin->tm_wday == PLUS_OFFSET) {
	tval.tv_usec += tmin->tm_yday;
	if (tval.tv_usec > 1000000) {
	    tval.tv_usec -= 1000000;
	    tval.tv_sec++;
	}
	tval.tv_sec += tmin->tm_sec;
    }

    /* negative offset interval */
    else if (tmin->tm_wday == NEG_OFFSET) {
	if (tval.tv_usec < tmin->tm_yday) {
	    tval.tv_usec += 1000000;
	    tval.tv_sec--;
	}
	tval.tv_usec -= tmin->tm_yday;
	tval.tv_sec -= tmin->tm_sec;
    }

    /* absolute time */
    else {
	/* tmin completely specified */
	if (tmin->tm_year != -1) {
	    tm = *tmin;
	    tval.tv_usec = tmin->tm_yday;
	}

	/* tmin partially specified */
	else {
	    t = (time_t)tval.tv_sec;
	    pmLocaltime(&t, &tm);
	    tm.tm_isdst = -1;

	    /* fill in elements of tm from spec */
	    if (tmin->tm_mon != -1) {
		if (tmin->tm_mon < tm.tm_mon)
		    /*
		     * tm_year is years since 1900 and the tm_year++ is
		     * adjusting for the specified month being before the
		     * current month, so this is Y2K safe
		     */
		    tm.tm_year++;
		tm.tm_mon = tmin->tm_mon;
		tm.tm_mday = tmin->tm_mday;
		tm.tm_hour = tmin->tm_hour;
		tm.tm_min = tmin->tm_min;
		tm.tm_sec = tmin->tm_sec;
		tval.tv_usec = tmin->tm_yday;
	    }
	    else if (tmin->tm_mday != -1) {
		if (tmin->tm_mday < tm.tm_mday)
		    tm.tm_mon++;
		tm.tm_mday = tmin->tm_mday;
		tm.tm_hour = tmin->tm_hour;
		tm.tm_min = tmin->tm_min;
		tm.tm_sec = tmin->tm_sec;
		tval.tv_usec = tmin->tm_yday;
	    }
	    else if (tmin->tm_hour != -1) {
		if (tmin->tm_hour < tm.tm_hour)
		    tm.tm_mday++;
		tm.tm_hour = tmin->tm_hour;
		tm.tm_min = tmin->tm_min;
		tm.tm_sec = tmin->tm_sec;
		tval.tv_usec = tmin->tm_yday;
	    }
	    else if (tmin->tm_min != -1) {
		if (tmin->tm_min < tm.tm_min)
		    tm.tm_hour++;
		tm.tm_min = tmin->tm_min;
		tm.tm_sec = tmin->tm_sec;
		tval.tv_usec = tmin->tm_yday;
	    }
	    else if (tmin->tm_sec != -1) {
		if (tmin->tm_sec < tm.tm_sec)
		    tm.tm_min++;
		tm.tm_sec = tmin->tm_sec;
		tval.tv_usec = tmin->tm_yday;
	    }
	}
	tval.tv_sec = __pmMktime(&tm);
    }

    *rslt = tval;
    return 0;
}


/*
 * Use heuristics to determine the presence of a relative date time
 * and its direction
 */
static int
glib_relative_date(const char *date_string)
{
    /*
     * Time terms most commonly used with an adjective modifier are
     * relative to the start/end time
     * e.g. last year, 2 year ago, next hour, -1 minute
     */
    char * const startend_relative_terms[] = {
	" YEAR",
	" MONTH",
	" FORTNIGHT",
	" WEEK",
	" DAY",
	" HOUR",
	" MINUTE",
	" MIN",
	" SECOND",
	" SEC"
    };

    /*
     * Time terms for a specific day are relative to the current time
     * TOMORROW, YESTERDAY, TODAY, NOW, MONDAY-SUNDAY
     */
    int rtu_bound = sizeof(startend_relative_terms) / sizeof(void *);
    int rtu_idx;

    while (isspace((int)*date_string))
    	date_string++;
    for (rtu_idx = 0; rtu_idx < rtu_bound; rtu_idx++)
        if (strcasestr(date_string, startend_relative_terms[rtu_idx]) != NULL)
            break;
    if (rtu_idx < rtu_bound) {
	if (strcasestr(date_string, "last") != NULL ||
	    strcasestr(date_string, "ago") != NULL ||
	    date_string[0] == '-')
	    return NEG_OFFSET;
	else
	    return PLUS_OFFSET;
    }
    return NO_OFFSET;
}

/*
 * Helper interface to wrap calls to the __pmGetDate interface
 */
static int
glib_get_date(
    const char		*scan,
    struct timeval	*start,
    struct timeval	*end,
    struct timeval	*rslt)
{
    int sts;
    int rel_type;
    struct timespec tsrslt;

    rel_type = glib_relative_date(scan);

    if (rel_type == NO_OFFSET)
	sts = __pmGetDate(&tsrslt, scan, NULL);
    else if (rel_type == NEG_OFFSET && end->tv_sec < INT_MAX) {
	struct timespec tsend;
	tsend.tv_sec = end->tv_sec;
	tsend.tv_nsec = end->tv_usec * 1000;
	sts = __pmGetDate(&tsrslt, scan, &tsend);
    }
    else {
	struct timespec tsstart;
	tsstart.tv_sec = start->tv_sec;
	tsstart.tv_nsec = start->tv_usec * 1000;
	sts = __pmGetDate(&tsrslt, scan, &tsstart);
    }
    if (sts < 0)
	return sts;

    rslt->tv_sec = tsrslt.tv_sec;
    rslt->tv_usec = tsrslt.tv_nsec / 1000;
    return 0;
}

int	/* 0 -> ok, -1 -> error */
__pmParseTime(
    const char	    *string,	/* string to be parsed */
    struct timeval  *logStart,	/* start of log or current time */
    struct timeval  *logEnd,	/* end of log or tv_sec == INT_MAX */
				/* assumes sizeof(t_time) == sizeof(int) */
    struct timeval  *rslt,	/* if parsed ok, result filled in */
    char	    **errMsg)	/* error message, please free */
{
    struct tm	    tm;
    const char	    *scan;
    struct timeval  start;
    struct timeval  end;
    struct timeval  tval;

    *errMsg = NULL;
    start = *logStart;
    end = *logEnd;
    if (end.tv_sec == INT_MAX)
	end.tv_usec = 999999;
    scan = string;

    /* ctime string */
    if (parseChar(&scan, '@')) {
	if (__pmParseCtime(scan, &tm, errMsg) >= 0) {
	    tm.tm_wday = NO_OFFSET;
	    __pmConvertTime(&tm, &start, rslt);
	    return 0;
	}
    }

    /* relative to end of archive */
    else if (end.tv_sec < INT_MAX && parseChar(&scan, '-')) {
	if (pmParseInterval(scan, &tval, errMsg) >= 0) {
	    tm.tm_wday = NEG_OFFSET;
	    tm.tm_sec = (int)tval.tv_sec;
	    tm.tm_yday = (int)tval.tv_usec;
	    __pmConvertTime(&tm, &end, rslt);
	    return 0;
	}
    }

    /* relative to start of archive or current time */
    else {
	parseChar(&scan, '+');
	if (pmParseInterval(scan, &tval, errMsg) >= 0) {
	    tm.tm_wday = PLUS_OFFSET;
	    tm.tm_sec = (int)tval.tv_sec;
	    tm.tm_yday = (int)tval.tv_usec;
	    __pmConvertTime(&tm,  &start, rslt);
	    return 0;
	}
    }

    /* datetime is not recognised, try the glib_get_date method */
    parseChar(&scan, '@');	/* ignore; glib_relative_date determines type */
    if (glib_get_date(scan, &start, &end, rslt) < 0)
	return -1;

    if (*errMsg)
	free(*errMsg);
    return 0;
}


/* This function is designed to encapsulate the interpretation of
   the -S, -T, -A and -O command line switches for use by the PCP
   client tools. */
int    /* 1 -> ok, 0 -> warning, -1 -> error */
pmParseTimeWindow(
    const char      *swStart,   /* argument of -S switch, may be NULL */
    const char      *swEnd,     /* argument of -T switch, may be NULL */
    const char      *swAlign,   /* argument of -A switch, may be NULL */
    const char	    *swOffset,	/* argument of -O switch, may be NULL */
    const struct timeval  *logStart,  /* start of log or current time */
    const struct timeval  *logEnd,    /* end of log or tv_sec == INT_MAX */
    struct timeval  *rsltStart, /* start time returned here */
    struct timeval  *rsltEnd,   /* end time returned here */
    struct timeval  *rsltOffset,/* offset time returned here */
    char            **errMsg)	/* error message, please free */
{
    struct timeval  astart;
    struct timeval  start;
    struct timeval  end;
    struct timeval  offset;
    struct timeval  aoffset;
    struct timeval  tval;
    const char	    *scan;
    __int64_t	    delta = 0;	/* initialize to pander to gcc */
    __int64_t 	    align;
    __int64_t 	    blign;
    int		    sts = 1;

    /* default values for start and end */
    start = *logStart;
    end = *logEnd;
    if (end.tv_sec == INT_MAX)
	end.tv_usec = 999999;

    /* parse -S argument and adjust start accordingly */
    if (swStart) {
	if (__pmParseTime(swStart, &start, &end, &start, errMsg) < 0)
	    return -1;
    }

    /* sanity check -S */
    if (tvcmp(start, *logStart) < 0)
	/* move start forwards to the beginning of the archive */
	start = *logStart;

    /* parse -A argument and adjust start accordingly */
    if (swAlign) {
	scan = swAlign;
	if (pmParseInterval(scan, &tval, errMsg) < 0)
	    return -1;
	delta = tval.tv_usec + 1000000 * (__int64_t)tval.tv_sec;
	align = start.tv_usec + 1000000 * (__int64_t)start.tv_sec;
	blign = (align / delta) * delta;
	if (blign < align)
	    blign += delta;
	astart.tv_sec = (time_t)(blign / 1000000);
	astart.tv_usec = (int)(blign % 1000000);

	/* sanity check -S after alignment */
	if (tvcmp(astart, *logStart) >= 0 && tvcmp(astart, *logEnd) <= 0)
	    start = astart;
	else {
	    parseError(swAlign, swAlign, alignmsg, errMsg);
	    sts = 0;
	}
    }

    /* parse -T argument and adjust end accordingly */
    if (swEnd) {
	if (__pmParseTime(swEnd, &start, &end, &end, errMsg) < 0)
	    return -1;
    }

    /* sanity check -T */
    if (tvcmp(end, *logEnd) > 0)
	/* move end backwards to the end of the archive */
	end = *logEnd;

    /* parse -O argument and align if required */
    offset = start;
    if (swOffset) {
	if (__pmParseTime(swOffset, &start, &end, &offset, errMsg) < 0)
	    return -1;

	/* sanity check -O */
	if (tvcmp(offset, start) < 0)
	    offset = start;
	else if (tvcmp(offset, end) > 0)
	    offset = end;

	if (swAlign) {
	    align = offset.tv_usec + 1000000 * (__int64_t)offset.tv_sec;
	    blign = (align / delta) * delta;
	    if (blign < align)
		blign += delta;
	    align = end.tv_usec + 1000000 * (__int64_t)end.tv_sec;
	    if (blign > align)
		blign -= delta;
	    aoffset.tv_sec = (time_t)(blign / 1000000);
	    aoffset.tv_usec = (int)(blign % 1000000);

	    /* sanity check -O after alignment */
	    if (tvcmp(aoffset, start) >= 0 && tvcmp(aoffset, end) <= 0)
		offset = aoffset;
	    else {
		parseError(swAlign, swAlign, alignmsg, errMsg);
		sts = 0;
	    }
	}
    }

    /* return results */
    *rsltStart = start;
    *rsltEnd = end;
    *rsltOffset = offset;
    return sts;
}
