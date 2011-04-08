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
#include "impl.h"
#include <sys/time.h>

/*
 * real additive time, *ap plus *bp
 */
double
__pmtimevalAdd(const struct timeval *ap, const struct timeval *bp)
{
     return (double)(ap->tv_sec + bp->tv_sec) + (double)(ap->tv_usec + bp->tv_usec)/1000000.0;
}

/*
 * real time difference, *ap minus *bp
 */
double
__pmtimevalSub(const struct timeval *ap, const struct timeval *bp)
{
     return (double)(ap->tv_sec - bp->tv_sec) + (double)(ap->tv_usec - bp->tv_usec)/1000000.0;
}

/*
 * convert a timeval to a double (units = seconds)
 */
double
__pmtimevalToReal(const struct timeval *val)
{
    double dbl = (double)(val->tv_sec);
    dbl += (double)val->tv_usec / 1000000.0;
    return dbl;
}

/*
 * convert double to a timeval
 */
void
__pmtimevalFromReal(double dbl, struct timeval *val)
{
    val->tv_sec = (time_t)dbl;
    val->tv_usec = (long)(((dbl - (double)val->tv_sec) * 1000000.0));
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

/* subtract timevals */
static struct timeval
tsub(struct timeval t1, struct timeval t2)
{
    t1.tv_usec -= t2.tv_usec;
    if (t1.tv_usec < 0) {
	t1.tv_usec += 1000000;
	t1.tv_sec--;
    }
    t1.tv_sec -= t2.tv_sec;
    return t1;
}

/* convert timeval to timespec */
static struct timespec *
tospec(struct timeval tv, struct timespec *ts)
{
    ts->tv_nsec = tv.tv_usec * 1000;
    ts->tv_sec = tv.tv_sec;
    return ts;
}

#if !defined(IS_MINGW)
void
__pmtimevalNow(struct timeval *tv)
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

    __pmtimevalNow(&curr);
    tospec(tsub(sched, curr), &delay);
    for (;;) {		/* loop to catch early wakeup by nanosleep */
	sts = nanosleep(&delay, &left);
	if (sts == 0 || (sts < 0 && oserror() != EINTR))
	    break;
	delay = left;
    }
}
