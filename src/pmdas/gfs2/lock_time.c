/*
 * GFS2 gfs2_glock_lock_time trace-point metrics.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "pmdagfs2.h"
#include "lock_time.h"
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

static int num_entries;
static size_t capacity = GLOCK_ARRAY_CAPACITY;

static struct lock_time *glock_array;

/*
 * Refreshing of the metrics for gfs2.lock_time, some of metrics are of
 * a different typing.
 *
 */
int 
gfs2_lock_time_fetch(int item, struct lock_time *glstats, pmAtomValue *atom)
{
    /* Check to see if we have values to assign */
    if (glstats->lock_type == LOCKTIME_INODE || glstats->lock_type == LOCKTIME_RGRP){
        switch(item){
            case LOCKTIME_LOCK_TYPE:
                atom->ul = glstats->lock_type; /* Glock type number */
                break;
            case LOCKTIME_NUMBER:
                atom->ull = glstats->number; /* Inode or resource group number */
                break;
            case LOCKTIME_SRTT:
                atom->ll = glstats->srtt; /* Non blocking smoothed round trip time */
                break;
            case LOCKTIME_SRTTVAR:
                atom->ll = glstats->srttvar; /* Non blocking smoothed variance */
                break;
            case LOCKTIME_SRTTB:
                atom->ll = glstats->srttb; /* Blocking smoothed round trip time */
                break;
            case LOCKTIME_SRTTVARB:
                atom->ll = glstats->srttvarb; /* Blocking smoothed variance */
                break;
            case LOCKTIME_SIRT:
                atom->ll = glstats->sirt; /* Smoothed Inter-request time */
                break;
            case LOCKTIME_SIRTVAR:
                atom->ll = glstats->sirtvar; /* Smoothed Inter-request variance */
                break;
            case LOCKTIME_DLM:
                atom->ll = glstats->dlm; /* Count of dlm requests */
                break;
            case LOCKTIME_QUEUE:
                atom->ll = glstats->queue; /* Count of gfs2_holder queues */
                break;
            default:
            return PM_ERR_PMID;
        }
        return 1; /* Return we have had values */
    } else { 
        return 0; /* If we have no valid values */
    }
}

extern int
gfs2_extract_glock_lock_time(char *buffer)
{
    /* If we havent already set the array, have a go */
    if (glock_array == NULL) {
        glock_array = malloc(capacity * sizeof(struct lock_time));
        if (glock_array == NULL) { /* If we fail, return */
            return -oserror();
        } 
    }

    struct lock_time glock;
    unsigned int major, minor;
    char *p;

    if ( (p = strstr(buffer, "gfs2_glock_lock_time: ")) ) {

        /* Assign data here */
        sscanf(p, 
            "gfs2_glock_lock_time: %"SCNu32",%"SCNu32" glock %"SCNu32":%"SCNu64" status:%*d flags:%*x tdiff:%*d srtt:%"SCNd64"/%"SCNd64" srttb:%"SCNd64"/%"SCNd64" sirt:%"SCNd64"/%"SCNd64" dcnt:%"SCNd64" qcnt:%"SCNd64,
             &major,
             &minor, 
             &glock.lock_type,
             &glock.number,
             &glock.srtt, 
             &glock.srttvar, 
             &glock.srttb, 
             &glock.srttvarb, 
             &glock.sirt, 
             &glock.sirtvar, 
             &glock.dlm, 
             &glock.queue
        );
        glock.dev_id = makedev(major, minor);

        /* Filter on required lock types */
        if ((glock.lock_type == LOCKTIME_INODE || 
            glock.lock_type == LOCKTIME_RGRP) &&
            (glock.dlm > COUNT_THRESHOLD || 
            glock.queue > COUNT_THRESHOLD)) {

            /* Re-allocate and extend array if we are near capacity */
            if (num_entries == capacity) {
                struct lock_time *glock_array_realloc = realloc(glock_array, (capacity + GLOCK_ARRAY_CAPACITY) * sizeof(struct lock_time));
            
                if (glock_array_realloc == NULL) {
                    free(glock_array);
                    return -oserror();
                } else {
                    glock_array = glock_array_realloc;
                    glock_array_realloc = NULL;
                    capacity += GLOCK_ARRAY_CAPACITY;
                }
            }
          
            /* Allocate and increase counters */        
            glock_array[num_entries] = glock;
            num_entries++;
            gfs2_ftrace_increase_num_accepted_locks();
        }
    }

    return 0;
}

/*
 * Comparison function we compare the values; we return the lock which 
 * is deemed to be the worst.
 *
 */
static int 
lock_comparison(const void *a, const void *b)
{
    struct lock_time *aa = (struct lock_time *)a;
    struct lock_time *bb = (struct lock_time *)b;
    int true_count = 0;

    /* (A sirt (LESS THAN) B sirt = A worse) */
    if (aa->sirtvar < bb->sirtvar)
        true_count++;

    /* A srtt (MORE THAN) B srtt = A worse */
    if (aa->srttvarb > bb->srttvarb)
        true_count++;

    /* A srttb (MORE THAN) B srttb = A worse */
    if (aa->srttvar > bb->srttvar)
        true_count++;

    /* If there are more counts where A is worse than B? */
    if ( true_count > 1 ) {
        return 1; /* a is worse than b */
    } else if ( true_count  == 1 ){
         /* Tie break condition */
         if ( aa->dlm > bb->queue ) return 1; /* a is worse than b */
    }
    return -1; /* b is worse than a */
}

/*
 * We loop through each of our available file-sytems, find the locks that corr-
 * esspond to the filesystem. With these locks we find the worst and assign it
 * to the filesystem before returning the metric values.
 *
 */
extern void
lock_time_assign_glocks(pmInDom gfs2_fs_indom)
{
    int i, j, sts;
    struct gfs2_fs *fs;

    /* Sort our values with our comparator */
    qsort(glock_array, num_entries, sizeof(struct lock_time), lock_comparison);  

    /* We walk through for each filesystem */
    for (pmdaCacheOp(gfs2_fs_indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(gfs2_fs_indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	sts = pmdaCacheLookup(gfs2_fs_indom, i, NULL, (void **)&fs);
	if (sts != PMDA_CACHE_ACTIVE)
	    continue;         

        /* Assign our worst glock */
        for (j = 0; j < num_entries; j++) {
            if (fs->dev_id != glock_array[j].dev_id)
                continue;

            fs->lock_time = glock_array[j];
            break;  
        }

    }
    /* Free memory used for array */
    free(glock_array);
    glock_array = NULL;
    num_entries = 0;
}
