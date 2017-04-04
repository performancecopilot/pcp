/*
 * Copyright (c) International Business Machines Corp., 2002
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

/*
 * This code contributed by Mike Mason <mmlnx@us.ibm.com>
 */

typedef struct {
	unsigned long uptime;
	unsigned long idletime;
} proc_uptime_t;

extern int refresh_proc_uptime(proc_uptime_t *);

