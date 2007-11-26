/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ifndef PGLOBAL_H
#define PGLOBAL_H

#define CLUSTER_GLOBAL 100

#define ITEM_NPROCS           0
#define ITEM_REFRESH          1
#define ITEM_CPUIDLE          2
#define ITEM_TOTAL_CPUBURN    3
#define ITEM_TRANSIENT        4
#define ITEM_TOTAL_NOTCPUBURN 5
#define ITEM_OTHER_TOTAL      6
#define ITEM_OTHER_PERCENT    7
#define ITEM_CONFIG           8
#define ITEM_CONFIG_GEN	      9

void pglobal_init(int dom);
int pglobal_getdesc(pmID pmid, pmDesc *desc);
int pglobal_setatom(int item, pmAtomValue *atom, int j);
int pglobal_getinfo(pid_t pid, int j);
int pglobal_allocbuf(int size);

#endif
