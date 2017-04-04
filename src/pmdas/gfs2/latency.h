/*
 * GFS2 latency metrics.
 *
 * Copyright (c) 2014 Red Hat.
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

#ifndef LATENCY_H
#define LATENCY_H

#define DEFAULT_LATENCY_STATE 1
#define NUM_LATENCY_VALUES 512

enum{
    START = 0,
    END
};

enum {
    LATENCY_GRANT_ALL = 0,
    LATENCY_GRANT_NL,
    LATENCY_GRANT_CR,
    LATENCY_GRANT_CW,
    LATENCY_GRANT_PR,
    LATENCY_GRANT_PW,
    LATENCY_GRANT_EX,
    LATENCY_DEMOTE_ALL,
    LATENCY_DEMOTE_NL,
    LATENCY_DEMOTE_CR,
    LATENCY_DEMOTE_CW,
    LATENCY_DEMOTE_PR,
    LATENCY_DEMOTE_PW,
    LATENCY_DEMOTE_EX,
    LATENCY_QUEUE_ALL,
    LATENCY_QUEUE_NL,
    LATENCY_QUEUE_CR,
    LATENCY_QUEUE_CW,
    LATENCY_QUEUE_PR,
    LATENCY_QUEUE_PW,
    LATENCY_QUEUE_EX,
    NUM_LATENCY_STATS
};

struct latency_data {
    uint32_t lock_type;
    uint64_t number;
    int64_t usecs; 
};

struct latency {
    struct latency_data values  [NUM_LATENCY_STATS * NUM_LATENCY_VALUES * 2]; /* START and STOP values */
    int                 counter [NUM_LATENCY_STATS];
};

extern int gfs2_latency_fetch(int, struct latency *, pmAtomValue *);
extern int gfs2_extract_latency(unsigned int, unsigned int, int, char *, pmInDom);

extern int latency_get_state();
extern int latency_set_state(pmValueSet *vsp);

#endif /* LATENCY_H */
