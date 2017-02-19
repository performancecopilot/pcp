/*
 * Copyright (C) 2013  Joe White
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef ARCHITECTURE_H_
#define ARCHITECTURE_H_

#include <stddef.h>

typedef struct cpulist_t_ {
    size_t count;
    int *index;
} cpulist_t;

typedef struct archinfo_t_ {
    /* List of all CPUs in system */
    cpulist_t cpus;

    /* Total number of numa nodes in system */
    size_t nnodes;

    /* array of cpu lists one for each numa node */
    cpulist_t *nodes;

    /* max number of cpus per node */
    size_t ncpus_per_node;

    /* array of lists of the nth cpus in each node */
    /* cpunodes[0] will contain a list of the first cpu in each node */
    /* cpunodes[n] will contain a list of the (n+1) cpu in each node */
    cpulist_t *cpunodes;
} archinfo_t;

archinfo_t *get_architecture();

void free_architecture(archinfo_t *);

int parse_delimited_list(const char *line, int *output);

#endif /* ARCHITECTURE_H_ */
