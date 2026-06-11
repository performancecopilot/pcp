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

#ifndef _BTRFS_DEV_H
#define _BTRFS_DEV_H

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

enum { BTRFS_DEV_INDOM = 1 };

enum {
    BTRFS_DEV_WRITE_ERRS = 0,
    BTRFS_DEV_READ_ERRS,
    BTRFS_DEV_FLUSH_ERRS,
    BTRFS_DEV_CORRUPTION_ERRS,
    BTRFS_DEV_GENERATION_ERRS,
};

typedef struct btrfs_dev {
    char	uuid[64];
    char	devid[32];
    uint64_t	write_errs;
    uint64_t	read_errs;
    uint64_t	flush_errs;
    uint64_t	corruption_errs;
    uint64_t	generation_errs;
} btrfs_dev_t;

extern void btrfs_dev_refresh(pmInDom indom);

#endif /* _BTRFS_DEV_H */
