/*
 * Copyright (c) 2017 Ken McDonell  All Rights Reserved.
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
#ifndef PCP_PMAPI_OLD_H
#define PCP_PMAPI_OLD_H

/*
 * The functions below are deprecated and while still available
 * in libpcp, they may be removed at some point in the future.
 *
 * Deprecated Symbol		Replacement
 * __pmSetProgname()		pmSetProgname()
 * pmProgname			pmGetProgname()
 *
 */

PCP_CALL extern int __pmSetProgname(const char *);
PCP_DATA extern char *pmProgname;

#endif /* PCP_PMAPI_OLD_H */
