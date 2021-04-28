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
 */
#ifndef _HOTPROC_H
#define _HOTPROC_H

#include "config.h"

/* main process node type */
typedef struct process_t {
    pid_t		pid;

    /* refreshed data */
    unsigned long	r_vctx;
    unsigned long	r_ictx;
    unsigned long long	r_bread;
    unsigned long long	r_bwrit;

    float		r_cpuburn;
    double		r_cputimestamp;
    double		r_cputime;

    unsigned long long	r_bwtime;
    unsigned long long	r_qwtime;

    /* predicate values */
    derived_pred_t	preds;
} process_t;

extern void hotproc_init();

extern struct timeval hotproc_update_interval;

#endif
