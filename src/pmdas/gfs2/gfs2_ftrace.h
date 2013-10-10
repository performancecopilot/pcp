/*
 * GFS2 gfs2_glock_lock_time trace-point metrics.
 *
 * Copyright (c) 2013 Red Hat.
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

#ifndef GFS2_FTRACE_H
#define GFS2_FTRACE_H

enum {
    GLOCK_LOCK_TIME = 0,
    NUM_FTRACE_ARRAYS
};

enum {
    FALSE = 0,
    TRUE = 1
};

extern void gfs2_ftrace_increase_num_accepted_locks();
extern int gfs2_refresh_ftrace_stats(pmInDom);

extern int gfs2_ftrace_get_threshold();
extern int gfs2_ftrace_set_threshold(pmValueSet *vsp);

#endif	/*GFS2_FTRACE_H*/
