/*
 * GFS2 trace-point enable controls.
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

#ifndef CONTROL_H
#define CONTROL_H

enum {
    CONTROL_ALL = 0,
    CONTROL_GLOCK_STATE_CHANGE,
    CONTROL_GLOCK_PUT,
    CONTROL_DEMOTE_RQ,
    CONTROL_PROMOTE,
    CONTROL_GLOCK_QUEUE,
    CONTROL_GLOCK_LOCK_TIME,
    CONTROL_PIN,
    CONTROL_LOG_FLUSH,
    CONTROL_LOG_BLOCKS,
    CONTROL_AIL_FLUSH,
    CONTROL_BLOCK_ALLOC,
    CONTROL_BMAP,
    CONTROL_RS,
    CONTROL_GLOBAL_TRACING,
    CONTROL_WORSTGLOCK,
    CONTROL_FTRACE_GLOCK_THRESHOLD,
    NUM_CONTROL_STATS
};

extern const char *control_locations[];

extern int gfs2_control_fetch(int, pmAtomValue *);
extern int gfs2_control_set_value(const char *, pmValueSet *);
extern int gfs2_control_check_value(const char *);

#endif /* CONTROL_H */
