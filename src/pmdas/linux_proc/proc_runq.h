/*
 * Copyright (c) 2014 Red Hat.
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

extern int proc_runq_append(const char *, proc_runq_t *);
extern int proc_runq_append_pid(int, proc_runq_t *);

#endif /* _PROC_RUNQ_H */
