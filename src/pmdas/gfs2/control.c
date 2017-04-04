/*
 * GFS2  trace-point metrics control.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "control.h"
#include "ftrace.h"
#include "worst_glock.h"
#include "latency.h"

/* Locations of the enable files for the gfs2 tracepoints */
const char *control_locations[] = {
        [CONTROL_ALL]			= "/sys/kernel/debug/tracing/events/gfs2/enable",
        [CONTROL_GLOCK_STATE_CHANGE]	= "/sys/kernel/debug/tracing/events/gfs2/gfs2_glock_state_change/enable",
	[CONTROL_GLOCK_PUT]		= "/sys/kernel/debug/tracing/events/gfs2/gfs2_glock_put/enable",
	[CONTROL_DEMOTE_RQ]		= "/sys/kernel/debug/tracing/events/gfs2/gfs2_demote_rq/enable",
	[CONTROL_PROMOTE]		= "/sys/kernel/debug/tracing/events/gfs2/gfs2_promote/enable",
	[CONTROL_GLOCK_QUEUE]		= "/sys/kernel/debug/tracing/events/gfs2/gfs2_glock_queue/enable",
	[CONTROL_GLOCK_LOCK_TIME]	= "/sys/kernel/debug/tracing/events/gfs2/gfs2_glock_lock_time/enable",
	[CONTROL_PIN]			= "/sys/kernel/debug/tracing/events/gfs2/gfs2_pin/enable",
	[CONTROL_LOG_FLUSH]		= "/sys/kernel/debug/tracing/events/gfs2/gfs2_log_flush/enable",
	[CONTROL_LOG_BLOCKS]		= "/sys/kernel/debug/tracing/events/gfs2/gfs2_log_blocks/enable",
	[CONTROL_AIL_FLUSH]		= "/sys/kernel/debug/tracing/events/gfs2/gfs2_ail_flush/enable",
	[CONTROL_BLOCK_ALLOC]		= "/sys/kernel/debug/tracing/events/gfs2/gfs2_block_alloc/enable",
	[CONTROL_BMAP]			= "/sys/kernel/debug/tracing/events/gfs2/gfs2_bmap/enable",
	[CONTROL_RS]			= "/sys/kernel/debug/tracing/events/gfs2/gfs2_rs/enable",
        [CONTROL_BUFFER_SIZE_KB]        = "/sys/kernel/debug/tracing/buffer_size_kb",
	[CONTROL_GLOBAL_TRACING]	= "/sys/kernel/debug/tracing/tracing_on"
};

/*
 * Refresh callback for the control metrics; For traceppoints that have file
 * based enabling we use gfs2_control_check_value(), for other metrics we
 * call their corresponding "get" value.
 */
int
gfs2_control_fetch(int item, pmAtomValue *atom)
{
    if (item >= CONTROL_ALL && item <= CONTROL_GLOBAL_TRACING) {
        atom->ul = gfs2_control_check_value(control_locations[item]);

    } else if (item == CONTROL_WORSTGLOCK) {
        atom->ul = worst_glock_get_state();

    } else if (item == CONTROL_LATENCY) {
        atom->ul = latency_get_state();   

    } else if (item == CONTROL_FTRACE_GLOCK_THRESHOLD) {
        atom->ul = ftrace_get_threshold();

    } else {
       return PM_ERR_PMID;

    }
    return 1;
}

/*
 * Attempt open the enable file for the given filename and set the value
 * contained in pmValueSet. The enable file for the tracepoint only accept
 * 0 for disabled or 1 for enabled.
 */
int 
gfs2_control_set_value(const char *filename, pmValueSet *vsp)
{
    FILE *fp;
    int	sts = 0;
    int value = vsp->vlist[0].value.lval;

    if (strncmp(filename, control_locations[CONTROL_BUFFER_SIZE_KB],
                   sizeof(control_locations[CONTROL_BUFFER_SIZE_KB])-1) == 0) {
        /* Special case for buffer_size_kb */
        if (value < 0 || value > 131072) /* Allow upto 128mb buffer per CPU */
            return - oserror();

    } else if (value < 0 || value > 1) {
	return -oserror();
    }

    fp = fopen(filename, "w");
    if (!fp) {
	sts = -oserror(); /* EACCESS, File not found (stats not supported) */;
    } else {
        fprintf(fp, "%d\n", value);
        fclose(fp);
    }
    return sts;
}

/*
 * We check the tracepoint enable file given by filename and return the value
 * contained. This should either be 0 for disabled or 1 for enabled. In the
 * event of permissions or file not found we will return zero.
 */
int 
gfs2_control_check_value(const char *filename)
{
    FILE *fp;
    char buffer[16];
    int value = 0;

    fp = fopen(filename, "r");
    if (fp) {
        while (fgets(buffer, sizeof(buffer), fp) != NULL)
            sscanf(buffer, "%d", &value);
	fclose(fp);
    }
    return value;
}
