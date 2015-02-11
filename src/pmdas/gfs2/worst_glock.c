/*
 * GFS2 gfs2_glock_lock_time trace-point metrics.
 *
 * Copyright (c) 2013 - 2014 Red Hat.
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

static struct glock glock_data;
static int reset_flag;

static int worst_glock_state = DEFAULT_WORST_GLOCK_STATE;

static const char *stattype[] = {
	[WORSTGLOCK_LOCK_TYPE]	= "lock_type",
	[WORSTGLOCK_NUMBER]	= "number",
	[WORSTGLOCK_SRTT]	= "srtt",
	[WORSTGLOCK_SRTTVAR]	= "srttvar",
	[WORSTGLOCK_SRTTB]	= "srttb",
	[WORSTGLOCK_SRTTVARB]	= "srttvarb",
	[WORSTGLOCK_SIRT]	= "sirt",
	[WORSTGLOCK_SIRTVAR]	= "sirtvar",
	[WORSTGLOCK_DLM]	= "dlm",
	[WORSTGLOCK_QUEUE]	= "queue",
};

static const char *stattext[] = {
	[WORSTGLOCK_LOCK_TYPE]	= "Glock type number",
	[WORSTGLOCK_NUMBER]	= "Inode or resource group number",
	[WORSTGLOCK_SRTT]	= "Non-blocking smoothed round trip time",
	[WORSTGLOCK_SRTTVAR]	= "Non-blocking smoothed variance",
	[WORSTGLOCK_SRTTB]	= "Blocking smoothed round trip time",
	[WORSTGLOCK_SRTTVARB]	= "Blocking smoothed variance",
        [WORSTGLOCK_SIRT]	= "Smoothed Inter-request time",
	[WORSTGLOCK_SIRTVAR]	= "Smoothed Inter-request variance",
	[WORSTGLOCK_DLM]	= "Count of Distributed Lock Manager requests",
	[WORSTGLOCK_QUEUE]	= "Count of gfs2_holder queues",
};

static const char *topnum[] = {
	[TOPNUM_FIRST]		= "first",
	[TOPNUM_SECOND]		= "second",
	[TOPNUM_THIRD]		= "third",
	[TOPNUM_FOURTH]		= "fourth",
	[TOPNUM_FIFTH]		= "fifth",
	[TOPNUM_SIXTH]		= "sixth",
	[TOPNUM_SEVENTH]	= "seventh",
	[TOPNUM_EIGHTH]		= "eighth",
	[TOPNUM_NINTH]		= "ninth",
	[TOPNUM_TENTH]		= "tenth",
};

/*
 * Sets the value of worst_glock_state using pmstore, value
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
    /* If we are assigning, we should set the reset flag for next assign */
    reset_flag = 1;

    int pmid = (item % 10);
    int position = (item / 10);

    /* Check if tracepoint is enabled */
    if (worst_glock_get_state() == 0)
        return 0;

    /* Check to see if we have values to assign */
    if (worst_glock->glocks[position].lock_type == WORSTGLOCK_INODE || 
        worst_glock->glocks[position].lock_type == WORSTGLOCK_RGRP){
        switch(pmid){
            case WORSTGLOCK_LOCK_TYPE:
                atom->ul = worst_glock->glocks[position].lock_type; /* Glock type number */
                break;
            case WORSTGLOCK_NUMBER:
                atom->ull = worst_glock->glocks[position].number; /* Inode or resource group number */
                break;
            case WORSTGLOCK_SRTT:
                atom->ll = worst_glock->glocks[position].srtt; /* Non blocking smoothed round trip time */
                break;
            case WORSTGLOCK_SRTTVAR:
                atom->ll = worst_glock->glocks[position].srttvar; /* Non blocking smoothed variance */
                break;
            case WORSTGLOCK_SRTTB:
                atom->ll = worst_glock->glocks[position].srttb; /* Blocking smoothed round trip time */
                break;
            case WORSTGLOCK_SRTTVARB:
                atom->ll = worst_glock->glocks[position].srttvarb; /* Blocking smoothed variance */
                break;
            case WORSTGLOCK_SIRT:
                atom->ll = worst_glock->glocks[position].sirt; /* Smoothed Inter-request time */
                break;
            case WORSTGLOCK_SIRTVAR:
                atom->ll = worst_glock->glocks[position].sirtvar; /* Smoothed Inter-request variance */
                break;
            case WORSTGLOCK_DLM:
                atom->ll = worst_glock->glocks[position].dlm; /* Count of dlm requests */
                break;
            case WORSTGLOCK_QUEUE:
                atom->ll = worst_glock->glocks[position].queue; /* Count of gfs2_holder queues */
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
 * Comparison function we compare the values; we return the lock which 
 * is deemed to be the worst.
 */
static int 
lock_comparison(const void *a, const void *b)
{
    struct glock *aa = (struct glock *)a;
    struct glock *bb = (struct glock *)b;
    int true_count = 0;

    /* Case to deal with the empty entries */
    if (aa->lock_type == 0)
        return 1;  /* Move empty a up the list, b moves down list */

    if (bb->lock_type == 0)
        return -1; /* Move a down the list, empty b moves up list*/

    /* A sirt (LESS THAN) B sirt = A worse */
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
        return -1; /* a is worse than b (move a down the list) */
    } else if ( true_count  == 1 ){
         /* Tie break condition */
         if ( aa->dlm > bb->queue ) return -1; /* a is worse than b (move a down the list) */
    }
    return 1; /* b is worse than a (move b up the list) */
}

/*
 * We loop through each of our available file-sytems, find the locks that corr-
 * esspond to the filesystem. With these locks we find the worst and assign it
 * to the filesystem before returning the metric values.
 */
static void
worst_glock_assign_glocks(pmInDom gfs_fs_indom)
{
    int i, j, sts;
    struct gfs2_fs *fs;

    /* We walk through for each filesystem */
    for (pmdaCacheOp(gfs_fs_indom, PMDA_CACHE_WALK_REWIND);;) {
        if ((i = pmdaCacheOp(gfs_fs_indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	sts = pmdaCacheLookup(gfs_fs_indom, i, NULL, (void **)&fs);
	if (sts != PMDA_CACHE_ACTIVE)
	    continue;

        /* Clear old entries if reset is set */
        if(reset_flag == 1){
            memset(&fs->worst_glock, 0, sizeof(struct worst_glock));
            reset_flag = 0;
        }

        /* Is the entry matching the filesystem we are on? */
        if (fs->dev_id != glock_data.dev_id)
            continue; 
        
        /* Check if we are updating an existing entry */
        for (j = 0; j < WORST_GLOCK_TOP; j++) {
            if ((fs->worst_glock.glocks[j].lock_type == glock_data.lock_type) && 
                (fs->worst_glock.glocks[j].number == glock_data.number)) {
                fs->worst_glock.glocks[j] = glock_data; 
                return;   
            }
        }

        /* If not place in next available slot */
        if (fs->worst_glock.assigned_entries < WORST_GLOCK_TOP) {
            fs->worst_glock.glocks[fs->worst_glock.assigned_entries] = glock_data;
            fs->worst_glock.assigned_entries++;
        } else {
            fs->worst_glock.glocks[WORST_GLOCK_TOP] = glock_data; /* Place in slot 11 */
        }
        qsort(fs->worst_glock.glocks, (WORST_GLOCK_TOP + 1), sizeof(struct glock), lock_comparison);             
    }   
}

/*
 * We work out the individual metric values from our buffer input and store
 * them for processing after all of the values have been extracted from the
 * trace pipe.
 */
int
gfs2_extract_worst_glock(char **buffer, pmInDom gfs_fs_indom)
{
    struct glock temp;
    unsigned int major, minor;

    /* Assign data */
    sscanf(*buffer, 
        "gfs2_glock_lock_time: %"SCNu32",%"SCNu32" glock %"SCNu32":%"SCNu64" status:%*d flags:%*x tdiff:%*d srtt:%"SCNd64"/%"SCNd64" srttb:%"SCNd64"/%"SCNd64" sirt:%"SCNd64"/%"SCNd64" dcnt:%"SCNd64" qcnt:%"SCNd64,
         &major,
         &minor, 
         &temp.lock_type,
         &temp.number,
         &temp.srtt, 
         &temp.srttvar, 
         &temp.srttb, 
         &temp.srttvarb, 
         &temp.sirt, 
         &temp.sirtvar, 
         &temp.dlm, 
         &temp.queue
    );
    temp.dev_id = makedev(major, minor);

    /* Filter on required lock types */
    if ((temp.lock_type == WORSTGLOCK_INODE || temp.lock_type == WORSTGLOCK_RGRP) &&
        (temp.dlm > COUNT_THRESHOLD || temp.queue > COUNT_THRESHOLD)) {
          
        /* Increase counters */
        glock_data = temp;         
        ftrace_increase_num_accepted_entries(); 
    }

    worst_glock_assign_glocks(gfs_fs_indom);
    return 0;
}

static void
add_pmns_node(__pmnsTree *tree, int domain, int cluster, int lock, int stat)
{
    char entry[64];
    pmID pmid = pmid_build(domain, cluster, (lock * NUM_GLOCKSTATS) + stat);

    snprintf(entry, sizeof(entry),
	     "gfs2.worst_glock.%s.%s", topnum[lock], stattype[stat]);
    __pmAddPMNSNode(tree, pmid, entry);

    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "GFS2 worst_glock added %s (%s)", entry, pmIDStr(pmid));
}

static int
refresh_worst_glock(pmdaExt *pmda, __pmnsTree **tree)
{
    int t, s, sts;
    static __pmnsTree *worst_glock_tree;

    if (worst_glock_tree) {
	*tree = worst_glock_tree;
    } else if ((sts = __pmNewPMNS(&worst_glock_tree)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: failed to create worst_glock names: %s\n",
			pmProgname, pmErrStr(sts));
	*tree = NULL;
    } else {
        for (t = 0; t < NUM_TOPNUM; t++)
	    for (s = 0; s < NUM_GLOCKSTATS; s++)
		add_pmns_node(worst_glock_tree, pmda->e_domain, CLUSTER_WORSTGLOCK, t, s);
	*tree = worst_glock_tree;
	pmdaTreeRebuildHash( worst_glock_tree, NUM_TOPNUM*NUM_GLOCKSTATS );
	return 1;
    }
    return 0;
}

/*
 * Create a new metric table entry based on an existing one.
 *
 */
static void
refresh_metrictable(pmdaMetric *source, pmdaMetric *dest, int lock)
{
    int item = pmid_item(source->m_desc.pmid);
    int domain = pmid_domain(source->m_desc.pmid);
    int cluster = pmid_cluster(source->m_desc.pmid);

    memcpy(dest, source, sizeof(pmdaMetric));
    item += lock * NUM_GLOCKSTATS;
    dest->m_desc.pmid = pmid_build(domain, cluster, item);

    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "GFS2 worst_glock refresh_metrictable: (%p -> %p) "
			"metric ID dup: %d.%d.%d -> %d.%d.%d\n",
			source, dest, domain, cluster,
			pmid_item(source->m_desc.pmid), domain, cluster, item);
}

/*
 * Used to answer the question: how much extra space needs to be
 * allocated in the metric table for (dynamic) worst_glock metrics?
 *
 */
static void
size_metrictable(int *total, int *trees)
{
    *total = NUM_GLOCKSTATS;
    *trees = NUM_TOPNUM;
}

static int
worst_glock_text(pmdaExt *pmda, pmID pmid, int type, char **buf)
{
    int item = pmid_item(pmid);
    static char text[128];

    if (pmid_cluster(pmid) != CLUSTER_WORSTGLOCK)
	return PM_ERR_PMID;
    if (item < 0 || item >= WORST_GLOCK_COUNT)
	return PM_ERR_PMID;
    snprintf(text, sizeof(text), "%s for %s worst glock",
	     stattext[item % NUM_GLOCKSTATS], topnum[item / NUM_TOPNUM]);

    *buf = text;

    return 0;
}

void
gfs2_worst_glock_init(pmdaMetric *metrics, int nmetrics)
{
    int set[] = { CLUSTER_WORSTGLOCK };

    pmdaDynamicPMNS("gfs2.worst_glock",
		    set, sizeof(set)/sizeof(int),
		    refresh_worst_glock, worst_glock_text,
		    refresh_metrictable, size_metrictable,
		    metrics, nmetrics);
}
