/*
 * CIFS Proc based stats
 *
 * Copyright (c) 2014,2018, 2025 Red Hat.
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

#include "pmapi.h"
#include "pmda.h"

#include "stats.h"
#include "pmdacifs.h"

#include <inttypes.h>
#include <ctype.h>

static uint64_t global_data[NUM_GLOBAL_STATS];
unsigned int global_version_major;
unsigned int global_version_minor;
static char version[10] = "";

static uint64_t global_debug_data[NUM_GLOBAL_DEBUG_STATS];

int
cifs_global_stats_fetch(int item, pmAtomValue *atom)
{

    /* check for bounds */
    if (item < 0 || item >= NUM_GLOBAL_STATS)
        return PMDA_FETCH_NOVALUES;

    /* if cifs kernel module not loaded array will be UNIT64_MAX */
    if (global_data[item] == UINT64_MAX)
        return PMDA_FETCH_NOVALUES;

    switch(item) {
    case GLOBAL_VERSION:
	pmsprintf(version, sizeof(version), "%u.%u", global_version_major, global_version_minor);
        atom->cp = version;
	break;
    default:
	atom->ull = global_data[item];
	break;
    }
    return PMDA_FETCH_STATIC;
}

int
cifs_fs_stats_fetch(int item, struct fs_stats *fs_stats, pmAtomValue *atom)
{

    /* check for bounds */
    if (item < 0 || item >= NUM_FS_STATS)
        return PMDA_FETCH_NOVALUES;

    if (global_version_major >= 2 &&
	item < FS_READ_FAILS &&
	item != FS_READ &&
	item != FS_WRITE &&
	item != FS_FLUSHES &&
	item != FS_LOCKS &&
	item != FS_CLOSE &&
	item != FS_SMBS &&
	item != FS_OPLOCK_BREAKS) // requested item is below
	return PM_ERR_APPVERSION;
    if (global_version_major < 2 && item >= FS_READ_FAILS)
	return PM_ERR_APPVERSION;

    atom->ull = fs_stats->values[item];
    return PMDA_FETCH_STATIC;
}

int
cifs_debug_stats_fetch(int item, pmAtomValue *atom)
{

    /* check for bounds */
    if (item < 0 || item >= NUM_GLOBAL_DEBUG_STATS)
        return PMDA_FETCH_NOVALUES;

    /* if cifs kernel module not loaded array will be UNIT64_MAX */
    if (global_data[item] == UINT64_MAX)
        return PMDA_FETCH_NOVALUES;

    switch(item) {
    default:
	atom->ull = global_debug_data[item];
	break;
    }
    return PMDA_FETCH_STATIC;
}

int
cifs_debug_server_stats_fetch(int item, struct debug_server_stats *server_stats,  pmAtomValue *atom)
{

    /* check for bounds */
    if (item < 0 || item >= NUM_DEBUG_SVR_STATS)
        return PMDA_FETCH_NOVALUES;

    switch (item) {
    
        case DEBUG_SVR_CONNECTION_ID:
            atom->cp = server_stats->connection_id;
            return PMDA_FETCH_STATIC;

        case DEBUG_SVR_HOSTNAME:
            atom->cp = server_stats->hostname;
            return PMDA_FETCH_STATIC;

        case DEBUG_SVR_CLIENT_GUID:
            atom->cp = server_stats->client_guid;
            return PMDA_FETCH_STATIC;

        case DEBUG_SVR_NUMBER_OF_CREDITS:
            atom->ull = server_stats->number_of_credits;
            return PMDA_FETCH_STATIC;

        case DEBUG_SVR_SERVER_CAPABILITIES:
            atom->cp = server_stats->server_capabilities;
            return PMDA_FETCH_STATIC;

        case DEBUG_SVR_TCP_STATUS:
            atom->ul = server_stats->tcp_status;
            return PMDA_FETCH_STATIC;

        case DEBUG_SVR_INSTANCE:
            atom->ul = server_stats->instance;
            return PMDA_FETCH_STATIC;

        case DEBUG_SVR_LOCAL_USERS_TO_SERVER:
            atom->ul = server_stats->local_users_to_server;
            return PMDA_FETCH_STATIC;

        case DEBUG_SVR_SECURITY_MODE:
            atom->cp = server_stats->security_mode;
            return PMDA_FETCH_STATIC;

        case DEBUG_SVR_REQUESTS_ON_WIRE:
            atom->ul = server_stats->requests_on_wire;
            return PMDA_FETCH_STATIC;

        case DEBUG_SVR_NET_NAMESPACE:
            atom->cp = server_stats->net_namespace;
            return PMDA_FETCH_STATIC;

        case DEBUG_SVR_SEND:
            atom->ull = server_stats->send;
            return PMDA_FETCH_STATIC;

        case DEBUG_SVR_MAX_REQUEST_WAIT:
            atom->ull = server_stats->max_request_wait;
            return PMDA_FETCH_STATIC;
    }
    return PMDA_FETCH_NOVALUES;
}

