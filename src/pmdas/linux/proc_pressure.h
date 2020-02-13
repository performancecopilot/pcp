/*
 * Linux /proc/pressure/ metrics clusters
 *
 * Copyright (c) 2019 Red Hat.
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
 * All fields from the /proc/pressure files
 */
typedef struct {
    int		updated;
    float	avg[3];		/* 10, 60, 300 second averages */
    __uint64_t	total;
} pressure_t;

typedef struct {
    pressure_t	some_cpu;
    pressure_t	full_mem;
    pressure_t	some_mem;
    pressure_t	full_io;
    pressure_t	some_io;
} proc_pressure_t;

extern int average_proc_pressure(pressure_t *, unsigned int, pmAtomValue *);

extern int refresh_proc_pressure_cpu(proc_pressure_t *);
extern int refresh_proc_pressure_mem(proc_pressure_t *);
extern int refresh_proc_pressure_io(proc_pressure_t *);
