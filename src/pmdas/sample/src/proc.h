/*
 * Copyright (c) 2021 Ken McDonell.  All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

extern void proc_reset(pmdaIndom *);
extern int proc_redo_indom(pmdaIndom *);
extern int proc_get_ordinal(int);
extern char *proc_get_exec(int);
extern __uint64_t proc_get_time(int);
