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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "pmdagfs2.h"

#include "ftrace.h"
#include "worst_glock.h"
#include "latency.h"

#include <string.h>
#include <inttypes.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

static struct ftrace_data latency_data;
static int reset_flag;

static int latency_state = DEFAULT_LATENCY_STATE;

/*
 * Calculates the offset position for the flat array as if it was
 * a three dimentional array containing the latency values.
 */
int 
offset(int x, int y, int z)
{
    return (z * NUM_LATENCY_STATS * NUM_LATENCY_VALUES) + (y * NUM_LATENCY_STATS) + x ; 
}

/*
 * Fetches the value for the given metric item and then assigns to pmAtomValue.
 * We check to see if item is in valid range for the metric.
 */
int
gfs2_latency_fetch(int item, struct latency *latency, pmAtomValue *atom)
{
    int i, counter, position, results_used = 0;
    int64_t result = 0;

    /* We are assigning values so we want to reset on next extract */
    reset_flag = 1;

    /* Ensure that metric value wanted is valid */
    if ((item < 0 || item >= NUM_LATENCY_STATS))
	return PM_ERR_PMID;

    counter = latency->counter[item];

    /* Calculate latency for the metric (deduct start time and add the matching finish time) */
    for (i = 0; i < counter; i++) {
        position = offset(item, i, END);
        result += latency->values[position].usecs;

        position = offset(item, i, START);
        result -= latency->values[position].usecs;

        results_used++;
    }
    /* If we have no values return no values */
    if (results_used == 0)
        return 0;

    /* Return no values if negative result */
    if (result < 0 )
        return 0;

    /* Divide final value by number of counts */
    result /= results_used;

    /* Assign value to the metric */
    atom->ll = result;

    return 1;
}

/*
 * Sets the value of latency using pmstore, value
 * must be 0 or 1.
 */
int 
latency_set_state(pmValueSet *vsp)
{
    int value = vsp->vlist[0].value.lval;

    if (value == 0 || value == 1) {
        latency_state = value;

        return 0;
    } else {
        return PM_ERR_SIGN;
    }
}

/*
 * Used to see whether the latency metrics are enabled or disabled. Should
 * only return either 0 or 1.
 */
int
latency_get_state()
{
    return latency_state;
}

/*
 * Concatenates the ftrace time stamp (major and minor) components into one
 * uint64_t timestamp in usecs.
 *
 */
static int64_t 
concatenate(int64_t a, int64_t b)
{
    unsigned int power = 10;

    while(a >= power)
        power *= 10;

    return a * power + b;        
}

/*
 * Converts lock state to an integer
 */
static int
lock_to_decimal(char *state)
{
    if (strncmp(state, "NL", 2) == 0) {
        return 0;
    } else if (strncmp(state, "CR", 2) == 0) {
        return 1;
    } else if (strncmp(state, "CW", 2) == 0) {
        return 2;
    } else if (strncmp(state, "PR", 2) == 0) {
        return 3;
    } else if (strncmp(state, "PW", 2) == 0) {
        return 4;
    } else if (strncmp(state, "EX", 2) == 0) {
        return 5;
    }

    return 0;
}

/*
 * Updates the records held in the fs->latency structure with new latency
 * stats.
 */
static int
update_records(struct gfs2_fs *fs, int metric, struct latency_data data, int placement)
{
    int i, position, counter;
    struct latency_data blank = { 0 };

    counter = fs->latency.counter[metric];

    /* If we have an intial value */
    if (placement == START) {
        position = offset(metric, counter, START);
        fs->latency.values[position] = data;

        position = offset(metric, counter, END);
        fs->latency.values[position] = blank;

        fs->latency.counter[metric] = (counter + 1) % NUM_LATENCY_VALUES;

    /* If we have a final value */
    } else if (placement == END) {
        for (i = 0; i < counter; i++){
            position = offset(metric, i, START);

            if ((fs->latency.values[position].lock_type == data.lock_type) && 
                (fs->latency.values[position].number == data.number) &&
                (fs->latency.values[position].usecs < data.usecs )) {        

                position = offset(metric, i, END);
                fs->latency.values[position] = data;

                return 0;
            }
        }
    }
    return 0;
}

/*
 * We work out the individual metric values from our buffer input and store
 * them for processing after all of the values have been extracted from the
 * trace pipe.
 */
