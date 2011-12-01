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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef PPRED_H
#define PPRED_H

#define CLUSTER_PRED      101

#define ITEM_SYSCALLS     0
#define ITEM_CTXSWITCH    1
#define ITEM_VIRTUALSIZE  2
#define ITEM_RESIDENTSIZE 3
#define ITEM_IODEMAND     4
#define ITEM_IOWAIT       5
#define ITEM_SCHEDWAIT    6

void ppred_init(int dom);
int ppred_getdesc(pmID pmid, pmDesc *desc);
int ppred_setatom(int item, pmAtomValue *atom, int j);
int ppred_getinfo(pid_t pid, int j);
int ppred_allocbuf(int size);
int ppred_available(int item);

#endif