int
cifs_debug_session_stats_fetch(int item, struct debug_session_stats *session_stats, pmAtomValue *atom)
{

    /* check for bounds */
    if (item < 0 || item >= NUM_DEBUG_SESSION_STATS)
        return PMDA_FETCH_NOVALUES;

    switch (item) {
    
        case DEBUG_SESSION_ADDR:
            atom->cp = session_stats->address;
            return PMDA_FETCH_STATIC;

        case DEBUG_SESSION_USES:
            atom->ul = session_stats->uses;
            return PMDA_FETCH_STATIC;

        case DEBUG_SESSION_CAPABILITY:
            atom->cp = session_stats->capability;
            return PMDA_FETCH_STATIC;

        case DEBUG_SESSION_STATUS:
            atom->ul = session_stats->status;
            return PMDA_FETCH_STATIC;

        case DEBUG_SESSION_SECURITY_TYPE:
            atom->cp = session_stats->security_type;
            return PMDA_FETCH_STATIC;

        case DEBUG_SESSION_ID:
            atom->cp = session_stats->id;
            return PMDA_FETCH_STATIC;

        case DEBUG_SESSION_USER:
            atom->ul = session_stats->user;
            return PMDA_FETCH_STATIC;

        case DEBUG_SESSION_CRED_USER:
            atom->ul = session_stats->cred_user;
            return PMDA_FETCH_STATIC;
    }
    return PMDA_FETCH_NOVALUES;
}


int cifs_debug_share_stats_fetch(int item, struct debug_share_stats *share_stats, pmAtomValue *atom)
{

    /* check for bounds */
    if (item < 0 || item >= NUM_DEBUG_SHARE_STATS)
        return PMDA_FETCH_NOVALUES;

    switch (item) {
    
        case DEBUG_SHARE_MOUNTS:
            atom->ull = share_stats->mounts;
            return PMDA_FETCH_STATIC;

        case DEBUG_SHARE_DEVINFO:
            atom->cp = share_stats->devinfo;
            return PMDA_FETCH_STATIC;

        case DEBUG_SHARE_ATTRIBUTES:
            atom->cp = share_stats->attributes;
            return PMDA_FETCH_STATIC;

        case DEBUG_SHARE_STATUS:
            atom->ul = share_stats->status;
            return PMDA_FETCH_STATIC;

        case DEBUG_SHARE_TYPE:
            atom->cp = share_stats->type;
            return PMDA_FETCH_STATIC;

        case DEBUG_SHARE_SERIAL_NUMBER:
            atom->cp = share_stats->serial_number;
            return PMDA_FETCH_STATIC;

        case DEBUG_SHARE_TID:
            atom->cp = share_stats->tid;
            return PMDA_FETCH_STATIC;

        case DEBUG_SHARE_MAXIMAL_ACCESS:
            atom->cp = share_stats->maximal_access;
            return PMDA_FETCH_STATIC;
    }
    return PMDA_FETCH_NOVALUES;
}

