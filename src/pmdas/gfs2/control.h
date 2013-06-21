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
        CONTROL_GLOCK_LOCK_TIME = 0,
        NUM_CONTROL_STATS
};

extern const char *control_locations[];

extern int gfs2_control_fetch(int);
extern int gfs2_control_set_value(const char *, pmValueSet *);
extern int gfs2_control_check_value(const char *);

#endif /* CONTROL_H */
