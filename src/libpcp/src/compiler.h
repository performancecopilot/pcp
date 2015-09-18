/*
 * Copyright (c) 2012-2015 Red Hat.
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
#ifndef _LIBPCP_COMPILER_H
#define _LIBPCP_COMPILER_H

#if defined(__GNUC__) && (__GNUC__ >= 4) && !defined(IS_MINGW)
# define _PCP_HIDDEN __attribute__ ((visibility ("hidden")))
#else
# define _PCP_HIDDEN
#endif

#if defined(__GNUC__)
# define likely(x)	__builtin_expect(!!(x),1)
# define unlikely(x)	__builtin_expect(!!(x),0)
#else
# define likely(x) (x)
# define unlikely(x) (x)
#endif

#endif /* _LIBPCP_COMPILER_H */