int
cifs_refresh_global_stats(const char *statspath, const char *procfsdir, const char *name){
    char buffer[PATH_MAX];
    FILE *fp;

    /* set counters, UINT64_MAX we can check later if we have results to return */
    memset(global_data, -1, sizeof global_data);

        pmsprintf(buffer, sizeof(buffer), "%s%s/Stats", statspath, procfsdir);
    buffer[sizeof(buffer)-1] = '\0';

    if ((fp = fopen(buffer, "r")) == NULL )
        return -oserror();

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        /* global cifs stats */
        if (strncmp(buffer, "CIFS Session:", 13) == 0)
            sscanf(buffer, "%*s %*s %"SCNu64"",
                &global_data[GLOBAL_SESSION]
            );
        if (strncmp(buffer, "Share (unique mount targets):", 29) == 0)
            sscanf(buffer, "%*s %*s %*s %*s %"SCNu64"",
                &global_data[GLOBAL_SHARES]
            );
        if (strncmp(buffer, "SMB Request/Response Buffer:", 28) == 0)
            sscanf(buffer, "%*s %*s %*s %"SCNu64" %*s %*s %"SCNu64"",
                &global_data[GLOBAL_BUFFER],
                &global_data[GLOBAL_POOL_SIZE]
            );
        if (strncmp(buffer, "SMB Small Req/Resp Buffer:", 26) == 0)
            sscanf(buffer, "%*s %*s %*s %*s %"SCNu64" %*s %*s %"SCNu64"",
                &global_data[GLOBAL_SMALL_BUFFER],
                &global_data[GLOBAL_SMALL_POOL_SIZE]
            );
        if (strncmp(buffer, "Operations (MIDs)", 17) == 0)
            sscanf(buffer, "%*s %*s %"SCNu64"",
                &global_data[GLOBAL_MID_OPS]
            );
        if (strstr(buffer, "share reconnects"))
            sscanf(buffer, "%"SCNu64" %*s %"SCNu64" %*s %*s",
                &global_data[GLOBAL_TOTAL_OPERATIONS],
                &global_data[GLOBAL_TOTAL_RECONNECTS]
            );
        if (strncmp(buffer, "Total vfs operations:", 21) == 0)
            sscanf(buffer, "%*s %*s %*s %"SCNu64" %*s %*s %*s %*s %"SCNu64"",
                &global_data[GLOBAL_VFS_OPS],
                &global_data[GLOBAL_VFS_OPS_MAX]
            );
        if (strstr(buffer, "\\\\"))
            break;
    }
    fclose(fp);

    global_data[GLOBAL_VERSION] = (uint64_t) global_version_major;
    return 0;
}

