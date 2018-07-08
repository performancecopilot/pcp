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
	GLOCKS_FLAGS_LOCKED = 5,
	GLOCKS_FLAGS_DEMOTE = 6,
	GLOCKS_FLAGS_DEMOTE_PENDING = 7,
	GLOCKS_FLAGS_DEMOTE_PROGRESS = 8,
	GLOCKS_FLAGS_DIRTY = 9,
	GLOCKS_FLAGS_LOG_FLUSH = 10,
	GLOCKS_FLAGS_INVALIDATE = 11,
	GLOCKS_FLAGS_REPLY_PENDING = 12,
	GLOCKS_FLAGS_INITIAL = 13,
	GLOCKS_FLAGS_FROZEN =14,
	GLOCKS_FLAGS_QUEUED = 15,
	GLOCKS_FLAGS_OBJECT_ATTACHED = 16,
	GLOCKS_FLAGS_BLOCKING_REQUEST = 17,
	GLOCKS_FLAGS_LRU = 18,
	HOLDERS_TOTAL = 19,
	HOLDERS_SHARED = 20,
	HOLDERS_UNLOCKED = 21,
	HOLDERS_DEFERRED = 22,
	HOLDERS_EXCLUSIVE = 23,
	HOLDERS_FLAGS_ASYNC = 24,
	HOLDERS_FLAGS_ANY = 25,
	HOLDERS_FLAGS_NO_CACHE = 26,
	HOLDERS_FLAGS_NO_EXPIRE = 27,
	HOLDERS_FLAGS_EXACT = 28,
	HOLDERS_FLAGS_FIRST = 29,
	HOLDERS_FLAGS_HOLDER = 30,
	HOLDERS_FLAGS_PRIORITY = 31,
	HOLDERS_FLAGS_TRY = 32,
	HOLDERS_FLAGS_TRY_1CB = 33,
	HOLDERS_FLAGS_WAIT = 34,
	NUM_GLOCKS_STATS 
};

struct glocks {
    __uint64_t	values[NUM_GLOCKS_STATS];
};

extern int gfs2_glocks_fetch(int, struct glocks *, pmAtomValue *);
extern int gfs2_refresh_glocks(const char *, const char *, struct glocks *);

#endif	/*GLOCKS_H*/
