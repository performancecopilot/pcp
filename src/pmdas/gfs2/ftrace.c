/*
 * GFS2 ftrace based trace-point metrics.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include "pmdagfs2.h"
#include "ftrace.h"
#include "worst_glock.h"

#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <sys/sysmacros.h>
#include <sys/types.h>


static char *TRACE_PIPE = "/sys/kernel/debug/tracing/trace_pipe";
static int max_glock_throughput = INITIAL_GLOBAL_MAX_GLOCK_THROUGHPUT;

static struct ftrace_data *ftrace_data;
static int capacity, num_accepted_entries;

/*
 * Fetches the value for the given metric item and then assigns to pmAtomValue.
 * We check to see if item is in valid range for the metric.
 */
int
gfs2_ftrace_fetch(int item, struct ftrace *ftrace, pmAtomValue *atom)
{
    /* Ensure that metric value wanted is valid */
    if ((item < 0 || item >= NUM_TRACEPOINT_STATS))
	return PM_ERR_PMID;

    atom->ull = ftrace->values[item];
    return 1;
}

/*
 * External function to allow the increment of the num_accepted_locks
 * variable from pmStore.
 */
void
ftrace_increase_num_accepted_entries(){
    num_accepted_entries++;
}

/*
 * Sets the value of max_glock_throughput using pmstore, value should
 * must be positive.
 */
int 
ftrace_set_threshold(pmValueSet *vsp)
{
    int value = vsp->vlist[0].value.lval;

    if (value < 0) /* Ensure positive value */
        return PM_ERR_SIGN;

    max_glock_throughput = value;

    return 0;
}

/*
 * Returns the max number of glocks we allow per run through the trace_pipe,
 * Used by the fetch for the control metrics.
 */
int
ftrace_get_threshold()
{
    return max_glock_throughput;
}

/*
 * We open the ftrace trace file in write mode and straight after
 * close it. This acts as a way to completely clear the trace ring-
 * buffer when needed.
 */
static int
ftrace_clear_buffer()
{
    char *TRACE = "/sys/kernel/debug/tracing/trace";
    FILE *fp;

    /* Open in write mode and then straight close will purge buffer contents */
    if (( fp = fopen(TRACE, "w")) == NULL )
        return -oserror();

    fclose(fp);

    return 0;
}

/*
 * We take tracedata from the trace pipe and store only the data which is
 * from GFS2 metrics. We collect all the data in one array to be worked
 * through later, this is because all trace data comes through the
 * trace pipe mixed. 
 */
