/*
 * CIFS Proc based stats
 *
 * Copyright (c) 2014, 2025 Red Hat.
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

enum {
    DEBUG_MAX_BUFFER_SIZE = 0,
    DEBUG_ACTIVE_VFS_REQUESTS,
    NUM_GLOBAL_DEBUG_STATS
};

enum {
    DEBUG_SVR_CONNECTION_ID = 0,
    DEBUG_SVR_HOSTNAME,
    DEBUG_SVR_CLIENT_GUID,
    DEBUG_SVR_NUMBER_OF_CREDITS,
    DEBUG_SVR_SERVER_CAPABILITIES,
    DEBUG_SVR_TCP_STATUS,
    DEBUG_SVR_INSTANCE,
    DEBUG_SVR_LOCAL_USERS_TO_SERVER,
    DEBUG_SVR_SECURITY_MODE,
    DEBUG_SVR_REQUESTS_ON_WIRE,
    DEBUG_SVR_NET_NAMESPACE,
    DEBUG_SVR_SEND,
    DEBUG_SVR_MAX_REQUEST_WAIT,
    NUM_DEBUG_SVR_STATS
};

enum {
    DEBUG_SESSION_ADDR = 0,
    DEBUG_SESSION_USES,
    DEBUG_SESSION_CAPABILITY,
    DEBUG_SESSION_STATUS,
    DEBUG_SESSION_SECURITY_TYPE,
    DEBUG_SESSION_ID,
    DEBUG_SESSION_USER,
    DEBUG_SESSION_CRED_USER,
    NUM_DEBUG_SESSION_STATS
};

enum {
    DEBUG_SHARE_MOUNTS = 0,
    DEBUG_SHARE_DEVINFO,
    DEBUG_SHARE_ATTRIBUTES,
    DEBUG_SHARE_STATUS,
    DEBUG_SHARE_TYPE,
    DEBUG_SHARE_SERIAL_NUMBER,
    DEBUG_SHARE_TID,
    DEBUG_SHARE_MAXIMAL_ACCESS,
    NUM_DEBUG_SHARE_STATS,
};

struct fs_stats {
    __uint64_t values[NUM_FS_STATS];
};

struct debug_server_stats {
    char connection_id[11];
    char hostname[54];
    char client_guid[37];
    __uint64_t number_of_credits;
    char server_capabilities[11];
    __uint32_t tcp_status;
    __uint32_t instance;
    __uint32_t local_users_to_server;
    char security_mode[5];
    __uint32_t requests_on_wire;
    char net_namespace[256];
    __uint64_t send;
    __uint64_t max_request_wait;
};

struct debug_session_stats {
    char address[52];
    __uint32_t uses;
    char capability[11];
    __uint32_t status;
    char security_type[16];
    char id[11];
    __uint32_t user;
    __uint32_t cred_user;
};

struct debug_share_stats {
   __uint64_t mounts;
   char devinfo[11];
   char attributes[11];
   __uint32_t status;
   char type[256];
   char serial_number[11];
   char tid[11];
   char maximal_access[11];
};

extern int cifs_global_stats_fetch(int, pmAtomValue *);
extern int cifs_fs_stats_fetch(int, struct fs_stats *, pmAtomValue *);
extern int cifs_refresh_global_stats(const char *, const char *, const char *);
extern int cifs_refresh_fs_stats(const char *, const char *, const char *, struct fs_stats *);

extern int cifs_debug_stats_fetch(int, pmAtomValue *);

extern int cifs_debug_server_stats_fetch(int, struct debug_server_stats *,  pmAtomValue *);
extern int cifs_refresh_debug_server_stats(const char *, const char *, const char *, struct debug_server_stats *);

extern int cifs_debug_session_stats_fetch(int, struct debug_session_stats *, pmAtomValue *);
extern int cifs_refresh_debug_session_stats(const char *, const char *, const char *, struct debug_session_stats *);

extern int cifs_debug_share_stats_fetch(int, struct debug_share_stats *, pmAtomValue *);
extern int cifs_refresh_debug_share_stats(const char *, const char *, const char *, struct debug_share_stats *);

extern int cifs_refresh_debug_stats(const char *, const char *);

#endif /* STATS_H */
