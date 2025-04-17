/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2021-2022 Red Hat.
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

double
pmtimespecAdd(const struct timespec *ap, const struct timespec *bp)
{
     return (double)(ap->tv_sec + bp->tv_sec) + (long double)(ap->tv_nsec + bp->tv_nsec) / (long double)1000000000;
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

void
pmtimespecInc(struct timespec *ap, const struct timespec *bp)
{
     ap->tv_sec += bp->tv_sec;
     ap->tv_nsec += bp->tv_nsec;
     if (ap->tv_nsec >= 1000000000) {
	ap->tv_nsec -= 1000000000;
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

double
pmtimespecSub(const struct timespec *ap, const struct timespec *bp)
{
     return (double)(ap->tv_sec - bp->tv_sec) + (long double)(ap->tv_nsec - bp->tv_nsec) / (long double)1000000000;
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

void
pmtimespecDec(struct timespec *ap, const struct timespec *bp)
{
     ap->tv_sec -= bp->tv_sec;
     ap->tv_nsec -= bp->tv_nsec;
     if (ap->tv_nsec < 0) {
	ap->tv_nsec += 1000000000;
	ap->tv_sec--;
    }
}

/*
 * convert a time structure to a double (units = seconds)
 */
double
pmtimespecToReal(const struct timespec *val)
{
    return val->tv_sec + ((long double)val->tv_nsec / (long double)1000000000);
}

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

void
pmtimespecFromReal(double secs, struct timespec *val)
{
    val->tv_sec = (time_t)secs;
    val->tv_nsec = (long)((long double)(secs - val->tv_sec) * (long double)1000000000 + (long double)0.5);
}

/*
 * Sleep for a specified amount of time
 */
void
__pmtimespecSleep(struct timespec delay)
{
    struct timespec left;

    for (;;) {		/* loop to catch early wakeup by nanosleep */
	int sts = nanosleep(&delay, &left);
	if (sts == 0 || (sts < 0 && oserror() != EINTR))
	    break;
	delay = left;
    }
}

void
__pmtimevalSleep(struct timeval delay)
{
    struct timespec	delta;	/* higher resolution delay */

    delta.tv_sec = delay.tv_sec;
    delta.tv_nsec = delay.tv_usec * 1000;

    __pmtimespecSleep(delta);
}

#if !defined(IS_MINGW)
void
pmtimevalNow(struct timeval *tv)
{
    struct timespec	tspec;
    clock_gettime(CLOCK_REALTIME, &tspec);
    tv->tv_sec = tspec.tv_sec;
    tv->tv_usec = tspec.tv_nsec / 1000;
}
#endif

/* sleep until given timeval */
void
__pmtimespecPause(struct timespec sched)
{
    struct timespec curr;	/* current time */
    struct timespec delay;	/* interval to sleep */
    struct timespec left;	/* remaining sleep time */

    pmtimespecNow(&curr);
    pmtimespecDec(&sched, &curr);
    delay = sched;
    for (;;) {		/* loop to catch early wakeup by nanosleep */
	int sts = nanosleep(&delay, &left);
	if (sts == 0 || (sts < 0 && oserror() != EINTR))
	    break;
	delay = left;
    }
}

void
__pmtimevalPause(struct timeval sched)
{
    struct timespec until;	/* higher resolution sched */

    until.tv_sec = sched.tv_sec;
    until.tv_nsec = sched.tv_usec * 1000;

    __pmtimespecPause(until);
}
