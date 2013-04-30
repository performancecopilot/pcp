/*
 * GFS2 glstats file statistics.
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

#ifndef GLSTATS_H
#define GLSTATS_H

/*
 * GFS2 glock type identification; note that the glock type 7 is currently not 
 * used and is reserved to be on the safe side.
 *
 * Type Lock type  Use
 * 1    Trans      Transaction lock
 * 2    Inode      Inode metadata and data
 * 3    Rgrp       Resource group metadata
 * 4    Meta       The superblock
 * 5    Iopen      Inode last closer detection
 * 6    Flock      flock(2) syscall
 * 8    Quota      Quota operations
 * 9    Journal    Journal mutex 
 *
 */

enum {
    GLSTATS_TOTAL = 0,
    GLSTATS_TRANS = 1,
    GLSTATS_INODE = 2,
    GLSTATS_RGRP = 3,
    GLSTATS_META = 4,
    GLSTATS_IOPEN = 5,
    GLSTATS_FLOCK = 6,
    GLSTATS_RESERVED_NOT_USED = 7,
    GLSTATS_QUOTA = 8,
    GLSTATS_JOURNAL = 9,
    NUM_GLSTATS_STATS
};

struct glstats {
    __uint64_t values[NUM_GLSTATS_STATS];
};

extern int gfs2_glstats_fetch(int, struct glstats *, pmAtomValue *);
extern int gfs2_refresh_glstats(const char *, const char *, struct glstats *);

#endif /* GLSTATS_H */
