/*
 *
 * Copyright (c) 2012 Ken McDonell  All Rights Reserved.
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

/*
 * instance domains control
 */
#define LOADAV_INDOM	0
#define CPU_INDOM	1
#define DISK_INDOM	2
#define NETIF_INDOM	3
extern pmdaIndom indomtab[];

extern void	refresh_disk_metrics(void);
extern int	do_disk_metrics(pmdaMetric *, unsigned int, pmAtomValue *);

extern void	refresh_netif_metrics(void);
extern int	do_netif_metrics(pmdaMetric *, unsigned int, pmAtomValue *);

/*
 * kernel memory reader pieces
 */
#include <kvm.h>
#include <nlist.h>

extern kvm_t	*kvmp;

#define KERN_IFNET	0
extern struct nlist	symbols[];

/* for uname(3) */
#include <sys/utsname.h>
