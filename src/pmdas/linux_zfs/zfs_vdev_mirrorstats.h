/*
 * Copyright (c) 2021 Red Hat.
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

typedef struct zfs_vdev_mirrorstats {
    uint64_t rotating_linear;
    uint64_t rotating_offset;
    uint64_t rotating_seek;
    uint64_t non_rotating_linear;
    uint64_t non_rotating_seek;
    uint64_t preferred_found;
    uint64_t preferred_not_found;
} zfs_vdev_mirrorstats_t;

void zfs_vdev_mirrorstats_refresh(zfs_vdev_mirrorstats_t *vdev_mirrorstats);
