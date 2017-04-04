/*
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "./domain.h"

typedef long long	longlong_t;		// TODO nuke for AIX
typedef unsigned long long	u_longlong_t;	// TODO nuke for AIX build
typedef unsigned char uchar;		// TODO nuke for AIX build

#include <libperfstat.h>

/*
 * libperfstat method controls
 *
 * md_method choices (see below) ... must be contiguous integers so
 * we can index directly into method[]
 */
#define M_CPU_TOTAL	0
#define M_CPU		1
#define M_DISK_TOTAL	2
#define M_DISK		3
#define M_NETIF		4
#define M_NETBUF	5	// TODO
#define M_PROTO		6	// TODO
#define M_MEM_TOTAL	7	// TODO

/*
 * special values for offset
 */
#define OFF_NOVALUES	-2
#define OFF_DERIVED	-1

typedef struct {
    void	(*m_init)(int);
    void	(*m_prefetch)(void);
    int		(*m_fetch)(pmdaMetric *, int, pmAtomValue *);
} method_t;

extern method_t		methodtab[];
extern int		methodtab_sz;

extern void init_data(int);

extern void cpu_total_init(int);
extern void cpu_total_prefetch(void);
extern int cpu_total_fetch(pmdaMetric *, int, pmAtomValue *);

extern void cpu_init(int);
extern void cpu_prefetch(void);
extern int cpu_fetch(pmdaMetric *, int, pmAtomValue *);

extern void disk_total_init(int);
extern void disk_total_prefetch(void);
extern int disk_total_fetch(pmdaMetric *, int, pmAtomValue *);

extern void disk_init(int);
extern void disk_prefetch(void);
extern int disk_fetch(pmdaMetric *, int, pmAtomValue *);

extern void netif_init(int);
extern void netif_prefetch(void);
extern int netif_fetch(pmdaMetric *, int, pmAtomValue *);

/*
 * metric descriptions
 */
typedef struct {
    pmDesc	md_desc;	// PMDA's idea of the semantics
    int		md_method;	// specific stats method
    int		md_offset;	// offset into stats structure
} metricdesc_t;

extern metricdesc_t	metricdesc[];
extern pmdaMetric	*metrictab;
extern int		metrictab_sz;

#define DISK_INDOM	0
#define CPU_INDOM	1
#define NETIF_INDOM	2

extern pmdaIndom	indomtab[];
extern int		indomtab_sz;