static int
gfs2_extract_trace_values(char *buffer)
{
    struct ftrace_data temp;
    int major, minor;
    char *data;

    /* Allocate memory for our data */
    if (ftrace_data == NULL) {
        ftrace_data = malloc(capacity * sizeof(struct ftrace_data));
        if (ftrace_data == NULL) {
            return -oserror();
        }
    }

    /* Interpret data, we work out what tracepoint it belongs to first */
    if ((data = strstr(buffer, "gfs2_glock_state_change: "))) {
        temp.tracepoint = GLOCK_STATE_CHANGE;
        sscanf(data, "gfs2_glock_state_change: %"SCNu32",%"SCNu32"", &major, &minor);

    } else if ((data = strstr(buffer, "gfs2_glock_put: "))) {
        temp.tracepoint = GLOCK_PUT;
        sscanf(data, "gfs2_glock_put: %"SCNu32",%"SCNu32"", &major, &minor);

    } else if ((data = strstr(buffer, "gfs2_demote_rq: "))) {
        temp.tracepoint = DEMOTE_RQ;
        sscanf(data, "gfs2_demote_rq: %"SCNu32",%"SCNu32"", &major, &minor);

    } else if ((data = strstr(buffer, "gfs2_promote: "))) {
        temp.tracepoint = PROMOTE;
        sscanf(data, "gfs2_promote: %"SCNu32",%"SCNu32"", &major, &minor);

    } else if ((data = strstr(buffer, "gfs2_glock_queue: "))) {
        temp.tracepoint = GLOCK_QUEUE;
        sscanf(data, "gfs2_glock_queue: %"SCNu32",%"SCNu32"", &major, &minor);

    } else if ((data = strstr(buffer, "gfs2_glock_lock_time: "))) {
        temp.tracepoint = GLOCK_LOCK_TIME;
        sscanf(data, "gfs2_glock_lock_time: %"SCNu32",%"SCNu32"", &major, &minor);

        /*
         * Pass tracepoint data over for worst_glock metrics for processing,
         * only if the metrics are enabled. 
         */
        if (worst_glock_get_state() == 1)
            gfs2_extract_worst_glock(data);

    } else if ((data = strstr(buffer, "gfs2_pin: "))) {
        temp.tracepoint = PIN;
        sscanf(data, "gfs2_pin: %"SCNu32",%"SCNu32"", &major, &minor);

    } else if ((data = strstr(buffer, "gfs2_log_flush: "))) {
        temp.tracepoint = LOG_FLUSH;
        sscanf(data, "gfs2_log_flush: %"SCNu32",%"SCNu32"", &major, &minor);

    } else if ((data = strstr(buffer, "gfs2_log_blocks: "))) {
        temp.tracepoint = LOG_BLOCKS;
        sscanf(data, "gfs2_log_blocks: %"SCNu32",%"SCNu32"", &major, &minor);

    } else if ((data = strstr(buffer, "gfs2_ail_flush: "))) {
        temp.tracepoint = AIL_FLUSH;
        sscanf(data, "gfs2_ail_flush: %"SCNu32",%"SCNu32"", &major, &minor);

    } else if ((data = strstr(buffer, "gfs2_block_alloc: "))) {
        temp.tracepoint = BLOCK_ALLOC;
        sscanf(data, "gfs2_block_alloc: %"SCNu32",%"SCNu32" %s", &major, &minor, data);

    } else if ((data = strstr(buffer, "gfs2_bmap: "))) {
        temp.tracepoint = BMAP;
        sscanf(data, "gfs2_bmap: %"SCNu32",%"SCNu32"", &major, &minor);

    } else if ((data = strstr(buffer, "gfs2_rs: "))) {
        temp.tracepoint = RS;
        sscanf(data, "gfs2_rs: %"SCNu32",%"SCNu32" %s", &major, &minor, data);
    } else {
        return 0; /* If we do not have matching data, return and continue */
    }

    temp.dev_id = makedev(major, minor);
    strncpy(temp.data, data, sizeof(temp.data)-1);

    /* Re-allocate and extend array if we are near capacity */
    if (num_accepted_entries == capacity) {
        struct ftrace_data *ftrace_data_realloc = 
            realloc(ftrace_data, (capacity + FTRACE_ARRAY_CAPACITY) * sizeof(struct ftrace_data)
        );

        if (ftrace_data_realloc == NULL) {
            free(ftrace_data);
            ftrace_data = NULL;
            return -oserror();
        } else {
            ftrace_data = ftrace_data_realloc;
            ftrace_data_realloc = NULL;
            capacity += FTRACE_ARRAY_CAPACITY;
        }
    }

    /* Assign data in the array and update counters */
    ftrace_data[num_accepted_entries] = temp;
    num_accepted_entries++;

    return 0;
}

/*
 * We work though each mounted filesystem and update the metric data based
 * off what tracepoint information we have collected from the trace pipe.
 * Data is consumed and the ftrace_data array is deallocated on each refresh
 * cycle.
 */
