/*
 * Copyright (c) 2013-2015 Red Hat Inc.
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

static void
put_disk_str(const char *leaf, char *inst, char *value)
{
    const char *subtree;
    pmInDom indom;
    char metric[MAXPATHLEN];

    if (strncmp(inst, "dm-", 3) == 0) {
	subtree = "disk.dm.";
	indom = pmInDom_build(LINUX_DOMAIN, DM_INDOM);
    }
    else {
	subtree = "disk.dev.";
	indom = pmInDom_build(LINUX_DOMAIN, DISK_INDOM);
    }

    strncpy(metric, subtree, sizeof(metric) - 1);
    strncat(metric, leaf, sizeof(metric) - strlen(metric) - 1);

    put_str_value(metric, indom, inst, value);
}

static void
put_disk_ull(const char *leaf, char *inst, unsigned long long value)
{
    char str[MAXPATHLEN];

    sprintf(str, "%llu", value);
    put_disk_str(leaf, inst, str);
}

int
disk_handler(handler_t *h, fields_t *f)
{
    char *inst;

    if (f->nfields != 15)
    	return -1;

    /* disk.{dev,dm}.* */
    inst = f->fields[3]; /* device name */

    put_disk_str("read", inst, f->fields[4]);
    put_disk_str("read_merge", inst, f->fields[5]);
    put_disk_str("blkread", inst, f->fields[6]);
    put_disk_str("read_rawactive", inst, f->fields[7]);
    put_disk_str("write", inst, f->fields[8]);
    put_disk_str("write_merge", inst, f->fields[9]);
    put_disk_str("blkwrite", inst, f->fields[10]);
    put_disk_str("write_rawactive", inst, f->fields[11]);
    /* skip in_flight at f->fields[12] */
    put_disk_str("avactive", inst, f->fields[13]);
    put_disk_str("aveq", inst, f->fields[14]);

    /* derived values */
    put_disk_ull("write_bytes", inst, strtoull(f->fields[10], NULL, 0) / 2);
    put_disk_ull("read_bytes", inst, strtoull(f->fields[6], NULL, 0) / 2);
    put_disk_ull("total_bytes", inst,
    	strtoull(f->fields[6], NULL, 0) + strtoull(f->fields[10], NULL, 0));

    return 0;
}

