/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2007, Nathan Scott.  All Rights Reserved.
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
 * Contact information: Ken McDonell, kenj At internode DoT on DoT net
 *                      Nathan Scott, nathans At debian DoT org
 */
#ifndef MAIN_H
#define MAIN_H

#define KMTIME_PORT_BASE	43334

#define KM_FASTMODE_DELAY	200	/* milliseconds */
#define KM_DEFAULT_DELTA	2	/* seconds */
#define KM_DEFAULT_SPEED(d)	(2.0 * (d))	/* num deltas per second */
#define KM_MINIMUM_SPEED(d)	(0.1 * (d))	/* min deltas per second */
#define KM_MAXIMUM_SPEED(d)	(100.0 * (d))	/* max deltas per second */

typedef enum { Msec, Sec, Min, Hour, Day, Week } delta_units;
typedef enum { DBG_APP = 0x1, DBG_PROTO = 0x2 } debug_options;

extern void tadd(struct timeval *a, struct timeval *b);
extern void tsub(struct timeval *a, struct timeval *b);
extern int tnonzero(struct timeval *a);
extern int tcmp(struct timeval *a, struct timeval *b);

extern void secondsToTV(double value, struct timeval *tv);
extern double secondsFromTV(struct timeval *tv);

extern double unitsToSeconds(double value, delta_units units);
extern double secondsToUnits(double value, delta_units units);

#endif	/* MAIN_H */
