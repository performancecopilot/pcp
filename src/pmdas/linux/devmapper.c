/*
 * Linux LVM Devices Cluster
 *
 * Copyright (c) 2013 Red Hat.
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

#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "devmapper.h"

#define	MAPDIR "/dev/mapper"

int
refresh_dev_mapper(dev_mapper_t *lvs)
{
    int i;
    DIR *dirp;
    struct dirent *dentry;
    struct stat statbuf;
    char path[MAXPATHLEN];

    dirp = opendir(MAPDIR);
    if (dirp == NULL)
        return 1;
  
    for (i = 0; i < lvs->nlv; i++) {
        free(lvs->lv[i].dev_name);
        free(lvs->lv[i].lv_name);
    }
    lvs->nlv = 0;
    lvs->lv = NULL;
    while ((dentry = readdir(dirp)) != NULL) {
        char linkname[MAXPATHLEN];
        int linkname_len;

        snprintf(path, sizeof(path), "%s/%s", MAPDIR, dentry->d_name);
        if (stat(path, &statbuf) == -1)
            continue;
        if (!S_ISBLK(statbuf.st_mode))
            continue;

        if ((linkname_len = readlink(path, linkname, sizeof(linkname)-1)) < 0)
	    continue;
	linkname[linkname_len] = '\0';

        i = lvs->nlv;
        lvs->nlv++;
      
        lvs->lv = (lv_entry_t *)realloc(lvs->lv, lvs->nlv * sizeof(lv_entry_t));
        lvs->lv[i].id = lvs->nlv;

        lvs->lv[i].dev_name = malloc(strlen(dentry->d_name)+1);
        strcpy(lvs->lv[i].dev_name, dentry->d_name);

        lvs->lv[i].lv_name = malloc(linkname_len+1);
        strcpy(lvs->lv[i].lv_name, linkname);
    }
    closedir(dirp);

    if (lvs->lv_indom->it_numinst != lvs->nlv) {
        lvs->lv_indom->it_numinst = lvs->nlv;
        lvs->lv_indom->it_set = (pmdaInstid *)
                realloc(lvs->lv_indom->it_set, lvs->nlv * sizeof(pmdaInstid));
    }
    for (i = 0; i < lvs->nlv; i++) {
        int skip_prefix = 0;
        lvs->lv_indom->it_set[i].i_inst = lvs->lv[i].id;
        if (strncmp (lvs->lv[i].lv_name, "../", 3) == 0)
            skip_prefix = 3;
        lvs->lv_indom->it_set[i].i_name = lvs->lv[i].lv_name + skip_prefix;
    }
    return 0;
}
