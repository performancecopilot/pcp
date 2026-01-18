/*
 * Darwin PMDA uname cluster
 *
 * Copyright (c) 2025 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#ifndef UNAME_H
#define UNAME_H

#include <sys/utsname.h>

/*
 * Refresh uname information from kernel
 */
extern int refresh_uname(struct utsname *);

/*
 * Fetch values for uname/version metrics
 */
extern int fetch_uname(unsigned int, pmAtomValue *);

#endif /* UNAME_H */
