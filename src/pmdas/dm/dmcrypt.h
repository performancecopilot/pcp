/*
 * Device Mapper PMDA - Crypt (dm-crypt) Stats
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

#ifndef DMCRYPT_H
#define DMCRYPT_H

enum {
    CRYPT_ACTIVE = 0,
    CRYPT_TYPE,
    CRYPT_CIPHER,
    CRYPT_KEYSIZE,
    CRYPT_KEY_LOCATION,
    CRYPT_DEVICE,
    CRYPT_SECTOR_SIZE,
    CRYPT_OFFSET,
    CRYPT_OFFSET_BYTES,
    CRYPT_SIZE,
    CRYPT_SIZE_BYTES,
    CRYPT_MODE,
    CRYPT_FLAGS,
    NUM_CRYPT_STATS
};

struct crypt_stats {
    __uint32_t active;
    char type[128];
    char cipher[256];
    __uint32_t keysize;
    char key_location[128];
    char device[PATH_MAX];
    __uint32_t sector_size;
    __uint64_t offset;
    __uint64_t offset_bytes;
    __uint64_t size;
    __uint64_t size_bytes;
    char mode[12];
    char flags[256];
};

extern int dm_crypt_fetch(int, struct crypt_stats *, pmAtomValue *);
extern int dm_refresh_crypt(const char *, struct crypt_stats *);
extern int dm_crypt_instance_refresh(void);
extern void dm_crypt_setup(void);

#endif /* DMCRYPT_H */