int
cifs_refresh_fs_stats(const char *statspath, const char *procfsdir, const char *name, struct fs_stats *fs_stats){
    char buffer[PATH_MAX], cifs_name[256];
    char cifs_connected[13] = {0};
    int found_fs = 0;
    FILE *fp;

        pmsprintf(buffer, sizeof(buffer), "%s%s/Stats", statspath, procfsdir);
    buffer[sizeof(buffer)-1] = '\0';

    if ((fp = fopen(buffer, "r")) == NULL )
        return -oserror();

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        /* match filesystem line to our filesystem that we want metrics for */
        if (strstr(buffer, "\\\\")) {
            if (found_fs)
		break;

            sscanf(buffer, "%*d%*s %s %s", cifs_name, cifs_connected);
            if (strcmp(name, cifs_name) == 0)
                found_fs = 1;
        }

        if (found_fs) {
            /* per fs cifs stats */
            if (strncmp(cifs_connected, "DISCONNECTED", 12) == 0){
                fs_stats->values[FS_CONNECTED] = 0;
            } else {
                fs_stats->values[FS_CONNECTED] = 1;
            }
            if (strncmp(buffer, "SMBs:", 4) == 0) {
		if (global_version_major < 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %*s %"SCNu64"",
			   &fs_stats->values[FS_SMBS],
			   &fs_stats->values[FS_OPLOCK_BREAKS]
			   );
	    }
            if (strncmp(buffer, "Reads:", 6) == 0) {
		if (global_version_major < 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64"",
			   &fs_stats->values[FS_READ],
			   &fs_stats->values[FS_READ_BYTES]
		    );
		else
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_READ],
			   &fs_stats->values[FS_READ_FAILS]);
	    }
            if (strncmp(buffer, "Writes:", 7) == 0) {
		if (global_version_major < 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64"",
			   &fs_stats->values[FS_WRITE],
			   &fs_stats->values[FS_WRITE_BYTES]
			   );
		else
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64"",
			   &fs_stats->values[FS_WRITE],
			   &fs_stats->values[FS_WRITE_FAILS]
			   );
	    }
            if (strncmp(buffer, "Flushes:", 8) == 0) {
		if (global_version_major < 2)
		    sscanf(buffer, "%*s %"SCNu64"",
			   &fs_stats->values[FS_FLUSHES]
			   );
		else
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64"",
			   &fs_stats->values[FS_FLUSHES],
			   &fs_stats->values[FS_FLUSHES_FAILS]
			   );
	    }
            if (strncmp(buffer, "Locks:", 6) == 0) {
		if (global_version_major < 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s %"SCNu64"",
			   &fs_stats->values[FS_LOCKS],
			   &fs_stats->values[FS_HARD_LINKS],
			   &fs_stats->values[FS_SYM_LINKS]
			   );
		else
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64"",
			   &fs_stats->values[FS_LOCKS],
			   &fs_stats->values[FS_LOCKS_FAILS]);
	    }
            if (strncmp(buffer, "Opens:", 6) == 0)
		if (global_version_major < 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s %"SCNu64"",
			   &fs_stats->values[FS_OPEN],
			   &fs_stats->values[FS_CLOSE],
			   &fs_stats->values[FS_DELETE]
			   );
            if (strncmp(buffer, "Posix Opens:", 12) == 0)
                sscanf(buffer, "%*s %*s %"SCNu64" %*s %*s %"SCNu64"",
                    &fs_stats->values[FS_POSIX_OPEN],
                    &fs_stats->values[FS_POSIX_MKDIR]
                );
            if (strncmp(buffer, "Mkdirs:", 7) == 0)
                sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64"",
                    &fs_stats->values[FS_MKDIR],
                    &fs_stats->values[FS_RMDIR]
                );
            if (strncmp(buffer, "Renames:", 8) == 0)
                sscanf(buffer, "%*s %"SCNu64" %*s %*s %"SCNu64"",
                    &fs_stats->values[FS_RENAME],
                    &fs_stats->values[FS_T2_RENAME]
                );
            if (strncmp(buffer, "FindFirst:", 10) == 0)
                sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s %"SCNu64"",
                    &fs_stats->values[FS_FIND_FIRST],
                    &fs_stats->values[FS_FIND_NEXT],
                    &fs_stats->values[FS_FIND_CLOSE]
                );
	    if (strncmp(buffer, "Negotiates:", 11) == 0)
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_NEGOTIATES],
			   &fs_stats->values[FS_NEGOTIATES_FAILS]);
	    if (strncmp(buffer, "SessionSetups:", 14) == 0)
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_SESSIONSETUPS],
			   &fs_stats->values[FS_SESSIONSETUPS_FAILS]);
	    if (strncmp(buffer, "Logoffs:", 8) == 0)
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_LOGOFFS],
			   &fs_stats->values[FS_LOGOFFS_FAILS]);
	    if (strncmp(buffer, "TreeConnects:", 6) == 0)
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_TREECONS],
			   &fs_stats->values[FS_TREECONS_FAILS]);
	    if (strncmp(buffer, "TreeDisconnects:", 16) == 0)
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_TREEDISCONS],
			   &fs_stats->values[FS_TREEDISCONS_FAILS]);
	    if (strncmp(buffer, "Creates:", 8) == 0)
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_CREATES],
			   &fs_stats->values[FS_CREATES_FAILS]);
	    if (strncmp(buffer, "IOCTLs:", 7) == 0){
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_IOCTLS],
			   &fs_stats->values[FS_IOCTLS_FAILS]);
	    }
	    if (strncmp(buffer, "Cancels:", 8) == 0)
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_CANCELS],
			   &fs_stats->values[FS_CANCELS_FAILS]);
	    if (strncmp(buffer, "Echos:", 6) == 0)
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_ECHOS],
			   &fs_stats->values[FS_ECHOS_FAILS]);
	    if (strncmp(buffer, "QueryDirectories:", 17) == 0)
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_QUERYDIRS],
			   &fs_stats->values[FS_QUERYDIRS_FAILS]);
	    if (strncmp(buffer, "ChangeNotifies:", 15) == 0)
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_CHANGENOTIFIES],
			   &fs_stats->values[FS_CHANGENOTIFIES_FAILS]);
	    if (strncmp(buffer, "QueryInfos:", 11) == 0)
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_QUERYINFOS],
			   &fs_stats->values[FS_QUERYINFOS_FAILS]);
	    if (strncmp(buffer, "SetInfos:", 9) == 0)
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_SETINFOS],
			   &fs_stats->values[FS_SETINFOS_FAILS]);
	    if (strncmp(buffer, "Closes:", 7) == 0)
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_CLOSE],
			   &fs_stats->values[FS_CLOSE_FAILS]);
	    if (strncmp(buffer, "OplockBreaks:", 13) == 0)
		if (global_version_major >= 2)
		    sscanf(buffer, "%*s %"SCNu64" %*s %"SCNu64" %*s",
			   &fs_stats->values[FS_OPLOCK_BREAKS],
			   &fs_stats->values[FS_OPLOCK_BREAKS_FAILS]);

        }
    }
    fclose(fp);
    return 0;
}

