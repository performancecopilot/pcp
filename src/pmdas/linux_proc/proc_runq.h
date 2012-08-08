/*
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

#ifndef _PROC_RUNQ_H
#define _PROC_RUNQ_H

typedef struct {
    int runnable;
    int blocked;
    int sleeping;
    int stopped;
    int swapped;
    int kernel;
    int defunct;
    int unknown;
} proc_runq_t;

extern int refresh_proc_runq(proc_runq_t *);

#endif /* _PROC_RUNQ_H */
