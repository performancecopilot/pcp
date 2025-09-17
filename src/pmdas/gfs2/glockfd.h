/*
 * GFS2 glockfd file statistics.
 *
 * Copyright (c) 2013-2025 Red Hat.
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

#ifndef GLOCKFD_H
#define GLOCKFD_H

#define MAX_NUMBER_OF_HOLDERS 10000

enum {
    GLOCKFD_TOTAL = 0,
    NUM_GLOCKFD_STATS
};

enum {
    GLOCKFD_PROCESS = 0,
    GLOCKFD_FILE_DESCRIPTOR,
    NUM_PER_HOLDER_STATS
};

struct glockfd {
    __uint64_t total;
};

struct glockfd_per_instance {
    __uint64_t holder_id;
    __int32_t pids;
    __int32_t fds;
};

extern int gfs2_glockfd_fetch(int, struct glockfd *, pmAtomValue *);
extern int gfs2_refresh_glockfd(const char *, const char *, struct glockfd *);

extern int gfs2_per_holder_fetch(int, unsigned int, pmAtomValue *);
extern int gfs2_refresh_per_holder(void);

#endif /* GLOCKFD_H */
