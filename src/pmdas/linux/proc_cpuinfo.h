/*
 * Linux /proc/cpuinfo metrics cluster
 *
 * Copyright (c) 2001 Gilly Ran (gilly@exanet.com) for the
 * portions of the code supporting the Alpha platform.
 * All rights reserved.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef  __alpha__
#define HAVE_ALPHA_LINUX
#endif

typedef struct {
    int			cpu_num;
    char		*sapic;
    char		*name;
    char		cpu_char;
    int			node;
    float		clock;
    char		*vendor;
    char		*model;
    char		*model_name;
    char		*stepping;
    unsigned int	cache;
    float		bogomips;
} cpuinfo_t;

typedef struct {
    char		*machine;
    cpuinfo_t		*cpuinfo;
    pmdaIndom		*cpuindom;
    pmdaIndom		*node_indom;
} proc_cpuinfo_t;

extern int refresh_proc_cpuinfo(proc_cpuinfo_t *);
extern char *cpu_name(proc_cpuinfo_t *, int);
