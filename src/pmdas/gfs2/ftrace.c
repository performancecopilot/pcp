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
#include "lock_time.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>


static char *TRACE_PIPE = "/sys/kernel/debug/tracing/trace_pipe";

static int num_accepted_locks;
static int ftrace_metrics_found[NUM_FTRACE_ARRAYS];
static int max_glock_throughput = 900000;

/*
 * External function to allow the increment of the num_accepted_locks
 * variable from outside of ftrace.
 *
 */
extern void
ftrace_increase_num_accepted_locks(){
    num_accepted_locks++;
}

/*
 * Sets the value of max_glock_throughput using pmstore, this number
 * must be positive.
 *
 */
extern int 
ftrace_set_threshold(pmValueSet *vsp)
{
    int value;

    value = vsp->vlist[0].value.lval;
    if (value < 0)
        return PM_ERR_SIGN;

    max_glock_throughput = value;

    return 0;
}

/*
 * Returns the max number of glocks we allow per run through the trace_pipe;
 *
 */
extern int
ftrace_get_threshold()
{
    return max_glock_throughput;
}

/*
 * We open the ftrace trace file in write mode and straight after
 * close it. This acts as a way to completely clear the trace ring-
 * buffer at command.
 *
 */
static int
ftrace_clear_buffer(){

    char *TRACE = "/sys/kernel/debug/tracing/trace";
    FILE *fp;

    /* Open in write mode and then straight close will purge buffer contents */
    if (( fp = fopen(TRACE, "w")) == NULL )
        return -oserror();

    fclose(fp);

    return 0;
}

/*
 * Check type of the lock and call the correct function to work with the
 * corresponding lock type.
 *
 */
static void
gfs2_extract_trace_values(char *buffer)
{
    char *p;

    if ((p = strstr(buffer, "gfs2_glock_lock_time: "))) {
        ftrace_metrics_found[GLOCK_LOCK_TIME] = TRUE;
        gfs2_extract_glock_lock_time(p);
    }

    /* Continue if for each trace type */
}

/* 
 * We take all required data from the trace_pipe. Whilst keeping track of
 * the number of locks we have seen so far. After locks have been collected
 * we assign values and return.
 *
 */
extern int 
gfs2_refresh_ftrace_stats(pmInDom gfs_fs_indom)
{
    FILE *fp;
    int fd, flags;
    char buffer[8196];

    /* Reset the metric types we have found */
    memset(ftrace_metrics_found, 0, sizeof(ftrace_metrics_found));
    num_accepted_locks = 0;

    /* We open the pipe in both read-only and non-blocking mode */
    if ((fp = fopen(TRACE_PIPE, "r")) == NULL)
	return -oserror();

    /* Set flags of fp as non-blocking */
    fd = fileno(fp);
    flags = fcntl(fd, F_GETFL);
    if (fcntl(fd, F_SETFL, flags | O_RDONLY | O_NONBLOCK) > 0) {
        free(fp);
        return -oserror();
    }

    /* Extract data from the trace_pipe */
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (num_accepted_locks >= max_glock_throughput)
            break;

        gfs2_extract_trace_values(buffer);
    }
    fclose(fp);

    /* We clear the rest of the ring buffer after passing max_glock_throughput */
    ftrace_clear_buffer();

    /* Check for and call the processing here */
    if(ftrace_metrics_found[GLOCK_LOCK_TIME] == TRUE){
        lock_time_assign_glocks(gfs_fs_indom);
    }

    return 0;
}
