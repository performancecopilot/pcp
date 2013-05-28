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
 * Handler for disk.dev
 *
 * disk    8       0 sda 13911739 267073 1248592592 58081804 4912190 5603474 382020376 181762699 0 53315955 240138732
 */

#include "metrics.h"

int
disk_handler(handler_t *h, fields_t *f)
{
    char *inst;
    pmInDom indom = pmInDom_build(LINUX_DOMAIN, DISK_INDOM);

    if (f->nfields != 15)
    	return -1;

    /* disk.dev.* */
    inst = f->fields[3]; /* diskname */

    put_str_value("disk.dev.read", indom, inst, f->fields[4]);
    put_str_value("disk.dev.read_merge", indom, inst, f->fields[5]);
    put_str_value("disk.dev.blkread", indom, inst, f->fields[6]);
    /* skip read_ticks at f->fields[7] */
    put_str_value("disk.dev.write", indom, inst, f->fields[8]);
    put_str_value("disk.dev.write_merge", indom, inst, f->fields[9]);
    put_str_value("disk.dev.blkwrite", indom, inst, f->fields[10]);
    /* skip write_ticks at f->fields[11] */
    /* skip in_flight at f->fields[12] */
    put_str_value("disk.dev.avactive", indom, inst, f->fields[13]);
    put_str_value("disk.dev.aveq", indom, inst, f->fields[14]);

    /* derived values */
    put_ull_value("disk.dev.write_bytes", indom, inst, strtoull(f->fields[10], NULL, 0) / 2);
    put_ull_value("disk.dev.read_bytes", indom, inst, strtoull(f->fields[6], NULL, 0) / 2);
    put_ull_value("disk.dev.total_bytes", indom, inst,
    	strtoull(f->fields[6], NULL, 0) + strtoull(f->fields[10], NULL, 0));

    return 0;
}

