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

#include "ftrace.h"
#include "worst_glock.h"

#include <string.h>
#include <inttypes.h>
#include <sys/sysmacros.h>
#include <sys/types.h>



static struct worst_glock *glock_data;
static int capacity, num_accepted_entries;

static int worst_glock_state = DEFAULT_WORST_GLOCK_STATE;

/*
 * Sets the value of max_glock_state using pmstore, value
 * must be 0 or 1.
 */
int 
worst_glock_set_state(pmValueSet *vsp)
{
    int value = vsp->vlist[0].value.lval;

    if (value == 0 || value == 1) {
        worst_glock_state = value;

        return 0;
    } else {
        return PM_ERR_SIGN;
    }
}

/*
 * Used to see whether the worst_glock metrics are enabled or disabled. Should
 * only return either 0 or 1.
 */
int
worst_glock_get_state()
{
    return worst_glock_state;
}


/*
 * Refreshing of the metrics for gfs2.lock_time, some of metrics are of
 * a different typing.
 */
int 
gfs2_worst_glock_fetch(int item, struct worst_glock *worst_glock, pmAtomValue *atom)
{

    /* Check if tracepoint is enabled */
    if (worst_glock_get_state() == 0)
        return 0;

    /* Check to see if we have values to assign */
    if (worst_glock->lock_type == LOCKTIME_INODE || worst_glock->lock_type == LOCKTIME_RGRP){
        switch(item){
            case WORSTGLOCK_LOCK_TYPE:
                atom->ul = worst_glock->lock_type; /* Glock type number */
                break;
            case WORSTGLOCK_NUMBER:
                atom->ull = worst_glock->number; /* Inode or resource group number */
                break;
            case WORSTGLOCK_SRTT:
                atom->ll = worst_glock->srtt; /* Non blocking smoothed round trip time */
                break;
            case WORSTGLOCK_SRTTVAR:
                atom->ll = worst_glock->srttvar; /* Non blocking smoothed variance */
                break;
            case WORSTGLOCK_SRTTB:
                atom->ll = worst_glock->srttb; /* Blocking smoothed round trip time */
                break;
            case WORSTGLOCK_SRTTVARB:
                atom->ll = worst_glock->srttvarb; /* Blocking smoothed variance */
                break;
            case WORSTGLOCK_SIRT:
                atom->ll = worst_glock->sirt; /* Smoothed Inter-request time */
                break;
            case WORSTGLOCK_SIRTVAR:
                atom->ll = worst_glock->sirtvar; /* Smoothed Inter-request variance */
                break;
            case WORSTGLOCK_DLM:
                atom->ll = worst_glock->dlm; /* Count of dlm requests */
                break;
            case WORSTGLOCK_QUEUE:
                atom->ll = worst_glock->queue; /* Count of gfs2_holder queues */
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
 * We work out the individual metric values from our buffer input and store
 * them for processing after all of the values have been extracted from the
 * trace pipe.
 */
int
gfs2_extract_worst_glock(char *buffer)
{
    struct worst_glock glock;
    unsigned int major, minor;

    /* If we havent already set the array, have a go */
    if (glock_data == NULL) {
        num_accepted_entries = 0;
        capacity = FTRACE_ARRAY_CAPACITY;

        glock_data = malloc(capacity * sizeof(struct worst_glock));

        if (glock_data == NULL) { /* If we fail, return */
            return -oserror();
        } 
    }

    /* Assign data */
    sscanf(buffer, 
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
    if ((glock.lock_type == LOCKTIME_INODE || glock.lock_type == LOCKTIME_RGRP) &&
        (glock.dlm > COUNT_THRESHOLD || glock.queue > COUNT_THRESHOLD)) {

        /* Re-allocate and extend array if we are near capacity */
        if (num_accepted_entries == capacity) {
            struct worst_glock *glock_data_realloc = realloc(glock_data, (capacity + FTRACE_ARRAY_CAPACITY) * sizeof(struct worst_glock));
            
            if (glock_data_realloc == NULL) {
                free(glock_data);
                glock_data = NULL;
                return -oserror();
            } else {
                glock_data = glock_data_realloc;
                glock_data_realloc = NULL;
                capacity += FTRACE_ARRAY_CAPACITY;
            }
        }
          
        /* Assign and increase counters */        
        glock_data[num_accepted_entries] = glock;
        num_accepted_entries++;
        ftrace_increase_num_accepted_entries(); /* Increase the global counter aswell */  
    }

    return 0;
}

/*
 * Comparison function we compare the values; we return the lock which 
 * is deemed to be the worst.
 */
static int 
lock_comparison(const void *a, const void *b)
{
    struct worst_glock *aa = (struct worst_glock *)a;
    struct worst_glock *bb = (struct worst_glock *)b;
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
 */
void
worst_glock_assign_glocks(pmInDom gfs2_fs_indom)
{
    int i, j, sts;
    struct gfs2_fs *fs;

    /* Sort our values with our comparator */
    qsort(glock_data, num_accepted_entries, sizeof(struct worst_glock), lock_comparison);  

    /* We walk through for each filesystem */
    for (pmdaCacheOp(gfs2_fs_indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(gfs2_fs_indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	sts = pmdaCacheLookup(gfs2_fs_indom, i, NULL, (void **)&fs);
	if (sts != PMDA_CACHE_ACTIVE)
	    continue;

            /* Reset old metric data */
            memset(&fs->worst_glock, 0, sizeof(struct worst_glock));

        /* Assign our worst glock */
        for (j = 0; j < num_accepted_entries; j++) {
            if (fs->dev_id != glock_data[j].dev_id)
                continue;    

            fs->worst_glock = glock_data[j];
            break;  
        }

    }
    /* Free memory */
    free(glock_data);
    glock_data = NULL;

    capacity = FTRACE_ARRAY_CAPACITY;
    num_accepted_entries = 0;
}