int cifs_refresh_debug_stats(const char *statspath, const char *procfsdir)
{
    char buffer[PATH_MAX];
    FILE *fp;

    /* set counters, UINT64_MAX we can check later if we have results to return */
    memset(global_debug_data, -1, sizeof global_debug_data);

    pmsprintf(buffer, sizeof(buffer), "%s%s/DebugData", statspath, procfsdir);
    buffer[sizeof(buffer)-1] = '\0';

    if ((fp = fopen(buffer, "r")) == NULL )
        return -oserror();

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {

        /* Collect Global Debug Stats */
        if (strncmp(buffer, "CIFSMaxBufSize:", 15) == 0)
            sscanf(buffer, "%*s %"SCNu64"", &global_debug_data[DEBUG_MAX_BUFFER_SIZE]);

        if (strncmp(buffer, "Active VFS Requests:", 20) == 0)
            sscanf(buffer, "%*s %*s %*s %"SCNu64"",
                &global_debug_data[DEBUG_ACTIVE_VFS_REQUESTS]);
    }
    fclose(fp);

    return 0;
};

int
cifs_refresh_debug_server_stats(const char *statspath, const char *procfsdir, const char *name, struct debug_server_stats *server_stats)
{
    char buffer[PATH_MAX];
    FILE *fp;
    int found_instance = 0;

    pmsprintf(buffer, sizeof(buffer), "%s%s/DebugData", statspath, procfsdir);
    buffer[sizeof(buffer)-1] = '\0';

    if ((fp = fopen(buffer, "r")) == NULL )
        return -oserror();

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {

        if (strstr(buffer, "ConnectionId:") != NULL) {  
            if (strstr(buffer, name) != NULL) {
                found_instance = 1;
            
                sscanf(buffer, "%*d) %*s %s %*s %s",
                    server_stats->connection_id,
                    server_stats->hostname);
            }
        }

        if (found_instance) {
            if (strncmp(buffer, "ClientGUID:", 11) == 0)
                sscanf(buffer, "%*s %s",
                    server_stats->client_guid);

            if (strncmp(buffer, "Number of credits:", 18) == 0)
                sscanf(buffer, "%*s %*s %*s %"SCNu64"",
                    &server_stats->number_of_credits);

            if (strncmp(buffer, "Server capabilities:", 20) == 0)
                sscanf(buffer, "%*s %*s %s",
                    server_stats->server_capabilities);

            if (strncmp(buffer, "TCP status:", 11) == 0)
                sscanf(buffer, "%*s %*s %"SCNu32" %*s %"SCNu32"",
                    &server_stats->tcp_status,
                    &server_stats->instance);

            if (strncmp(buffer, "Local Users To Server:", 22) == 0)
                sscanf(buffer, "%*s %*s %*s %*s %"SCNu32" %*s %s %*s %*s %*s %"SCNu32" %*s %*s %s",
                    &server_stats->local_users_to_server,
                    server_stats->security_mode,
                    &server_stats->requests_on_wire,
                    server_stats->net_namespace);

            if (strncmp(buffer, "In Send:", 8) == 0) {
                sscanf(buffer, "%*s %*s %"SCNu64" %*s %*s %*s %"SCNu64"",
                    &server_stats->send,
                    &server_stats->max_request_wait);

            /* We need to break out when we have completed collecting metrics for
               our current instance otherwise we wrap over and merge later share
               entries in the DebugData output */
            break;
            }
        }
    }
    fclose(fp);

    return 0;
}

