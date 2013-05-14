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
 * load 1.07 1.18 1.30 3/658 4944
 *
 */

#include "metrics.h"

int
load_handler(char *buf)
{
    char *s;

    s = strtok(buf, " ");
    s = strtok(NULL, " ");
    put_str_value("kernel.all.load", LOADAVG_INDOM, "1 minute", s);
    s = strtok(NULL, " ");
    put_str_value("kernel.all.load", LOADAVG_INDOM, "5 minute", s);
    s = strtok(NULL, " ");
    put_str_value("kernel.all.load", LOADAVG_INDOM, "15 minute", s);

    return 0;
}
