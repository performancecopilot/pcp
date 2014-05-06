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
 * Handler for load average
 * "load 1.07 1.18 1.30 3/658 4944"
 *
 */

#include "metrics.h"

int
loadavg_handler(handler_t *h, fields_t *f)
{
    pmInDom indom = pmInDom_build(LINUX_DOMAIN, LOADAVG_INDOM);
    if (f->nfields < 4)
    	return -1;
    put_str_value("kernel.all.load", indom, "1 minute", f->fields[1]);
    put_str_value("kernel.all.load", indom, "5 minute", f->fields[2]);
    put_str_value("kernel.all.load", indom, "15 minute", f->fields[3]);

    return 0;
}
