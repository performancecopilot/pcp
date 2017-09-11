/*
 * Copyright (c) 2016-2017 Fujitsu.
 * Copyright (c) 2017 Red Hat.
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
#include <dirent.h>
#include "linux.h"
#include "ksm.h"

int
refresh_ksm_info(ksm_info_t *ksm_info)
{
    int sts = 0;
    DIR	*ksm_dir;
    FILE *fp;
    struct dirent *de;
    char buffer[BUFSIZ];
    char path[MAXPATHLEN];

    pmsprintf(path, sizeof(path), "%s/sys/kernel/mm/ksm", linux_statspath);
    path[sizeof(path)-1] = '\0';
    if ((ksm_dir = opendir(path)) == NULL)
	return -oserror();

    while ((de = readdir(ksm_dir)) != NULL) {   
	if (!strncmp(de->d_name, ".", 1))
	    continue;

	pmsprintf(path, sizeof(path), "%s/sys/kernel/mm/ksm/%s", linux_statspath, de->d_name);
	path[sizeof(path)-1] = '\0';
	if ((fp = fopen(path, "r")) == NULL) {
	    sts = -oserror();
	    break;
	}

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
	    if (!strncmp(de->d_name, "full_scans", 10)) {
	       ksm_info->full_scans = strtoul(buffer, NULL, 10);
	       break;
	    }
	    else if (!strncmp(de->d_name, "merge_across_nodes", 18)) {
	       ksm_info->merge_across_nodes = strtoul(buffer, NULL, 10);
	       break;
	    }
	    else if (!strncmp(de->d_name, "pages_shared", 12)) {
	       ksm_info->pages_shared = strtoul(buffer, NULL, 10);
	       break;
	    }
	    else if (!strncmp(de->d_name, "pages_sharing", 13)) {
	       ksm_info->pages_sharing = strtoul(buffer, NULL, 10);
	       break;
	    }
	    else if (!strncmp(de->d_name, "pages_to_scan", 13)) {
	       ksm_info->pages_to_scan = strtoul(buffer, NULL, 10);
	       break;
	    }
	    else if (!strncmp(de->d_name, "pages_unshared", 14)) {
	       ksm_info->pages_unshared = strtoul(buffer, NULL, 10);
	       break;
	    }
	    else if (!strncmp(de->d_name, "pages_volatile", 14)) {
	       ksm_info->pages_volatile = strtoul(buffer, NULL, 10);
	       break;
	    }
	    else if (!strncmp(de->d_name, "run", 3)) {
	       ksm_info->run = strtoul(buffer, NULL, 10);
	       break;
	    }
	    else if (!strncmp(de->d_name, "sleep_millisecs", 15)) {
	       ksm_info->sleep_millisecs = strtoul(buffer, NULL, 10);
	       break;
	    }
	}
	fclose(fp);
    } 
    closedir(ksm_dir);
    return sts;
}
