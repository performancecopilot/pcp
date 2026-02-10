/*
 * Darwin PMDA cpuload cluster
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

#ifndef CPULOAD_H
#define CPULOAD_H

#include <mach/host_info.h>

/*
 * Refresh CPU load information from kernel
 */
extern int refresh_cpuload(struct host_cpu_load_info *);

/*
 * Fetch values for CPU load metrics
 */
extern int fetch_cpuload(unsigned int, pmAtomValue *);

#endif /* CPULOAD_H */
