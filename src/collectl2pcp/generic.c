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
 * Generic handler for singular metrics with value in fields[1] e.g. :
 * "processes 516779"
 */

#include "metrics.h"

/* generic handler for <tag> <value> */
int
generic1_handler(handler_t *h, fields_t *f)
{
    put_str_value(h->metric_name, PM_INDOM_NULL, NULL, f->fields[1]);
    return 0;
}

/* generic handler for <tag> <somethingelse> <value> */
int
generic2_handler(handler_t *h, fields_t *f)
{
    put_str_value(h->metric_name, PM_INDOM_NULL, NULL, f->fields[2]);
    return 0;
}
