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

#ifndef _BTRFS_UTILS_H
#define _BTRFS_UTILS_H

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

extern char btrfs_path[MAXPATHLEN];

extern int btrfs_read_string(const char *path, char *buf, size_t buflen);
extern int btrfs_read_u64(const char *path, uint64_t *value);
extern int btrfs_read_u32(const char *path, uint32_t *value);

#endif /* _BTRFS_UTILS_H */
