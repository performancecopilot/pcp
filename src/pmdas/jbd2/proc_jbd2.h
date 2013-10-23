/*
 * Linux JBD2 (ext3/ext4) driver metrics.
 *
 * Copyright (C) 2013 Red Hat.
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

typedef struct {
    __uint32_t version;
    __uint32_t max_buffers;

    __uint64_t tid;
    __uint64_t requested;

    __uint64_t waiting;
    __uint64_t request_delay;
    __uint64_t running;
    __uint64_t locked;
    __uint64_t flushing;
    __uint64_t logging;
    __uint64_t average_commit_time;

    __uint64_t handles;

    __uint64_t blocks;
    __uint64_t blocks_logged;
} proc_jbd2_t;

extern int refresh_jbd2(const char *path, pmInDom jbd2_indom);
