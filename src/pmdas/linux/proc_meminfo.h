/*
 * Linux /proc/meminfo metrics cluster
 *
 * Copyright (c) 2002 Silicon Graphics, Inc.  All Rights Reserved.
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

/*
 * All fields in /proc/meminfo
 */
typedef struct {
    int64_t MemTotal;
    int64_t MemFree;
    int64_t MemShared;
    int64_t Buffers;
    int64_t Cached;
    int64_t SwapCached;
    int64_t Active;
    int64_t Inactive;
    int64_t HighTotal;
    int64_t HighFree;
    int64_t LowTotal;
    int64_t LowFree;
    int64_t SwapTotal;
    int64_t SwapFree;
    int64_t SwapUsed; /* computed */
    int64_t Dirty;
    int64_t Writeback;
    int64_t Mapped;
    int64_t Slab;
    int64_t CommitLimit;
    int64_t Committed_AS;
    int64_t PageTables;
    int64_t ReverseMaps;
    int64_t AnonPages;
    int64_t Bounce;
    int64_t NFS_Unstable;
    int64_t SlabReclaimable;
    int64_t SlabUnreclaimable;
} proc_meminfo_t;

extern int refresh_proc_meminfo(proc_meminfo_t *);
