/*
 * GFS2 glock file statistics.
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

#ifndef GLOCKS_H
#define GLOCKS_H

enum {
	GLOCKS_TOTAL = 0,
	GLOCKS_SHARED = 1,
	GLOCKS_UNLOCKED = 2,
	GLOCKS_DEFERRED = 3,
	GLOCKS_EXCLUSIVE = 4,
	NUM_GLOCKS_STATS 
};

struct glocks {
    __uint64_t	values[NUM_GLOCKS_STATS];
};

extern int gfs2_glocks_fetch(int, struct glocks *, pmAtomValue *);
extern int gfs2_refresh_glocks(const char *, const char *, struct glocks *);

#endif	/*GLOCKS_H*/