int
gfs2_extract_latency(unsigned int major, unsigned int minor, int tracepoint, char *data, pmInDom gfs_fs_indom)
{
    latency_data.dev_id = makedev(major, minor);
    latency_data.tracepoint = tracepoint;
    strncpy(latency_data.data, data, sizeof(latency_data.data)-1);

    int i, sts;
    struct gfs2_fs *fs;

    /* We walk through for each filesystem */
    for (pmdaCacheOp(gfs_fs_indom, PMDA_CACHE_WALK_REWIND);;) {
        if ((i = pmdaCacheOp(gfs_fs_indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	sts = pmdaCacheLookup(gfs_fs_indom, i, NULL, (void **)&fs);
	if (sts != PMDA_CACHE_ACTIVE)
	    continue;

        /* Clear old entries if reset is set */
        if (reset_flag == 1) {
            memset(fs->latency.values, 0, sizeof fs->latency.values);
            memset(fs->latency.counter, 0, sizeof fs->latency.counter);
            reset_flag = 0;
        }

        /* Is the entry matching the filesystem we are on? */
        if (fs->dev_id != latency_data.dev_id)
            continue; 

        if (latency_data.tracepoint ==  GLOCK_QUEUE) {

            struct latency_data data;
            int64_t time_major, time_minor;
            char queue[8], state[3];

            sscanf(latency_data.data, 
                "%*s [%*d] %"SCNd64".%"SCNd64": gfs2_glock_queue: %*d,%*d glock %"SCNu32":%"SCNu64" %s %s",
                &time_major, &time_minor, &data.lock_type, &data.number, queue, state
            );
            data.usecs = concatenate(time_major, time_minor);

            if (data.lock_type == WORSTGLOCK_INODE || data.lock_type == WORSTGLOCK_RGRP) {

                /* queue trace data is used both for latency.grant and latency.queue */
                if (strncmp(queue, "queue", 6) == 0) {
                    if (strncmp(state, "NL", 2) == 0) {
                        update_records(fs, LATENCY_GRANT_NL, data, START);
                        update_records(fs, LATENCY_QUEUE_NL, data, START);
                    } else if (strncmp(state, "CR", 2) == 0) {
                        update_records(fs, LATENCY_GRANT_CR, data, START);
                        update_records(fs, LATENCY_QUEUE_CR, data, START);
                    } else if (strncmp(state, "CW", 2) == 0) {
                        update_records(fs, LATENCY_GRANT_CW, data, START);
                        update_records(fs, LATENCY_QUEUE_CW, data, START);
                    } else if (strncmp(state, "PR", 2) == 0) {
                        update_records(fs, LATENCY_GRANT_PR, data, START);
                        update_records(fs, LATENCY_QUEUE_PR, data, START);
                    } else if (strncmp(state, "PW", 2) == 0) {
                        update_records(fs, LATENCY_GRANT_PW, data, START);
                        update_records(fs, LATENCY_QUEUE_PW, data, START);
                    } else if (strncmp(state, "EX", 2) == 0) {
                        update_records(fs, LATENCY_GRANT_EX, data, START);
                        update_records(fs, LATENCY_QUEUE_EX, data, START);
                    }
                    update_records(fs, LATENCY_GRANT_ALL, data, START);
                    update_records(fs, LATENCY_QUEUE_ALL, data, START);

                } else if (strncmp(queue, "dequeue", 8) == 0) {
                    if (strncmp(state, "NL", 2) == 0) {
                        update_records(fs, LATENCY_QUEUE_NL, data, END);
                    } else if (strncmp(state, "CR", 2) == 0) {
                        update_records(fs, LATENCY_QUEUE_CR, data, END);
                    } else if (strncmp(state, "CW", 2) == 0) {
                        update_records(fs, LATENCY_QUEUE_CW, data, END);
                    } else if (strncmp(state, "PR", 2) == 0) {
                        update_records(fs, LATENCY_QUEUE_PR, data, END);
                    } else if (strncmp(state, "PW", 2) == 0) {
                        update_records(fs, LATENCY_QUEUE_PW, data, END);
                    } else if (strncmp(state, "EX", 2) == 0) {
                        update_records(fs, LATENCY_QUEUE_EX, data, END);
                    }
                    update_records(fs, LATENCY_QUEUE_ALL, data, END);
                }
            }
        } else if (latency_data.tracepoint ==  GLOCK_STATE_CHANGE) {

            struct latency_data data;
            int64_t time_major, time_minor;
            char state[3], to[3], target[3];
            int state_decimal, to_decimal;

            sscanf(latency_data.data, 
                "%*s [%*d] %"SCNd64".%"SCNd64": gfs2_glock_state_change: %*d,%*d glock %"SCNu32":%"SCNu64" state %s to %s tgt:%s dmt:%*s flags:%*s", 
                &time_major, &time_minor, &data.lock_type, &data.number, state, to, target
            );
            data.usecs = concatenate(time_major, time_minor);
            state_decimal = lock_to_decimal(state);
            to_decimal = lock_to_decimal(to);

            if (data.lock_type == WORSTGLOCK_INODE || data.lock_type == WORSTGLOCK_RGRP) {

                /* state change trace data is used both for latency.grant and latency.demote */
                if ((state_decimal < to_decimal) && (strncmp(to, target, 2) == 0)) {
                    if (strncmp(to, "NL", 2) == 0) {
                        update_records(fs, LATENCY_GRANT_NL, data, END);
                    } else if (strncmp(to, "CR", 2) == 0) {
                        update_records(fs, LATENCY_GRANT_CR, data, END);
                    } else if (strncmp(to, "CW", 2) == 0) {
                        update_records(fs, LATENCY_GRANT_CW, data, END);
                    } else if (strncmp(to, "PR", 2) == 0) {
                        update_records(fs, LATENCY_GRANT_PR, data, END);
                    } else if (strncmp(to, "PW", 2) == 0) {
                        update_records(fs, LATENCY_GRANT_PW, data, END);
                    } else if (strncmp(to, "EX", 2) == 0) {
                        update_records(fs, LATENCY_GRANT_EX, data, END);
                    }
                    update_records(fs, LATENCY_GRANT_ALL, data, END);

                } else if ((state_decimal > to_decimal) && (strncmp(to, target, 2) == 0)) {
                    if (strncmp(state, "NL", 2) == 0) {
                        update_records(fs, LATENCY_DEMOTE_NL, data, END);
                    } else if (strncmp(state, "CR", 2) == 0) {
                        update_records(fs, LATENCY_DEMOTE_CR, data, END);
                    } else if (strncmp(state, "CW", 2) == 0) {
                        update_records(fs, LATENCY_DEMOTE_CW, data, END);
                    } else if (strncmp(state, "PR", 2) == 0) {
                        update_records(fs, LATENCY_DEMOTE_PR, data, END);
                    } else if (strncmp(state, "PW", 2) == 0) {
                        update_records(fs, LATENCY_DEMOTE_PW, data, END);
                    } else if (strncmp(state, "EX", 2) == 0) {
                        update_records(fs, LATENCY_DEMOTE_EX, data, END);
                    }
                    update_records(fs, LATENCY_DEMOTE_ALL, data, END);
                }
            }
        } else if (latency_data.tracepoint ==  DEMOTE_RQ) {

            struct latency_data data;
            int64_t time_major, time_minor;
            char state[3];

            sscanf(latency_data.data, 
                "%*s [%*d] %"SCNd64".%"SCNd64": gfs2_demote_rq: %*d,%*d glock %"SCNu32":%"SCNu64" demote %s to %*s flags:%*s %*s", 
                &time_major, &time_minor, &data.lock_type, &data.number, state
            );
            data.usecs = concatenate(time_major, time_minor);

            if ((data.lock_type == WORSTGLOCK_INODE) || (data.lock_type == WORSTGLOCK_RGRP)) {

                /* demote rq trace data is used for latency.demote */
                if (strncmp(state, "NL", 2) == 0) {
                    update_records(fs, LATENCY_DEMOTE_NL, data, START);
                } else if (strncmp(state, "CR", 2) == 0) {
                    update_records(fs, LATENCY_DEMOTE_CR, data, START);
                } else if (strncmp(state, "CW", 2) == 0) {
                    update_records(fs, LATENCY_DEMOTE_CW, data, START);
                } else if (strncmp(state, "PR", 2) == 0) {
                    update_records(fs, LATENCY_DEMOTE_PR, data, START);
                } else if (strncmp(state, "PW", 2) == 0) {
                    update_records(fs, LATENCY_DEMOTE_PW, data, START);
                } else if (strncmp(state, "EX", 2) == 0) {
                    update_records(fs, LATENCY_DEMOTE_EX, data, START);
                }
                update_records(fs, LATENCY_DEMOTE_ALL, data, START);
            }
        }     
    }
    return 0;                       
}
