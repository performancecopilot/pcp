/*
 * Copyright (c) 2013 Red Hat Inc.
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

    if (seconds && (sts = pmiWrite(seconds, mseconds*1000)) < 0) {
	if (sts != PMI_ERR_NODATA) {
	    fprintf(stderr, "Error: pmiWrite failed: error %d: %s\n", sts, pmiErrStr(sts));
	    err = sts; /* probably fatal */
	}
    }

    return err;
}

int
timestamp_handler(char *buf)
{

    int sts;
    int err = 0;

    if ((sts = timestamp_flush()) < 0)
    	err = sts;
    sscanf(buf, ">>> %d.%d", &seconds, &mseconds);

    return err;
}
