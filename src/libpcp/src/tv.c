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
