/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifndef _INTERNAL_H
#define _INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * routines used across libpcp source files, but not exposed in impl.h
 */
extern int __pmFetchLocal(__pmContext *, int, pmID *, pmResult **);

#ifdef __cplusplus
}
#endif

#endif /* _INTERNAL_H */