int
cifs_refresh_debug_session_stats(const char *statspath, const char *procfsdir, const char *name, struct debug_session_stats *session_stats)
{
    char buffer[PATH_MAX];
    FILE *fp;
    int found_instance = 0;

    pmsprintf(buffer, sizeof(buffer), "%s%s/DebugData", statspath, procfsdir);
    buffer[sizeof(buffer)-1] = '\0';

    if ((fp = fopen(buffer, "r")) == NULL )
        return -oserror();

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {

        if (strstr(buffer, ") Address:") != NULL) {
            char instance_name[PATH_MAX];
            strncpy(instance_name, name, sizeof(instance_name)-1);

            if (strstr(buffer, strtok(instance_name, ":")) != NULL) {
                found_instance = 1;

                sscanf(strtrim(buffer), "%*d) %*s %s %*s %"SCNu32" %*s %s %*s %*s %"SCNu32"",
                    session_stats->address,
                    &session_stats->uses,
                    session_stats->capability,
                    &session_stats->status);
            }
        }

        if (found_instance) {
            if (strncmp(strtrim(buffer), "Security type:", 14) == 0)
                sscanf(strtrim(buffer), "%*s %*s %s %*s %s",
                    session_stats->security_type,
                    session_stats->id);

            if (strncmp(strtrim(buffer), "User:", 5) == 0) {
                sscanf(strtrim(buffer), "%*s %"SCNu32", %*s %*s %"SCNu32"",
                    &session_stats->user,
                    &session_stats->cred_user);

            /* We need to break out when we have completed collecting metrics for
               our current instance otherwise we wrap over and merge later share
               entries in the DebugData output */
            break;
            }
        }
    }
    fclose(fp);

    return 0;
}

int
cifs_refresh_debug_share_stats(const char *statspath, const char *procfsdir, const char *name, struct debug_share_stats *share_stats)
{
    char buffer[PATH_MAX];
    FILE *fp;
    int found_instance = 0;

    pmsprintf(buffer, sizeof(buffer), "%s%s/DebugData", statspath, procfsdir);
    buffer[sizeof(buffer)-1] = '\0';

    if ((fp = fopen(buffer, "r")) == NULL )
        return -oserror();

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (strstr(buffer, "DevInfo:") != NULL) {
            if (strstr(buffer, name) != NULL) {
                found_instance = 1;

                if (strstr(buffer, "IPC") != NULL) {
                    sscanf(strtrim(buffer), "%*d) IPC: %*s %*s %"SCNu64" %*s %s %*s %s",
                        &share_stats->mounts,
                        share_stats->devinfo,
                        share_stats->attributes);
                } else {
                    sscanf(strtrim(buffer), "%*d) %*s %*s %"SCNu64" %*s %s %*s %s",
                        &share_stats->mounts,
                        share_stats->devinfo,
                        share_stats->attributes);
                }
            }
        }
        
        if (found_instance) {
            if (strncmp(strtrim(buffer), "PathComponentMax:", 17) == 0)
                sscanf(strtrim(buffer), "%*s %*d %*s %"SCNu32" %*s %s %*s %*s %s",
                    &share_stats->status,
                    share_stats->type,
                    share_stats->serial_number);

            if (strncmp(strtrim(buffer), "tid:", 4) == 0) {
                if (strstr(buffer, "Optimal sector size:") != NULL) {
                    sscanf(strtrim(buffer), "%*s %s %*s %*s %*s %*s %*s %*s %s",
                        share_stats->tid,
                        share_stats->maximal_access);            
                } else {
                    sscanf(strtrim(buffer), "%*s %s %*s %*s %s",
                        share_stats->tid,
                        share_stats->maximal_access);
                }

            /* We need to break out when we have completed collecting metrics for
               our current instance otherwise we wrap over and merge later share
               entries in the DebugData output */
            break;
            }
        }
    }
    fclose(fp);

    return 0;
}
