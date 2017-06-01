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
#ifndef PCP_CONFIG64_H
#define PCP_CONFIG64_H

#define HAVE_64BIT_LONG 1
/* #undef HAVE_32BIT_LONG */
/* #undef HAVE_32BIT_PTR */
#define HAVE_64BIT_PTR 1

#define PM_SIZEOF_SUSECONDS_T 8
#define PM_SIZEOF_TIME_T 8

#endif /* PCP_CONFIG64_H */
