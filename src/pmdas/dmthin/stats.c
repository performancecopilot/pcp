/*
 * dm-thin stats derrived from dmsetup status
 *
 * Copyright (c) 2015 Red Hat.
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
#include "impl.h"
#include "pmda.h"

#include "stats.h"

#include <inttypes.h>

/*
 * Fetches the value for the given metric item and then assigns to pmAtomValue.
 * We check to see if item is in valid range for the metric.
 */
int
dmthin_pool_fetch(int item, struct pool_stats *pool_stats, pmAtomValue *atom)
{
    if (item < 0 || item >= NUM_POOL_STATS)
	return PM_ERR_PMID;

    switch(item) {
        case POOL_META_USED:
            atom->ull = pool_stats->meta_used;
            break;
        case POOL_META_TOTAL:
            atom->ull = pool_stats->meta_total;
            break;
        case POOL_DATA_USED:
            atom->ull = pool_stats->data_used;
            break;
        case POOL_DATA_TOTAL:
            atom->ull = pool_stats->data_total;
            break;
        case POOL_HELD_ROOT:
            atom->cp = pool_stats->held_root;
            break;
        case POOL_READ_MODE:
            atom->cp = pool_stats->read_mode;
            break;
        case POOL_DISCARD_PASSDOWN:
            atom->cp = pool_stats->discard_passdown;
            break;
        case POOL_NO_SPACE_MODE:
            atom->cp = pool_stats->no_space_mode;
            break;
    }     
    return 1;
}

/*
 * Fetches the value for the given metric item and then assigns to pmAtomValue.
 * We check to see if item is in valid range for the metric.
 */
int
dmthin_vol_fetch(int item, struct vol_stats *vol_stats, pmAtomValue *atom)
{
    if (item < 0 || item >= NUM_VOL_STATS)
	return PM_ERR_PMID;

    switch(item) {
        case VOL_NUM_MAPPED_SECTORS:
            atom->ull = vol_stats->num_mapped_sectors;
            break;
        case VOL_HIGHEST_MAPPED_SECTORS:
            atom->ull = vol_stats->high_mapped_sector;
            break;
    }     
    return 1;
}

/* 
 * Grab output from dmsetup status (or read in from testfile under QA),
 * Match the data to the pool which we wish to update the metrics and
 * assign the values to pool_stats. 
 */
int
dmthin_refresh_pool(const int _isQA, const char *statspath, const char *pool_name, struct pool_stats *pool_stats){
    char buffer[PATH_MAX], *token;
    FILE *fp;

    /* _isQA is set if statspath has been set during pmda init */
    if (_isQA) {
        snprintf(buffer, sizeof(buffer), "%s/dmthin-pool", statspath);
        buffer[sizeof(buffer)-1] = '\0';

        if ((fp = fopen(buffer, "r")) == NULL )
            return -oserror();

    } else {
        if ((fp = popen("dmsetup status --target thin-pool", "r")) == NULL)
            return -oserror();
    }

    while (fgets(buffer, sizeof(buffer) -1, fp)) {
        if (!strstr(buffer, ":") || strstr(buffer, "Fail"))
            continue;

        token = strtok(buffer, ":");

        if (strcmp(token, pool_name) == 0) {
            token = strtok(NULL, ":");

            /* Pattern match our output to the given thin-pool status
             * output (minus pool name). 
             * The format is:
             * <name>:<start> <end> <target>
             *     <transaction id> <used metadata blocks>/<total metadata blocks>
             *     <used data blocks>/<total data blocks> <held metadata root>
             *     ro|rw [no_]discard_passdown  [error|queue]_if_no_space
             */
            sscanf(token, " %*d %*d thin-pool %*d %"SCNu64"/%"SCNu64" %"SCNu64"/%"SCNu64" %s %s %s %s",
                &pool_stats->meta_used,
                &pool_stats->meta_total,
                &pool_stats->data_used,
                &pool_stats->data_total,
                pool_stats->held_root,
                pool_stats->read_mode,
                pool_stats->discard_passdown,
                pool_stats->no_space_mode
            );
        }
    }

    /* Close process (or file if _isQA) */
    if (_isQA) {
        fclose(fp);
    } else {
        if (pclose(fp) != 0)
            return -oserror(); 
    }
    return 0;
}

/* 
 * Grab output from dmsetup status (or read in from testfile under QA),
 * Match the data to the volume which we wish to update the metrics and
 * assign the values to vol_stats. 
 */
int
dmthin_refresh_vol(const int _isQA, const char *statspath, const char *vol_name, struct vol_stats *vol_stats){
    char buffer[PATH_MAX], *token;
    FILE *fp;

    /* _isQA is set if statspath has been set during pmda init */
    if (_isQA) {
        snprintf(buffer, sizeof(buffer), "%s/dmthin-thin", statspath);
        buffer[sizeof(buffer)-1] = '\0';

        if ((fp = fopen(buffer, "r")) == NULL )
            return -oserror();

    } else {
        if ((fp = popen("dmsetup status --target thin", "r")) == NULL)
            return -oserror();
    }

    while (fgets(buffer, sizeof(buffer) -1, fp)) {
        if (!strstr(buffer, ":") || strstr(buffer, "Fail"))
            continue;

        token = strtok(buffer, ":");

        if (strcmp(token, vol_name) == 0) {
            token = strtok(NULL, ":");

            /* Pattern match our output to the given thin-volume status
             * output (minus volume name). 
             * The format is:
             * <name>:<start> <end> <target>
             *     <nr mapped sectors> <highest mapped sector>
             */
            sscanf(token, " %*d %*d thin %"SCNu64" %"SCNu64"",
                &vol_stats->num_mapped_sectors,
                &vol_stats->high_mapped_sector
            );
        }
    }

    /* Close process (or file if _isQA) */
    if (_isQA) {
        fclose(fp);
    } else {
        if (pclose(fp) != 0)
            return -oserror(); 
    }
    return 0;
}
