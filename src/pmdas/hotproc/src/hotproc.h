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

#ifndef PHOTPROC_H
#define PHOTPROC_H

#include "config.h"

/* main process node type */
typedef struct process_t {
    pid_t pid;

    /* refreshed data */
    ulong_t r_vctx;
    ulong_t r_ictx;
    ulong_t r_syscalls;
    ulong_t r_bread;
    ulong_t r_gbread;
    ulong_t r_bwrit;
    ulong_t r_gbwrit;

    float  r_cpuburn;
    double r_cputimestamp;
    double r_cputime;

    accum_t r_bwtime;
    accum_t r_rwtime;
    accum_t r_qwtime;

    /* predicate values */
    derived_pred_t preds;

} process_t;

process_t * lookup_curr_node(pid_t);

#endif
