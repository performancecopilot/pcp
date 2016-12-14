/*
 * Linux zoneinfo Cluster
 *
 * Copyright (c) 2016 fujitsu.
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
#include "indom.h"
#include "proc_zoneinfo.h"
#define SYSFS_NODE_PATH "/sys/devices/system/node"

int get_nodes(){
    DIR * dir;
    struct dirent * ptr;
    int nodes =0;
    dir = opendir(SYSFS_NODE_PATH);
    while((ptr = readdir(dir)) != NULL)
    {
        if (strstr(ptr->d_name, "node")) {
           nodes++;
        }
    }
    return nodes;
}

int
refresh_proc_zoneinfo(pmInDom indom)
{
    int sts;
    zoneinfo_entry_t *ze;
    int nodeid = 0;
    char node_id[20];
    int i = 0;
    char *sp;
    int nodes = get_nodes();
    zoneinfo_entry_t *info;
    char type[20];
    int page_free = 0;
    FILE *fp;
    char buf[1024];

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
    if ((fp = linux_statsfile("/proc/zoneinfo", buf, sizeof(buf))) == NULL)
        return -oserror();
    for (i=0; i < nodes; i++) {
        info = (zoneinfo_entry_t *)malloc(sizeof(zoneinfo_entry_t));
        info->dma32_free = 0; 
        info->dma_free = 0; 
        info->normal_free = 0;
        info->highmem_free = 0; 
        while(fgets(buf, sizeof(buf), fp) != NULL){
            if (strncmp(buf, "Node", 4) != 0)
                continue;
 
            if (sscanf(buf, "Node %d, zone   %s", &nodeid, type) != 2)
                continue;
            if (i == nodeid) {
                snprintf(node_id, sizeof(node_id), "node%d", nodeid);
                node_id[sizeof(node_id)-1] = '\0';
                while(fgets(buf, sizeof(buf), fp) != NULL){
 	            if ((sp = strstr(buf, "pages free")) != (char *)NULL) {
                        if (sscanf(sp, "pages free     %d", &page_free) == 1) {

                            if (strncmp(type, "Normal", 6) ==0) {
                                info->normal_free = page_free;
                            }
#ifdef __x86_64__
                            else if (strncmp(type, "DMA32", 5) == 0) {
                                info->dma32_free = page_free;
                            }
#elif __i386__
                            else if (strncmp(type, "HighMem", 7) ==0) {
                                info->highmem_free = page_free;
                            }
#endif
                            else if(strncmp(type, "DMA", 3) == 0) {
                                info->dma_free = page_free;
                            }
                            break;
                        }
                    }
                }
            }
        }
        fseek(fp, 0L, 0);
	sts = pmdaCacheLookupName(indom, node_id, NULL, (void **)&ze);
	if (sts == PMDA_CACHE_ACTIVE)
	    continue;
	if (sts == PMDA_CACHE_INACTIVE) {
            ze = info;
	    sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, node_id, (void *)ze);
        }
        else {
	    if ((ze = (zoneinfo_entry_t *)malloc(sizeof(zoneinfo_entry_t))) == NULL) {
                continue;
	    }
            memset(ze, 0, sizeof(zoneinfo_entry_t));

            ze = info;
	    sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, node_id, (void *)ze);
	    if (sts < 0) {
		fprintf(stderr, "Warning: refresh_proc_zoneinfo: pmdaCacheOp(%s, ADD, \"%s\"): %s\n",
		    pmInDomStr(indom), node_id, pmErrStr(sts));
		free(ze);
	    }
#if PCP_DEBUG
	    else {
		if (pmDebug & DBG_TRACE_LIBPMDA)
		    fprintf(stderr, "refresh_proc_zoneinfo: instance \"%s\" = \"\n",
			node_id);
	    }
#endif
        }
    }
    pmdaCacheOp(indom, PMDA_CACHE_SAVE);
    fclose(fp);
    return 0;
}
