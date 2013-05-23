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

#include "stdio.h"
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
 * Refreshing of the metrics for gsf2.lock_time, some of metrics are of
 * a different typing.
 *
 */
int 
gfs2_locktime_fetch(int item, struct lock_time *glstats, pmAtomValue *atom)
{
    /* Check to see if we have values to assign */
    if (glstats->lock_type >= TYPENUMBER_TRANS && glstats->lock_type <= TYPENUMBER_JOURNAL){
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

/* 
 * Gathering of the required data for the gsf2.lock_time metrics. We take all 
 * required data from the trace_pipe and the trace values come from the 
 * gfs2_glock_lock_time trace-point. Items are read in and stored within a 
 * pmdaCache. Locks are later compared in order to find the worse lock this 
 * refresh for each mounted filesystem. 
 *
 */
int 
gfs2_refresh_lock_time(pmInDom lock_time_indom, pmInDom gfs_fs_indom){
    FILE *fp;
    int fd, flags;
    char buffer[8196], *token;
    static char *TRACE_PIPE = "/sys/kernel/debug/tracing/trace_pipe";

    /* We open the pipe in both read-only and non-blocking mode */
    if ((fp = fopen(TRACE_PIPE, "r")) == NULL)
	return -oserror();

    /* Set flags of fp as non-blocking */
    fd = fileno(fp);
    flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    /*
     * Read through glocks file accumulating statistics as we go;
     * we bin unwanted glocks which are not inode(2) or resource group(3)
     * and compare to locate the worse lock on this run though
     *
     */ 
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {

        /* Clear old lock data from the cache */
        pmdaCacheOp(lock_time_indom, PMDA_CACHE_CULL);

        for (token = strtok(buffer, "."); token != NULL; token = strtok(NULL, ".")){
            char *p;
            unsigned int major, minor;
            dev_t dev_id;
            struct lock_time glock;

            if ((p = strstr(token, "gfs2_glock_lock_time: "))){  
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

                /* Check to see if the current lock is of correct type */
                if (glock.lock_type == TYPENUMBER_INODE || glock.lock_type == TYPENUMBER_RGRP){

                    /* Create hash in order to store the distinuish the data */
                    dev_id = makedev(major, minor);

                    char hash[256];
                    snprintf(hash, sizeof hash, "%d:%d|%"PRIu32"|%"PRIu64"", 
                    major(dev_id), minor(dev_id), glock.lock_type, glock.number);
      
                    if (hash == NULL){
                        close(fd);
                        return -oserror();
                    }

                    /* Store our lock data into the indom */
	            pmdaCacheStore(lock_time_indom, PMDA_CACHE_ADD, hash, (void *)&glock);
                }
            }
        }
    }
    /* 
     * We have collected our data from the trace_pipe, now it is time to take
     * the data and find the worst glocks for each of our mounted file-systems,
     * finally assign the worst locks to the file-systems to be returned to the 
     * metrics.
     *
     */
    lock_time_assign_glocks(lock_time_indom, gfs_fs_indom);

    close(fd);
    return 0;
}

/*
 * We loop through each of our available file-sytems, find the locks that corr-
 * esspond to the filesystem. With these locks we find the worst and assign it
 * to the filesystem before returning the metric values.
 *
 */
void lock_time_assign_glocks(pmInDom glock_indom, pmInDom gfs2_fs_indom){
    int i, j, major, minor, count;
    char *hash, *fs_name;

    struct gfs2_fs *fs;
    struct lock_time *glock, glockA, glockB, worst_glock;

    /* We walk through for each filesystem */
    for (pmdaCacheOp(gfs2_fs_indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(gfs2_fs_indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(gfs2_fs_indom, i, &fs_name, (void **)&fs) || !fs)
	    continue;

        /* We walk through each lock we have */
        for (pmdaCacheOp(glock_indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((j = pmdaCacheOp(glock_indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(glock_indom, j, &hash, (void **)&glock) || !glock)
	        continue;

            /* Work out the device id for the lock from the hash */
            sscanf(hash, "%u:%u", &major, &minor);

            /* If our lock belongs to the current filesystem */
            if (fs->dev_id == makedev(major, minor)){
                if (count == 0){
                    glockA = *glock;
                } else {
                    glockB = *glock;
                }
                if (count >= 1){
                    /* 
                     * Call to compare the two given locks, the return value indicates
                     * which lock is the worst, 1 for glockA and 0 for glockB.
                     *
                     */
                    if (lock_compare(&glockA, &glockB) == 1){
                        worst_glock = glockA; 
                    } else {
                        worst_glock = glockB;
                        /* 
                         * If glockB is the worse overwrite glockA with that value
                         * because on the each loop the new "challenger" will be
                         * placed as glockB.
                         *
                         */
                        glockA = glockB;
                    }
                } else {
                    worst_glock = glockA;
                }
                count++;

                /* Assign our value */
                fs->lock_time = worst_glock;
            }
        }
        count = 0;
    }
}

/*
 * Comparison function to allow the comparison of two different locks, we
 * compare the values; we return A worse than B = 1, 
 *                               B worse than A = 0.
 *
 */
int 
lock_compare(struct lock_time *glockA, struct lock_time *glockB)
{
    int true_count = 0;

    /* (A sirt (LESS THAN) B sirt = A worse) */
    if (glockA->sirtvar < glockB->sirtvar)
        true_count++;

    /* A srtt (MORE THAN) B srtt = A worse */
    if (glockA->srttvarb > glockB->srttvarb)
        true_count++;

    /* A srttb (MORE THAN) B srttb = A worse */
    if (glockA->srttvar > glockB->srttvar)
        true_count++;

    /* 
     * Base return on the number of true counts for A worse than B
     * if there is more than one true out of three A is worse, if 
     * there is one count each way we decide on qucount and dcount
     * else B is worse.
     * 
     */
    if (true_count > 1){
        return 1; /* glockA worse */
    } else if(true_count == 1){
         /* Tie-break decision case */
         if (glockA->dlm > glockA->queue) return 1; /* glockA worse */
    }
    return 0; /* glockB worse */
}
