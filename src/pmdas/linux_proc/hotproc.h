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

/* Global items */
#define ITEM_HOTPROC_G_REFRESH 1
#define ITEM_HOTPROC_G_CPUIDLE 2
#define ITEM_HOTPROC_G_CPUBURN 3
#define ITEM_HOTPROC_G_OTHER_TRANSIENT 4
#define ITEM_HOTPROC_G_OTHER_NOT_CPUBURN 5
#define ITEM_HOTPROC_G_OTHER_TOTAL 6
#define ITEM_HOTPROC_G_OTHER_PERCENT 7
#define ITEM_HOTPROC_G_CONFIG 8
#define ITEM_HOTPROC_G_CONFIG_GEN 9
#define ITEM_HOTPROC_G_RELOAD_CONFIG 10

/* Predicate items */
#define ITEM_HOTPROC_P_SYSCALLS 0
#define ITEM_HOTPROC_P_CTXSWITCH 1
#define ITEM_HOTPROC_P_VSIZE 2
#define ITEM_HOTPROC_P_RSIZE 3
#define ITEM_HOTPROC_P_IODEMAND 4
#define ITEM_HOTPROC_P_IOWAIT 5
#define ITEM_HOTPROC_P_SCHEDWAIT 6
#define ITEM_HOTPROC_P_CPUBURN 7

/* main process node type */
typedef struct process_t {
    pid_t pid;

    /* refreshed data */
    unsigned long r_vctx;
    unsigned long r_ictx;
    //ulong_t r_syscalls;
    unsigned long long r_bread;
    unsigned long long r_bwrit;

    float  r_cpuburn;
    double r_cputimestamp;
    double r_cputime;

    unsigned long long r_bwtime;
    unsigned long long r_qwtime;

    /* predicate values */
    derived_pred_t preds;

} process_t;

void hotproc_init();

#endif
