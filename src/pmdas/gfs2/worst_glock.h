/*
 * GFS2 gfs2_glock_lock_time trace-point metrics.
 *
 * Copyright (c) 2013 - 2014 Red Hat.
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
#define WORST_GLOCK_TOP 10
#define WORST_GLOCK_COUNT (NUM_GLOCKSTATS*NUM_TOPNUM)

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
    WORSTGLOCK_QUEUE,
    NUM_GLOCKSTATS
};

enum {
    WORSTGLOCK_TRANS = 1,
    WORSTGLOCK_INODE = 2,
    WORSTGLOCK_RGRP = 3,
    WORSTGLOCK_META = 4,
    WORSTGLOCK_IOPEN = 5,
    WORSTGLOCK_FLOCK = 6,
    WORSTGLOCK_RESERVED = 7,
    WORSTGLOCK_QUOTA = 8,
    WORSTGLOCK_JOURNAL = 9
};

enum {
    TOPNUM_FIRST = 0,
    TOPNUM_SECOND,
    TOPNUM_THIRD,
    TOPNUM_FOURTH,
    TOPNUM_FIFTH,
    TOPNUM_SIXTH,
    TOPNUM_SEVENTH,
    TOPNUM_EIGHTH,
    TOPNUM_NINTH,
    TOPNUM_TENTH,
    NUM_TOPNUM
};

struct glock {
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

struct worst_glock {
    struct glock glocks[WORST_GLOCK_TOP + 1];
    int    assigned_entries;
};

extern void gfs2_worst_glock_init(pmdaMetric *, int);
extern int gfs2_worst_glock_fetch(int, struct worst_glock *, pmAtomValue *);
extern int gfs2_extract_worst_glock(char **, pmInDom);

extern int worst_glock_get_state();
extern int worst_glock_set_state(pmValueSet *vsp);

#endif /* LOCK_TIME_H */
