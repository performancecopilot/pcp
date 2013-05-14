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
disk_handler(char *buf)
{
    char *s;
    char *inst;

    /* disk.dev.* */
    s = strtok(buf, " "); /* "disk" */
    s = strtok(NULL, " "); /* major */
    s = strtok(NULL, " "); /* minor */
    inst = strtok(NULL, " "); /* diskname */

    s = strtok(NULL, " ");
    put_str_value("disk.dev.read", DISK_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("disk.dev.read_merge", DISK_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("disk.dev.blkread", DISK_INDOM, inst, s);
    s = strtok(NULL, " "); /* read_ticks */

    s = strtok(NULL, " ");
    put_str_value("disk.dev.write", DISK_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("disk.dev.write_merge", DISK_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("disk.dev.blkwrite", DISK_INDOM, inst, s);
    s = strtok(NULL, " "); /* write_ticks */

    s = strtok(NULL, " "); /* in_flight */

    s = strtok(NULL, " ");
    put_str_value("disk.dev.avactive", DISK_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("disk.dev.aveq", DISK_INDOM, inst, s);

    return 0;
}

