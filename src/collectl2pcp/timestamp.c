/*
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
 *
 * Handler for timestamps
 * >>> 1368076410.001 <<<
 */

#include "metrics.h"

static int seconds = 0, mseconds = 0;

int
timestamp_flush(void)
{
    int sts;
    int err = 0;

    /*
     * timstamps in collectl logs are seconds.mseconds since the epoch in utc
     * Since we've set pmiSetTimezone to what was found in the header, we need
     * to offset it here so the PCP archive matches the host timezone.
     */
    if (seconds && (sts = pmiWrite(seconds + utc_offset * 60 * 60, mseconds*1000)) < 0) {
	if (sts != PMI_ERR_NODATA) {
	    fprintf(stderr, "Error: pmiWrite failed: error %d: %s\n", sts, pmiErrStr(sts));
	    err = sts; /* probably fatal */
	}
    }

    return err;
}

int
timestamp_handler(handler_t *h, fields_t *f)
{

    int sts;
    int err = 0;

    /* >>> 1368076390.001 <<< */
    if (f->nfields != 3)
    	return -1;
    if ((sts = timestamp_flush()) < 0)
    	err = sts;
    sscanf(f->fields[1], "%d.%d", &seconds, &mseconds);

    return err;
}
