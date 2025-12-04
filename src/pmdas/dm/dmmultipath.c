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

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "indom.h"
#include "dmmultipath.h"

#include <inttypes.h>
#include <ctype.h>

static char *dm_setup_dmmultipathd;

static int multipathd_found = 1;
static int multipath_maps_paths_found = 1;

/* multipathd command output is padded with a leading tab layout,
 * trim this out from the output to make comparisons easier.
 */
char 
*trim_whitespace(char* str)
{
    /* trim leading space */
    while (isspace((unsigned char)*str))
	str++;
    return str;
}

int
dm_multipath_info_fetch(int item, struct multipath_info *info, pmAtomValue *atom)
{
    if (item < 0 || item >= NUM_MULTIPATH_STATS)
	return PM_ERR_PMID;

    switch (item) {
        case MULTIPATH_NAME:
            atom->cp = info->name;
            break;

        case MULTIPATH_WWID:
            atom->cp = info->wwid;
            break;

        case MULTIPATH_DEVICE:
            atom->cp = info->device;
            break;

        case MULTIPATH_VENDOR:
            atom->cp = info->vendor;
            break;

        case MULTIPATH_PRODUCT_NAME:
            atom->cp = info->product_name;
            break;

        case MULTIPATH_SIZE:
            atom->cp = info->size;
            break;

        case MULTIPATH_FEATURES:
            atom->cp = info->features;
            break;

        case MULTIPATH_HARDWARE_HANDLER:
            atom->cp = info->hardware_handler;
            break;

        case MULTIPATH_PERMISSIONS:
            atom->cp = info->permissions;
            break;
    }
    return PMDA_FETCH_STATIC;
}

int
dm_multipath_path_fetch(int item, struct multipath_path *path, pmAtomValue *atom)
{
    if (item < 0 || item >= NUM_MULTIPATH_PATH_STATS)
	return PM_ERR_PMID;

    switch (item) {
        case MULTIPATH_PATH_SELECTOR_ALGORITHM:
            atom->cp = path->selector_algorithm;
            break;

        case MULTIPATH_PATH_PRIORITY:
            atom->ull = path->priority;
            break;

        case MULTIPATH_PATH_STATUS:
            atom->cp = path->status;
            break;
    }
    return PMDA_FETCH_STATIC;
}

int
dm_multipath_device_fetch(int item, struct multipath_device *dev, pmAtomValue *atom)
{
    if (item < 0 || item >= NUM_MULTIPATH_DEVICE_STATS)
	return PM_ERR_PMID;

    switch (item) {
        case MULTIPATH_DEVICE_BUS_ID:
            atom->cp = dev->bus_id;
            break;

        case MULTIPATH_DEVICE_DEV_NAME:
            atom->cp = dev->dev_name;
            break;

        case MULTIPATH_DEVICE_DEV_ID:
            atom->cp = dev->dev_id;
            break;

        case MULTIPATH_DEVICE_DEVICE_STATUS:
            atom->cp = dev->device_status;
            break;

        case MULTIPATH_DEVICE_PATH_STATUS:
            atom->cp = dev->path_status;
            break;

        case MULTIPATH_DEVICE_KERNEL_STATUS:
            atom->cp = dev->kernel_status;
            break;

    }
    return PMDA_FETCH_STATIC;
}

