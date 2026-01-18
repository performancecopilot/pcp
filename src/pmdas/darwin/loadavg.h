/*
 * Darwin PMDA loadavg cluster
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

#ifndef LOADAVG_H
#define LOADAVG_H

/*
 * Refresh load average data from kernel
 */
extern int refresh_loadavg(float *);

/*
 * Fetch values for load average metrics
 */
extern int fetch_loadavg(unsigned int, unsigned int, pmAtomValue *);

#endif /* LOADAVG_H */
