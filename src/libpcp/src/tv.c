/*
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

#include "pmapi.h"
#include "libpcp.h"
#include <sys/time.h>

/*
 * real additive time, *ap plus *bp
 */
double
pmtimevalAdd(const struct timeval *ap, const struct timeval *bp)
{
     return (double)(ap->tv_sec + bp->tv_sec) + (long double)(ap->tv_usec + bp->tv_usec) / (long double)1000000;
}

/*
 * struct additive time, *ap = *ap + *bp
 */
void
pmtimevalInc(struct timeval *ap, const struct timeval *bp)
{
     ap->tv_sec += bp->tv_sec;
     ap->tv_usec += bp->tv_usec;
     if (ap->tv_usec >= 1000000) {
	ap->tv_usec -= 1000000;
	ap->tv_sec++;
    }
}

/*
 * real time difference, *ap minus *bp
 */
double
pmtimevalSub(const struct timeval *ap, const struct timeval *bp)
{
     return (double)(ap->tv_sec - bp->tv_sec) + (long double)(ap->tv_usec - bp->tv_usec) / (long double)1000000;
}

/*
 * struct subtractive time, *ap = *ap - *bp
 */
void
pmtimevalDec(struct timeval *ap, const struct timeval *bp)
{
     ap->tv_sec -= bp->tv_sec;
     ap->tv_usec -= bp->tv_usec;
     if (ap->tv_usec < 0) {
	ap->tv_usec += 1000000;
	ap->tv_sec--;
    }
}

/*
 * convert a timeval to a double (units = seconds)
 */
double
pmtimevalToReal(const struct timeval *val)
{
    return val->tv_sec + ((long double)val->tv_usec / (long double)1000000);
}

/*
 * convert double (units == seconds) to a timeval
 */
void
pmtimevalFromReal(double secs, struct timeval *val)
{
    val->tv_sec = (time_t)secs;
    val->tv_usec = (long)((long double)(secs - val->tv_sec) * (long double)1000000 + (long double)0.5);
}

/*
 * Sleep for a specified amount of time
 */
void
__pmtimevalSleep(struct timeval interval)
{
    struct timespec delay;
    struct timespec left;
    int sts;

    delay.tv_sec = interval.tv_sec;
    delay.tv_nsec = interval.tv_usec * 1000;

    for (;;) {		/* loop to catch early wakeup by nanosleep */
	sts = nanosleep(&delay, &left);
	if (sts == 0 || (sts < 0 && oserror() != EINTR))
	    break;
	delay = left;
    }
}

/* convert timeval to timespec */
static void
tospec(struct timeval *tv, struct timespec *ts)
{
    ts->tv_nsec = tv->tv_usec * 1000;
    ts->tv_sec = tv->tv_sec;
}

#if !defined(IS_MINGW)
void
pmtimevalNow(struct timeval *tv)
{
    gettimeofday(tv, NULL);
}
#endif

/* sleep until given timeval */
void
__pmtimevalPause(struct timeval sched)
{
    int sts;
    struct timeval curr;	/* current time */
    struct timespec delay;	/* interval to sleep */
    struct timespec left;	/* remaining sleep time */

    pmtimevalNow(&curr);
    pmtimevalDec(&sched, &curr);
    tospec(&sched, &delay);
    for (;;) {		/* loop to catch early wakeup by nanosleep */
	sts = nanosleep(&delay, &left);
	if (sts == 0 || (sts < 0 && oserror() != EINTR))
	    break;
	delay = left;
    }
}
