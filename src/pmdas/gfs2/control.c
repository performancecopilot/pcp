/*
 * GFS2  trace-point metrics control.
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
#include "control.h"
#include <ctype.h>

const char *control_locations[] = {
	[CONTROL_GLOCK_LOCK_TIME] = "/sys/kernel/debug/tracing/events/gfs2/gfs2_glock_lock_time/enable"
};

/*
 * Refreshing of the control metrics.
 *
 */
extern int
gfs2_control_fetch(int item){
    switch (item) {
        case CONTROL_GLOCK_LOCK_TIME: /* gfs2.control.glock_lock_time */
            return gfs2_control_check_value(control_locations[CONTROL_GLOCK_LOCK_TIME]); 
            break;
        default:
            return PM_ERR_PMID;
        }

    return 0;
}

/*
 * Attempt to open the given file and set a value in this file. We then return
 * any issues with this operation.
 *
 */
extern int 
gfs2_control_set_value(const char *filename, pmValueSet *vsp){
    FILE *fp;
    int value;
    int	sts = 0;

    value = vsp->vlist[0].value.lval;
    if (value < 0)
	return PM_ERR_SIGN;

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
 * We attempt to open the given file and check the value that is contained with.
 * In the event that the file does not exist or permission errors, we default to
 * 0 which signifies disabled. 
 *
 */
extern int 
gfs2_control_check_value(const char *filename){
    FILE *fp;
    char buffer[5];
    int value = 0;

    fp = fopen(filename, "r");
    if (!fp) {
	value = 0;
    } else {
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            sscanf(buffer, "%d", &value);
        }
	fclose(fp);
    }
    return value;
}
