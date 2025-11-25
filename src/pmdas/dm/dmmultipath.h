/*
 * Device Mapper PMDA - Multipath (dm-multipath) Stats
 *
 * Copyright (c) 2025 Red Hat.
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

#ifndef DMMULTIPATH_H
#define DMMULTIPATH_H

enum {
    MULTIPATH_NAME = 0,
    MULTIPATH_WWID,
    MULTIPATH_DEVICE,
    MULTIPATH_VENDOR,
    MULTIPATH_PRODUCT_NAME,
    MULTIPATH_SIZE,
    MULTIPATH_FEATURES,
    MULTIPATH_HARDWARE_HANDLER,
    MULTIPATH_PERMISSIONS,
    NUM_MULTIPATH_STATS
};

enum {
    MULTIPATH_PATH_SELECTOR_ALGORITHM = 0,
    MULTIPATH_PATH_PRIORITY,
    MULTIPATH_PATH_STATUS,
    NUM_MULTIPATH_PATH_STATS
};

enum {
    MULTIPATH_DEVICE_BUS_ID = 0,
    MULTIPATH_DEVICE_DEV_NAME,
    MULTIPATH_DEVICE_DEV_ID,
    MULTIPATH_DEVICE_DEVICE_STATUS,
    MULTIPATH_DEVICE_PATH_STATUS,
    MULTIPATH_DEVICE_KERNEL_STATUS,
    NUM_MULTIPATH_DEVICE_STATS
};

struct multipath_info {
    char name[256];
    char wwid[37];
    char device[11];
    char vendor[256];
    char product_name[256];
    char size[128];
    char features[256];
    char hardware_handler[128];
    char permissions [8];
};

struct multipath_path {
    char selector_algorithm[128];
    uint64_t priority;
    char status[10];
};

struct multipath_device {
    char bus_id[256];
    char dev_name[256];
    char dev_id[12];
    char device_status[7];
    char path_status[12];
    char kernel_status[10];

};

extern int dm_multipath_info_fetch(int, struct multipath_info *, pmAtomValue *);
extern int dm_multipath_path_fetch(int, struct multipath_path *, pmAtomValue *);
extern int dm_multipath_device_fetch(int, struct multipath_device *, pmAtomValue *);

extern int dm_multipath_instance_refresh(void);

extern void dm_multipath_setup(void);

#endif /* DMMULTIPATH_H */
