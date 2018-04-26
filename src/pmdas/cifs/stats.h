/*
 * CIFS Proc based stats
 *
 * Copyright (c) 2014 Red Hat.
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

#ifndef STATS_H
#define STATS_H

enum {
    GLOBAL_SESSION = 0,
    GLOBAL_SHARES,
    GLOBAL_BUFFER,
    GLOBAL_POOL_SIZE,
    GLOBAL_SMALL_BUFFER,
    GLOBAL_SMALL_POOL_SIZE,
    GLOBAL_MID_OPS,
    GLOBAL_TOTAL_OPERATIONS,
    GLOBAL_TOTAL_RECONNECTS,
    GLOBAL_VFS_OPS,
    GLOBAL_VFS_OPS_MAX,
    GLOBAL_VERSION,
    NUM_GLOBAL_STATS
};

enum {
    FS_CONNECTED = 0,
    FS_SMBS,
    FS_OPLOCK_BREAKS,
    FS_READ,
    FS_READ_BYTES,
    FS_WRITE,
    FS_WRITE_BYTES,
    FS_FLUSHES,
    FS_LOCKS,
    FS_HARD_LINKS,
    FS_SYM_LINKS,
    FS_OPEN,
    FS_CLOSE,
    FS_DELETE,
    FS_POSIX_OPEN,
    FS_POSIX_MKDIR,
    FS_MKDIR,
    FS_RMDIR,
    FS_RENAME,
    FS_T2_RENAME,
    FS_FIND_FIRST,
    FS_FIND_NEXT,
    FS_FIND_CLOSE,
    FS_READ_FAILS,
    FS_WRITE_FAILS,
    FS_FLUSHES_FAILS,
    FS_LOCKS_FAILS,
    FS_CLOSE_FAILS,
    FS_NEGOTIATES,
    FS_NEGOTIATES_FAILS,
    FS_SESSIONSETUPS,
    FS_SESSIONSETUPS_FAILS,
    FS_LOGOFFS,
    FS_LOGOFFS_FAILS,
    FS_TREECONS,
    FS_TREECONS_FAILS,
    FS_TREEDISCONS,
    FS_TREEDISCONS_FAILS,
    FS_CREATES,
    FS_CREATES_FAILS,
    FS_IOCTLS,
    FS_IOCTLS_FAILS,
    FS_CANCELS,
    FS_CANCELS_FAILS,
    FS_ECHOS,
    FS_ECHOS_FAILS,
    FS_QUERYDIRS,
    FS_QUERYDIRS_FAILS,
    FS_CHANGENOTIFIES,
    FS_CHANGENOTIFIES_FAILS,
    FS_QUERYINFOS,
    FS_QUERYINFOS_FAILS,
    FS_SETINFOS,
    FS_SETINFOS_FAILS,
    FS_OPLOCK_BREAKS_FAILS,
    NUM_FS_STATS
};

struct fs_stats {
    __uint64_t values[NUM_FS_STATS];
};

extern int cifs_global_stats_fetch(int, pmAtomValue *);
extern int cifs_fs_stats_fetch(int, struct fs_stats *, pmAtomValue *);
extern int cifs_refresh_global_stats(const char *, const char *, const char *);
extern int cifs_refresh_fs_stats(const char *, const char *, const char *, struct fs_stats *);

#endif /* STATS_H */
