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
    TYPENUMBER_TRANS = 1,
    TYPENUMBER_INODE = 2,
    TYPENUMBER_RGRP = 3,
    TYPENUMBER_META = 4,
    TYPENUMBER_IOPEN = 5,
    TYPENUMBER_FLOCK = 6,
    TYPENUMBER_RESERVED = 7,
    TYPENUMBER_QUOTA = 8,
    TYPENUMBER_JOURNAL = 9,
};

struct lock_time {
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

typedef struct node {
    struct lock_time data;   /* Holding data for our locks*/
    struct node* next;       /* Pointer to the next node in the list */
    dev_t dev_id;            /* Filesystem block device identifer */
} linkedList_t;

extern int gfs2_locktime_fetch(int, struct lock_time *, pmAtomValue *);
extern int gfs2_refresh_lock_time(pmInDom, pmInDom);

void lock_time_assign_glocks(pmInDom, pmInDom);
int lock_compare(struct lock_time *, struct lock_time *);

#endif /* LOCK_TIME_H */
