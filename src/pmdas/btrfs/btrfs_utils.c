/*
 * Copyright (c) 2026 Red Hat.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btrfs_utils.h"

char btrfs_path[MAXPATHLEN];

int
btrfs_read_string(const char *path, char *buf, size_t buflen)
{
    FILE *fp;
    char *nl;

    fp = fopen(path, "r");
    if (fp == NULL)
	return -1;
    if (fgets(buf, buflen, fp) == NULL) {
	fclose(fp);
	return -1;
    }
    fclose(fp);
    nl = strchr(buf, '\n');
    if (nl)
	*nl = '\0';
    return 0;
}

int
btrfs_read_u64(const char *path, uint64_t *value)
{
    char buf[64];

    if (btrfs_read_string(path, buf, sizeof(buf)) < 0)
	return -1;
    *value = strtoull(buf, NULL, 0);
    return 0;
}

int
btrfs_read_u32(const char *path, uint32_t *value)
{
    char buf[64];

    if (btrfs_read_string(path, buf, sizeof(buf)) < 0)
	return -1;
    *value = (uint32_t)strtoul(buf, NULL, 0);
    return 0;
}