static void
gfs2_assign_ftrace(pmInDom gfs2_fs_indom) 
{
    int i, j, k, sts;
    struct gfs2_fs *fs;

    /* We walk through for each filesystem */
    for (pmdaCacheOp(gfs2_fs_indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(gfs2_fs_indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	sts = pmdaCacheLookup(gfs2_fs_indom, i, NULL, (void **)&fs);
	if (sts != PMDA_CACHE_ACTIVE)
	    continue;   

        for (j = 0; j < NUM_TRACEPOINT_STATS; j++) {
            /* Reset old metric data for all tracepoints */
            fs->ftrace.values[j] = 0;
        }      

        for (k = 0; k < num_accepted_entries; k++) {
            if (fs->dev_id != ftrace_data[k].dev_id)
                continue;

            /* Work through the data, increasing metric counters */
            if (ftrace_data[k].tracepoint == GLOCK_STATE_CHANGE) {
                char state[3], target[3];

                sscanf(ftrace_data[k].data, 
                    "gfs2_glock_state_change: %*d,%*d glock %*d:%*d state %*s to %s tgt:%s dmt:%*s flags:%*s", 
                    state, target
                );

                if (strncmp(state, "NL", 2) == 0) {
                    fs->ftrace.values[FTRACE_GLOCKSTATE_NULLLOCK]++;
                } else if (strncmp(state, "CR", 2) == 0) {
                    fs->ftrace.values[FTRACE_GLOCKSTATE_CONCURRENTREAD]++;
                } else if (strncmp(state, "CW", 2) == 0) {
                    fs->ftrace.values[FTRACE_GLOCKSTATE_CONCURRENTWRITE]++;
                } else if (strncmp(state, "PR", 2) == 0) {
                    fs->ftrace.values[FTRACE_GLOCKSTATE_PROTECTEDREAD]++;
                } else if (strncmp(state, "PW", 2) == 0) {
                    fs->ftrace.values[FTRACE_GLOCKSTATE_PROTECTEDWRITE]++;
                } else if (strncmp(state, "EX", 2) == 0) {
                   fs->ftrace.values[FTRACE_GLOCKSTATE_EXCLUSIVE]++;
                }
                fs->ftrace.values[FTRACE_GLOCKSTATE_TOTAL]++;

                if (strncmp(state, target, 2) == 0) {
                    fs->ftrace.values[FTRACE_GLOCKSTATE_GLOCK_CHANGEDTARGET]++;
                } else {
                    fs->ftrace.values[FTRACE_GLOCKSTATE_GLOCK_MISSEDTARGET]++;
                }

            } else if (ftrace_data[k].tracepoint == GLOCK_PUT) {
                char state[3];

                sscanf(ftrace_data[k].data, 
                    "gfs2_glock_put: %*d,%*d glock %*d:%*d state %*s => %s flags:%*s", 
                    state
                );

                if (strncmp(state, "NL", 2) == 0) {
                    fs->ftrace.values[FTRACE_GLOCKPUT_NULLLOCK]++;
                } else if (strncmp(state, "CR", 2) == 0) {
                    fs->ftrace.values[FTRACE_GLOCKPUT_CONCURRENTREAD]++;
                } else if (strncmp(state, "CW", 2) == 0) {
                    fs->ftrace.values[FTRACE_GLOCKPUT_CONCURRENTWRITE]++;
                } else if (strncmp(state, "PR", 2) == 0) {
                    fs->ftrace.values[FTRACE_GLOCKPUT_PROTECTEDREAD]++;
                } else if (strncmp(state, "PW", 2) == 0) {
                    fs->ftrace.values[FTRACE_GLOCKPUT_PROTECTEDWRITE]++;
                } else if (strncmp(state, "EX", 2) == 0) {
                    fs->ftrace.values[FTRACE_GLOCKPUT_EXCLUSIVE]++;
                }
                fs->ftrace.values[FTRACE_GLOCKPUT_TOTAL]++;

            } else if (ftrace_data[k].tracepoint == DEMOTE_RQ) {
                char state[3], remote[7];

                sscanf(ftrace_data[k].data, 
                    "gfs2_demote_rq: %*d,%*d glock %*d:%*d demote %*s to %s flags:%*s %s", 
                    state, remote
                );

                if (strncmp(state, "NL", 2) == 0) {
                    fs->ftrace.values[FTRACE_DEMOTERQ_NULLLOCK]++;
                } else if (strncmp(state, "CR", 2) == 0) {
                    fs->ftrace.values[FTRACE_DEMOTERQ_CONCURRENTREAD]++;
                } else if (strncmp(state, "CW", 2) == 0) {
                    fs->ftrace.values[FTRACE_DEMOTERQ_CONCURRENTWRITE]++;
                } else if (strncmp(state, "PR", 2) == 0) {
                    fs->ftrace.values[FTRACE_DEMOTERQ_PROTECTEDREAD]++;
                } else if (strncmp(state, "PW", 2) == 0) {
                    fs->ftrace.values[FTRACE_DEMOTERQ_PROTECTEDWRITE]++;
                } else if (strncmp(state, "EX", 2) == 0) {
                    fs->ftrace.values[FTRACE_DEMOTERQ_EXCLUSIVE]++;
                }
                fs->ftrace.values[FTRACE_DEMOTERQ_TOTAL]++;

                if (strncmp(remote, "remote", 6) == 0) {
                    fs->ftrace.values[FTRACE_DEMOTERQ_REQUESTED_REMOTE]++;
                } else if (strncmp(remote, "local", 6) == 0) {
                    fs->ftrace.values[FTRACE_DEMOTERQ_REQUESTED_LOCAL]++;
                } 

            } else if (ftrace_data[k].tracepoint == PROMOTE) {
                char state[3], first[6];

                sscanf(ftrace_data[k].data, 
                    "gfs2_promote: %*d,%*d glock %*d:%*d promote %s %s", 
                    first, state
                );

                if (strncmp(first, "first", 5) == 0) {
                    if (strncmp(state, "NL", 2) == 0) {
                        fs->ftrace.values[FTRACE_PROMOTE_FIRST_NULLLOCK]++;
                    } else if (strncmp(state, "CR", 2) == 0) {
                        fs->ftrace.values[FTRACE_PROMOTE_FIRST_CONCURRENTREAD]++;
                    } else if (strncmp(state, "CW", 2) == 0) {
                        fs->ftrace.values[FTRACE_PROMOTE_FIRST_CONCURRENTWRITE]++;
                    } else if (strncmp(state, "PR", 2) == 0) {
                        fs->ftrace.values[FTRACE_PROMOTE_FIRST_PROTECTEDREAD]++;
                    } else if (strncmp(state, "PW", 2) == 0) {
                        fs->ftrace.values[FTRACE_PROMOTE_FIRST_PROTECTEDWRITE]++;
                    } else if (strncmp(state, "EX", 2) == 0) {
                        fs->ftrace.values[FTRACE_PROMOTE_FIRST_EXCLUSIVE]++;
                    }
                } else if (strncmp(first, "other", 5) == 0) {
                    if (strncmp(state, "NL", 2) == 0) {
                        fs->ftrace.values[FTRACE_PROMOTE_OTHER_NULLLOCK]++;
                    } else if (strncmp(state, "CR", 2) == 0) {
                        fs->ftrace.values[FTRACE_PROMOTE_OTHER_CONCURRENTREAD]++;
                    } else if (strncmp(state, "CW", 2) == 0) {
                        fs->ftrace.values[FTRACE_PROMOTE_OTHER_CONCURRENTWRITE]++;
                    } else if (strncmp(state, "PR", 2) == 0) {
                        fs->ftrace.values[FTRACE_PROMOTE_OTHER_PROTECTEDREAD]++;
                    } else if (strncmp(state, "PW", 2) == 0) {
                        fs->ftrace.values[FTRACE_PROMOTE_OTHER_PROTECTEDWRITE]++;
                    } else if (strncmp(state, "EX", 2) == 0) {
                        fs->ftrace.values[FTRACE_PROMOTE_OTHER_EXCLUSIVE]++;
                    }
                }
                fs->ftrace.values[FTRACE_PROMOTE_TOTAL]++;

            } else if (ftrace_data[k].tracepoint == GLOCK_QUEUE) {
                char state[3], queue[3];
            
                sscanf(ftrace_data[k].data, 
                    "gfs2_glock_queue: %*d,%*d glock %*d:%*d %squeue %s", 
                    queue, state
                );

                if (strncmp(queue, "", 2) == 0) {
                    if (strncmp(state, "NL", 2) == 0) {
                        fs->ftrace.values[FTRACE_GLOCKQUEUE_QUEUE_NULLLOCK]++;
                    } else if (strncmp(state, "CR", 2) == 0) {
                        fs->ftrace.values[FTRACE_GLOCKQUEUE_QUEUE_CONCURRENTREAD]++;
                    } else if (strncmp(state, "CW", 2) == 0) {
                        fs->ftrace.values[FTRACE_GLOCKQUEUE_QUEUE_CONCURRENTWRITE]++;
                    } else if (strncmp(state, "PR", 2) == 0) {
                       fs->ftrace.values[FTRACE_GLOCKQUEUE_QUEUE_PROTECTEDREAD]++;
                    } else if (strncmp(state, "PW", 2) == 0) {
                       fs->ftrace.values[FTRACE_GLOCKQUEUE_QUEUE_PROTECTEDWRITE]++;
                    } else if (strncmp(state, "EX", 2) == 0) {
                       fs->ftrace.values[FTRACE_GLOCKQUEUE_QUEUE_EXCLUSIVE]++;
                    }
                    fs->ftrace.values[FTRACE_GLOCKQUEUE_QUEUE_TOTAL]++;

                } else if (strncmp(queue, "de", 2) == 0) {
                    if (strncmp(state, "NL", 2) == 0) {
                        fs->ftrace.values[FTRACE_GLOCKQUEUE_DEQUEUE_NULLLOCK]++;
                    } else if (strncmp(state, "CR", 2) == 0) {
                        fs->ftrace.values[FTRACE_GLOCKQUEUE_DEQUEUE_CONCURRENTREAD]++;
                    } else if (strncmp(state, "CW", 2) == 0) {
                        fs->ftrace.values[FTRACE_GLOCKQUEUE_DEQUEUE_CONCURRENTWRITE]++;
                    } else if (strncmp(state, "PR", 2) == 0) {
                        fs->ftrace.values[FTRACE_GLOCKQUEUE_DEQUEUE_PROTECTEDREAD]++;
                    } else if (strncmp(state, "PW", 2) == 0) {
                        fs->ftrace.values[FTRACE_GLOCKQUEUE_DEQUEUE_PROTECTEDWRITE]++;
                    } else if (strncmp(state, "EX", 2) == 0) {
                        fs->ftrace.values[FTRACE_GLOCKQUEUE_DEQUEUE_EXCLUSIVE]++;
                    }
                    fs->ftrace.values[FTRACE_GLOCKQUEUE_DEQUEUE_TOTAL]++;
                }
                fs->ftrace.values[FTRACE_PROMOTE_TOTAL]++;

            } else if (ftrace_data[k].tracepoint == GLOCK_LOCK_TIME) {
                uint32_t lock_type;

                sscanf(ftrace_data[k].data, 
                    "gfs2_glock_lock_time: %*d,%*d glock %"SCNu32":%*d status:%*d flags:%*x tdiff:%*d srtt:%*d/%*d srttb:%*d/%*d sirt:%*d/%*d dcnt:%*d qcnt:%*d",
                    &lock_type
                );

                if (lock_type == 1) {
                    fs->ftrace.values[FTRACE_GLOCKLOCKTIME_TRANS]++;
                } else if (lock_type == 2) {
                    fs->ftrace.values[FTRACE_GLOCKLOCKTIME_INDOE]++;
                } else if (lock_type == 3) {
                    fs->ftrace.values[FTRACE_GLOCKLOCKTIME_RGRP]++;
                } else if (lock_type == 4) {
                    fs->ftrace.values[FTRACE_GLOCKLOCKTIME_META]++;
                } else if (lock_type == 5) {
                    fs->ftrace.values[FTRACE_GLOCKLOCKTIME_IOPEN]++;
                } else if (lock_type == 6) {
                    fs->ftrace.values[FTRACE_GLOCKLOCKTIME_FLOCK]++;
                } else if (lock_type == 8) {
                    fs->ftrace.values[FTRACE_GLOCKLOCKTIME_QUOTA]++;
                } else if (lock_type == 9) {
                    fs->ftrace.values[FTRACE_GLOCKLOCKTIME_JOURNAL]++;
                }
                fs->ftrace.values[FTRACE_GLOCKLOCKTIME_TOTAL]++;

            } else if (ftrace_data[k].tracepoint == PIN) {
                char pinned[6];
                uint32_t length;

                sscanf(ftrace_data[k].data, 
                    "gfs2_pin: %*d,%*d log %s %*d/%"SCNu32" inode: %*d",
                    pinned, &length
                );                

                if (strncmp(pinned, "pin", 5) == 0) {
                    fs->ftrace.values[FTRACE_PIN_PINTOTAL]++;
                } else if (strncmp(pinned, "unpin", 5) == 0) {
                    fs->ftrace.values[FTRACE_PIN_UNPINTOTAL]++;
                }
                fs->ftrace.values[FTRACE_PIN_TOTAL]++;
        
                if(fs->ftrace.values[FTRACE_PIN_LONGESTPINNED] < length)
                    fs->ftrace.values[FTRACE_PIN_LONGESTPINNED] = length;

            } else if (ftrace_data[k].tracepoint == LOG_FLUSH) {
                fs->ftrace.values[FTRACE_LOGFLUSH_TOTAL]++; 

            } else if (ftrace_data[k].tracepoint == LOG_BLOCKS) {
                fs->ftrace.values[FTRACE_LOGBLOCKS_TOTAL]++;

            } else if (ftrace_data[k].tracepoint == AIL_FLUSH) {
                fs->ftrace.values[FTRACE_AILFLUSH_TOTAL]++;

            } else if (ftrace_data[k].tracepoint == BLOCK_ALLOC) {
                char type[8];

                sscanf(ftrace_data[k].data, 
                    "gfs2_block_alloc: %*d,%*d bmap %*u alloc %*u/%*u %s rg:%*u rf:%*u rr:%*u",
                    type 
                );

                if (strncmp(type, "free", 8) == 0) {
                    fs->ftrace.values[FTRACE_BLOCKALLOC_FREE]++;
                } else if (strncmp(type, "used", 8) == 0) {
                    fs->ftrace.values[FTRACE_BLOCKALLOC_USED]++;
                } else if (strncmp(type, "dinode", 8) == 0) {
                   fs->ftrace.values[FTRACE_BLOCKALLOC_DINODE]++;
                } else if (strncmp(type, "unlinked", 8) == 0) {
                    fs->ftrace.values[FTRACE_BLOCKALLOC_UNLINKED]++;
            }
            fs->ftrace.values[FTRACE_BLOCKALLOC_TOTAL]++;

            } else if (ftrace_data[k].tracepoint == BMAP) {
                char type[8];

                sscanf(ftrace_data[k].data, 
                    "gfs2_bmap: %*d,%*d bmap %*u map %*u/%*u to %*u flags:%*x %s %*d",
                    type 
                );

                if (strncmp(type, "create", 8) == 0) {
                    fs->ftrace.values[FTRACE_BMAP_CREATE]++;
                } else if (strncmp(type, "nocreate", 8) == 0) {
                    fs->ftrace.values[FTRACE_BMAP_NOCREATE]++;
                }
                fs->ftrace.values[FTRACE_BMAP_TOTAL]++;

            } else if (ftrace_data[k].tracepoint == RS) {
                char type[8];

                sscanf(ftrace_data[k].data, 
                    "gfs2_rs: %*d,%*d bmap %*u resrv %*u rg:%*u rf:%*u rr:%*u %s f:%*u",
                    type 
                );

                if (strncmp(type, "del", 4) == 0) {
                    fs->ftrace.values[FTRACE_RS_DEL]++;
                } else if (strncmp(type, "tdel", 4) == 0) {
                    fs->ftrace.values[FTRACE_RS_TDEL]++;
                } else if (strncmp(type, "ins", 4) == 0) {
                    fs->ftrace.values[FTRACE_RS_INS]++;
                } else if (strncmp(type, "clm", 4) == 0) {
                    fs->ftrace.values[FTRACE_RS_CLM]++;
                }
                fs->ftrace.values[FTRACE_RS_TOTAL]++;

            }
        }
        /* LOGFLUSH & AILFLUSH have start/end operations so we divide the total values */
        fs ->ftrace.values[FTRACE_LOGFLUSH_TOTAL] /= 2;
        fs ->ftrace.values[FTRACE_AILFLUSH_TOTAL] /= 2;
    }

    /* Free memory */
    free(ftrace_data);
    ftrace_data = NULL;
}

/* 
 * We take all required data from the trace_pipe. Whilst keeping track of
 * the number of locks we have seen so far. After locks have been collected
 * we assign values and return.
 */
int 
gfs2_refresh_ftrace_stats(pmInDom gfs_fs_indom)
{
    FILE *fp;
    int fd, flags;
    char buffer[8196];

    /* Reset the metric types we have found */
    num_accepted_entries = 0;
    capacity = FTRACE_ARRAY_CAPACITY;

    /* We open the pipe in both read-only and non-blocking mode */
    if ((fp = fopen(TRACE_PIPE, "r")) == NULL)
	return -oserror();

    /* Set flags of fp as non-blocking */
    fd = fileno(fp);
    flags = fcntl(fd, F_GETFL);
    if (fcntl(fd, F_SETFL, flags | O_RDONLY | O_NONBLOCK) < 0) {
        fclose(fp);
        return -oserror();
    }

    /* Extract data from the trace_pipe */
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (num_accepted_entries >= max_glock_throughput)
            break;

        /* In the event of an allocation error */
        if (gfs2_extract_trace_values(buffer) != 0)
            break;
    }
    fclose(fp);

    /* Clear the rest of the ring buffer after passing max_glock_throughput */
    ftrace_clear_buffer();

    /* Processing here */
    if (ftrace_data != NULL)
        gfs2_assign_ftrace(gfs_fs_indom);

    /* Assign worst_glock entries */
    if (worst_glock_get_state() == 1)
    worst_glock_assign_glocks(gfs_fs_indom);

    return 0;
}
