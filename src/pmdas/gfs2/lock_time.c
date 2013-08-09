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
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

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

static void
gfs2_extract_glock_lock_time(char *buffer, pmInDom lock_time_indom)
{
    struct lock_time glock;
    unsigned int major, minor;
    char *p, hash[256];

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
        if (glock.lock_type == LOCKTIME_INODE || glock.lock_type == LOCKTIME_RGRP) {

            if (glock.dlm > COUNT_THRESHOLD && glock.queue > COUNT_THRESHOLD) {

                /* Create unique hash for pmdaCache */
                snprintf(hash, sizeof(hash), "%d:%d|%"PRIu32"|%"PRIu64"", 
                    major(glock.dev_id),
                    minor(glock.dev_id),
                    glock.lock_type, 
                    glock.number);
                hash[sizeof(hash)-1] = '\0';
        
                /* Store in pmdaCache */
                pmdaCacheStore(lock_time_indom, PMDA_CACHE_ADD, hash, (void *)&glock);
            }
        }
    }
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
static int
lock_time_assign_glocks(pmInDom glock_indom, pmInDom gfs2_fs_indom)
{
    int i, j;
    struct gfs2_fs *fs;
    struct lock_time *glock;   

    int array_size = pmdaCacheOp(glock_indom, PMDA_CACHE_SIZE_ACTIVE);   
  
    struct lock_time *glock_array = malloc(array_size * sizeof(struct lock_time));
    if (glock_array == NULL){
        return -oserror();
    }   

    /* We walk through for each filesystem */
    for (pmdaCacheOp(gfs2_fs_indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(gfs2_fs_indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(gfs2_fs_indom, i, NULL, (void **)&fs) || !fs)
	    continue;

        int counter = 0;

        /* We walk through each lock we have */
        for (pmdaCacheOp(glock_indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((j = pmdaCacheOp(glock_indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(glock_indom, j, NULL, (void **)&glock) || !glock || !PMDA_CACHE_ACTIVE)
	        continue;

            /* If our lock belongs to the current filesystem */
            if (fs->dev_id == glock->dev_id){

                    /* Assign values in array */
                    glock_array[counter] = *glock;
                    counter++;
            }    
        }

        if (counter > 0){
            /* Sort our values with our comparator */
            qsort(glock_array, counter, sizeof(struct lock_time), lock_comparison);           

            /* Assign our worst glock */
            fs->lock_time = glock_array[0];
        }    

        /* Clear array for next filesystem */
        memset(glock_array, 0, array_size * sizeof(struct lock_time));
    }
    /* Free memory used for array */
    free(glock_array);

    return 0;
}

/* 
 * Gathering of the required data for the gfs2.lock_time metrics. We take all 
 * required data from the trace_pipe and the trace values come from the 
 * gfs2_glock_lock_time trace-point. Items are read in and stored within a 
 * pmdaCache. Locks are later compared in order to find the worse lock this 
 * refresh for each mounted filesystem. 
 *
 */
int 
gfs2_refresh_lock_time(pmInDom lock_time_indom, pmInDom gfs_fs_indom)
{
    FILE *fp;
    int fd, flags;
    char buffer[8196];
    static char *TRACE_PIPE = "/sys/kernel/debug/tracing/trace_pipe";

    /* Clear old lock data from the cache */
    pmdaCacheOp(lock_time_indom, PMDA_CACHE_CULL);

    /* We open the pipe in both read-only and non-blocking mode */
    if ((fp = fopen(TRACE_PIPE, "r")) == NULL)
	return -oserror();

    /* Set flags of fp as non-blocking */
    fd = fileno(fp);
    flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | O_RDONLY | O_NONBLOCK);

    /*
     * Read through glocks file accumulating statistics as we go.
     *
     */
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        gfs2_extract_glock_lock_time(buffer, lock_time_indom);
    }
    fclose(fp);

    /* 
     * We have collected our data from the trace_pipe, now it is time to take
     * the data and find the worst glocks for each of our mounted file-systems,
     * finally assign the worst locks.
     *
     */
    lock_time_assign_glocks(lock_time_indom, gfs_fs_indom);
    return 0;
}
