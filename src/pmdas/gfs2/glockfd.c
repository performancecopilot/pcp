/*
 * GFS2 glockfd file statistics.
 *
 * Copyright (c) 2013-2025 Red Hat.
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
#include "libpcp.h"

#include "pmdagfs2.h"
#include "glockfd.h"

int32_t *fds;
int32_t *pids;

int
gfs2_glockfd_fetch(int item, struct glockfd *glockfd, pmAtomValue *atom)
{
    switch (item) {
        case GLOCKFD_TOTAL:
             atom->ull = glockfd->total;
             return PMDA_FETCH_STATIC;

        default:
            return PM_ERR_PMID;
    }
}

int
gfs2_per_holder_fetch(int item, unsigned int inst, pmAtomValue *atom)
{
    struct glockfd_per_instance *per_instance;
    pmInDom indom;
    int sts;

    switch (item) {
        case GLOCKFD_PROCESS:
            indom = INDOM(GFS_HOLDER_INDOM);
            sts = pmdaCacheLookup(indom, inst, NULL, (void **)&per_instance);
            
            if (sts <0)
                return sts;

            if (sts != PMDA_CACHE_ACTIVE)
                return PM_ERR_INST;

            atom->ull = per_instance->pids;
            return PMDA_FETCH_STATIC;

        case GLOCKFD_FILE_DESCRIPTOR:
            indom = INDOM(GFS_HOLDER_INDOM);
            sts = pmdaCacheLookup(indom, inst, NULL, (void **)&per_instance);
            
            if (sts <0)
                return sts;

            if (sts != PMDA_CACHE_ACTIVE)
                return PM_ERR_INST;

            atom->ull = per_instance->fds;
            return PMDA_FETCH_STATIC;

        default:
            return PM_ERR_PMID;
    }
}

int
gfs2_refresh_glockfd(const char *sysfs, const char *name, struct glockfd *glockfd)
{
    char buffer[4096];
    uint64_t total = 0, counter = 0;
    FILE *pf;
    
    pmsprintf(buffer, sizeof(buffer), "%s/%s/glockfd", sysfs, name);
    
    if ((pf = fopen(buffer, "r")) == NULL)
        return -oserror();

    while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
        // for each entry we increase the value of total
        total++;
        
        // set total
        glockfd->total = total;
    }
    
    // reset pointer to top of file to collect holder information
    // now we know the file size
    fseeko(pf, 0, SEEK_SET);
    
    fds = calloc(total, sizeof(int32_t));
    if (fds == NULL) {
        fclose(pf);
        return PM_ERR_AGAIN;
    }

    pids = calloc(total, sizeof(int32_t));
    if (pids == NULL) {
        fclose(pf);
        free(fds);
        return PM_ERR_AGAIN;
    }

    while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
        // collect the values
        sscanf(buffer, "%"SCNi32" %"SCNi32" %*s",
             &fds[counter],
             &pids[counter]
        );
        counter++;
    }
    fclose(pf);

    return 0;
}

int
gfs2_refresh_per_holder(void)
{
    char inst_name[128], *fs_name;
    struct gfs2_fs *fs;
    int inst, sts;
    
    pmInDom fs_indom = INDOM(GFS_FS_INDOM);
    pmInDom holder_indom = INDOM(GFS_HOLDER_INDOM);

    for (pmdaCacheOp(fs_indom, PMDA_CACHE_WALK_REWIND);;) {
        if ((inst = pmdaCacheOp(fs_indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
        if (!pmdaCacheLookup(fs_indom, inst, &fs_name, (void **)&fs) || !fs)
	    continue;

        for (int i = 0; i < fs->glockfd.total; i++) {
            pmsprintf(inst_name, sizeof(inst_name), "%s::holder_%d", fs_name, i);

            struct glockfd_per_instance *per_instance;

            sts = pmdaCacheLookupName(holder_indom, inst_name, NULL, (void **)&per_instance);
            if (sts == PM_ERR_INST || (sts >=0 && per_instance == NULL)) {
                per_instance = calloc(1, sizeof(struct glockfd_per_instance));
                if (per_instance == NULL) {
                    return PM_ERR_AGAIN;
                }
            }
            else if (sts < 0)
                continue;

            per_instance->holder_id = i;
            per_instance->pids = pids[i];
            per_instance->fds = fds[i];
            
            pmdaCacheStore(holder_indom, PMDA_CACHE_ADD, inst_name, (void *)per_instance);
        }
    }
    free(pids);
    free(fds);
    
    return 0;   
}
