/*
 * Copyright (c) 2014,2016 Red Hat.
 * Headers for "multilib" support (32-bit and 64-bit packages co-existing)
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
#ifndef PCP_CONFIG32_H
#define PCP_CONFIG32_H

/* #undef HAVE_64BIT_LONG */
#define HAVE_32BIT_LONG 1
#define HAVE_32BIT_PTR 1
/* #undef HAVE_64BIT_PTR */

#define PM_SIZEOF_SUSECONDS_T 4
#define PM_SIZEOF_TIME_T 4

#endif /* PCP_CONFIG32_H */