int
dm_multipath_instance_refresh(void)
{
    char info_name[128] = {'\0'}, path_name[128] = {'\0'}, dev_name[128];
    char device[128];
    char buffer[BUFSIZ];
    FILE *fp;
    
    int sts;
    
    pmInDom info_indom = dm_indom(DM_MULTI_INFO_INDOM);
    pmInDom path_indom = dm_indom(DM_MULTI_PATH_INDOM);
    pmInDom dev_indom = dm_indom(DM_MULTI_DEV_INDOM);
    
    pmdaCacheOp(info_indom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(path_indom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(dev_indom, PMDA_CACHE_INACTIVE);

    if (!multipathd_found || !multipath_maps_paths_found)
        return 0;

    pmsprintf(buffer, sizeof(buffer), "%s", dm_setup_dmmultipathd);
    if ((fp = popen(buffer, "r")) == NULL)
        return -oserror();

    int path_count = 0;

    struct multipath_info *info = {0};

    while (fgets(buffer, sizeof(buffer) -1, fp) != NULL) {
        char *trimmed = trim_whitespace(buffer);

        // mulitipathd not installed
        if (strstr(buffer, "command not found")) {
            pmNotifyErr(LOG_ERR, "%s: multipathd executable not found\n", 
                        __FUNCTION__);

            multipathd_found = 0;
            break;
        }

        // No entries found
        if (strstr(buffer, "multipath.conf does not exist")) {
            pmNotifyErr(LOG_WARNING, "%s: multipath.conf, no map/path configuration found\n", 
                            __FUNCTION__);
            
            multipath_maps_paths_found = 0;
            break;
        }

        if (trimmed[0] == '\0')
            // Skip empty lines
            continue;

        if (trimmed[0] != '`' && trimmed[0] != '|') {

            if (!strstr(trimmed, "size=")) { 
                // We are starting a new map this line should start with the
                // map alias and not include size/feature info
                path_count = 0;

                sscanf(trimmed, "%*s %*s %s", info_name);

                sts = pmdaCacheLookupName(info_indom, info_name, NULL, (void **)&info);
                if (sts == PM_ERR_INST || (sts >=0 && info == NULL )) {
                    info = calloc(1, sizeof(struct multipath_info));
                    if (info == NULL) {
                        __pmProcessPipeClose(fp);
                        return PM_ERR_AGAIN;
                    }
                }
                else if (sts < 0)
	            continue;

                sscanf(trimmed, "%s %s %s %255[^,],%s",
                    info->name,
                    info->wwid,
                    info->device,
                    info->vendor,
                    info->product_name);

                // Remove initial and trailing parenthesis from WWID entry
                memmove(info->wwid, info->wwid+1, strlen(info->wwid));
                info->wwid[strlen(info->wwid) - 1] = '\0';
            } else {
                // Second information line of current map, can collect and save
                // info indom

                if (info == NULL) {
                    break; // Something has gone wrong with part 1 of map allocation
                }

                sscanf(trimmed, "size=%s features='%255[^']' hwhandler='%127[^']' wp=%s",
                    info->size,
                    info->features,
                    info->hardware_handler,
                    info->permissions);

                pmdaCacheStore(info_indom, PMDA_CACHE_ADD, info_name, (void *)info);
            }
        }

        if (strstr(trimmed, "policy=")) {
            // We are on a new path group on the current map, can collect and
            // save path indom
            pmsprintf(path_name, sizeof(path_name), "%s:path_%d", info_name, path_count);

            struct multipath_path *path;

            sts = pmdaCacheLookupName(path_indom, path_name, NULL, (void **)&path);
            if (sts == PM_ERR_INST || (sts >=0 && path == NULL )) {
                path = calloc(1, sizeof(struct multipath_path));
                if (path == NULL) {
                    free(info);
                    __pmProcessPipeClose(fp);
                    return PM_ERR_AGAIN;
                }
            }
            else if (sts < 0)
	        continue;

            sscanf(trimmed, "%*s policy='%[^']' prio=%"SCNu64" status=%s",
                path->selector_algorithm,
                &path->priority,
                path->status);

            pmdaCacheStore(path_indom, PMDA_CACHE_ADD, path_name, (void *)path);

            //increment path each time until it is reset by a new map.
            path_count++;
        }
        
        if (trimmed[0] == '|' || trimmed[0] == '`') {
            // We are on a new device on the current path for the map, can
            // collect and save device indom

            if(strstr(trimmed, "policy="))
                continue; // We don't want path line information, that's above

            char *data_start = buffer + 4; // Skip leading ASCII art characters
            sscanf(data_start, "%*s %s", device);

            pmsprintf(dev_name, sizeof(dev_name), "%s:%s", path_name, device);

            struct multipath_device *dev;

            sts = pmdaCacheLookupName(dev_indom, dev_name, NULL, (void **)&dev);
            if (sts == PM_ERR_INST || (sts >=0 && dev == NULL )) {
                dev = calloc(1, sizeof(struct multipath_device));
                if (dev == NULL) {
                    free(info);
                    __pmProcessPipeClose(fp);
                    return PM_ERR_AGAIN;
                }
            }
            else if (sts < 0)
	        continue;

            sscanf(data_start, "%s %s %s %s %s %s",
                dev->bus_id,
                dev->dev_name,
                dev->dev_id,
                dev->device_status,
                dev->path_status,
                dev->kernel_status);

            pmdaCacheStore(dev_indom, PMDA_CACHE_ADD, dev_name, (void *)dev);
        }
    }
    
    pclose(fp);
    return 0; 
}

void
dm_multipath_setup(void)
{
    static char multipathd_command[] = "multipathd show topology 2>&1";
    char *env_command;

    /* allow override at startup for QA testing */
    if ((env_command = getenv("DM_SETUP_MULTIPATH")) != NULL)
        dm_setup_dmmultipathd = env_command;
    else
        dm_setup_dmmultipathd = multipathd_command;
}
