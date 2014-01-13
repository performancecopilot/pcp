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

#define COUNT_THRESHOLD 350
#define DEFAULT_WORST_GLOCK_STATE 1

enum {
    WORSTGLOCK_LOCK_TYPE = 0,
    WORSTGLOCK_NUMBER,
    WORSTGLOCK_SRTT,
    WORSTGLOCK_SRTTVAR,
    WORSTGLOCK_SRTTB,
    WORSTGLOCK_SRTTVARB,
    WORSTGLOCK_SIRT,
    WORSTGLOCK_SIRTVAR,
    WORSTGLOCK_DLM,
    WORSTGLOCK_QUEUE
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
    LOCKTIME_JOURNAL = 9
};

struct worst_glock {
    dev_t dev_id; 
    uint32_t lock_type;    /* Glock type number */
    uint64_t number;       /* Inode or resource group number */
    int64_t srtt;          /* Non blocking smoothed round trip time */
    int64_t srttvar;       /* Non blocking smoothed variance */
    int64_t srttb;         /* Blocking smoothed round trip time */
    int64_t srttvarb;      /* Blocking smoothed variance */
    int64_t sirt;          /* Smoothed Inter-request time */
    int64_t sirtvar;       /* Smoothed Inter-request variance */
    int64_t dlm;           /* Count of dlm requests */
    int64_t queue;         /* Count of gfs2_holder queues */
};

extern int gfs2_worst_glock_fetch(int, struct worst_glock *, pmAtomValue *);
extern int gfs2_extract_worst_glock(char *);
extern void worst_glock_assign_glocks(pmInDom);

extern int worst_glock_get_state();
extern int worst_glock_set_state(pmValueSet *vsp);

#endif /* LOCK_TIME_H */
