/*
 * Linux /proc/buddyinfo metrics cluster
 *
 * Copyright (c) 2016 Fujitsu.
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
 * All fields in /proc/buddyinfo
 */
typedef struct {
    int		id;
    char	id_name[128];
    char	node_name[64];
    char	zone_name[64];
    int		value;
} buddyinfo_t;

typedef struct {
    int		nbuddys;
    buddyinfo_t	*buddys;
    pmdaIndom	*indom;
} proc_buddyinfo_t;

extern int refresh_proc_buddyinfo(proc_buddyinfo_t *);
