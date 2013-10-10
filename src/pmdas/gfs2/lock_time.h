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

#ifndef LOCK_TIME_H
#define LOCK_TIME_H

#include <inttypes.h>

#define COUNT_THRESHOLD 350
#define GLOCK_ARRAY_CAPACITY 2048

enum {
    LOCKTIME_LOCK_TYPE = 0,
    LOCKTIME_NUMBER,
    LOCKTIME_SRTT,
    LOCKTIME_SRTTVAR,
    LOCKTIME_SRTTB,
    LOCKTIME_SRTTVARB,
    LOCKTIME_SIRT,
    LOCKTIME_SIRTVAR,
    LOCKTIME_DLM,
    LOCKTIME_QUEUE
};

enum {
    LOCKTIME_TRANS = 1,
    LOCKTIME_INODE = 2,
    LOCKTIME_RGRP = 3,
    LOCKTIME_META = 4,
    LOCKTIME_IOPEN = 5,
    LOCKTIME_FLOCK = 6,
    LOCKTIME_RESERVED = 7,
    LOCKTIME_QUOTA = 8,
    LOCKTIME_JOURNAL = 9,
};

struct lock_time {
    dev_t dev_id; 
    __uint32_t lock_type;    /* Glock type number */
    __uint64_t number;       /* Inode or resource group number */
    __int64_t srtt;          /* Non blocking smoothed round trip time */
    __int64_t srttvar;       /* Non blocking smoothed variance */
    __int64_t srttb;         /* Blocking smoothed round trip time */
    __int64_t srttvarb;      /* Blocking smoothed variance */
    __int64_t sirt;          /* Smoothed Inter-request time */
    __int64_t sirtvar;       /* Smoothed Inter-request variance */
    __int64_t dlm;           /* Count of dlm requests */
    __int64_t queue;         /* Count of gfs2_holder queues */
};

extern int gfs2_lock_time_fetch(int, struct lock_time *, pmAtomValue *);
extern int gfs2_extract_glock_lock_time(char *);
extern void lock_time_assign_glocks(pmInDom);

#endif /* LOCK_TIME_H */
