/*
 * CIFS Proc based stats
 *
 * Copyright (c) 2014,2018 Red Hat.
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

#include <inttypes.h>

static uint64_t global_data[NUM_GLOBAL_STATS];
unsigned int global_version_major;
unsigned int global_version_minor;
static char version[10] = "";

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
