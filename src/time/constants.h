/*
 * Copyright (C) 2006 Aconex.  All Rights Reserved.
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
 */
#ifndef CONSTANTS_H
#define CONSTANTS_H

typedef enum { Msec, Sec, Min, Hour, Day, Week } delta_units;
typedef enum { DBG_APP = 0x1, DBG_PROTO = 0x2 } debug_options;

#define KM_FASTMODE_DELAY	200	/* msecs */
#define KM_DEFAULT_DELTA	2	/* secs */
#define KM_DEFAULT_SPEED	1
#define KM_MAXIMUM_SPEED	50

#endif	/* CONSTANTS_H */
