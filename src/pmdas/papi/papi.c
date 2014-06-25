/*
 * PAPI PMDA
 *
 * Copyright (c) 2014 Red Hat.
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

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include "domain.h"
#include <papi.h>
#include <assert.h>
#include <grp.h>


#define CLUSTER_PAPI 0 //we should define this in a header for when these exand possible values
#define NumEvents 17
static char     *username = "adm";
static char     mypath[MAXPATHLEN];
static char     isDSO = 1; /* == 0 if I am a daemon */
static int      EventSet = PAPI_NULL;
//static int      NumEvents = 17;
static long_long values[NumEvents] = {(long_long) 0};
static int enablers[NumEvents] = {0};
static unsigned int enable_counters = 0;
struct uid_gid_tuple {
    char wildcard_p; /* do not filter for this context. */
    char uid_p; char gid_p; /* uid/gid received flags. */
    int uid; int gid; /* uid/gid received from PCP_ATTR_* */
    int trackers[17];
}; 
static struct uid_gid_tuple *ctxtab = NULL;
int ctxtab_size = 0;
static unsigned int queue;

/*
 * There will be two domain instances, one for kernel counters
 * and one for process counters.
 */

static pmdaIndom indomtab[] = {
#define PAPI_KERNEL    0
    { PAPI_KERNEL, 1, 0 },
    //#define PAPI_PROC      1
    //      {},
};

/* XXX not entirely sure what this is */
static pmInDom *kernel_indom = &indomtab[PAPI_KERNEL].it_indom;

void enlarge_ctxtab(int context)
{
    /* Grow the context table if necessary. */
    if (ctxtab_size /* cardinal */ <= context /* ordinal */) {
        size_t need = (context + 1) * sizeof(struct uid_gid_tuple);
        ctxtab = realloc (ctxtab, need);
        if (ctxtab == NULL)
            __pmNoMem("papi ctx table", need, PM_FATAL_ERR);
        /* Blank out new entries. */
        while (ctxtab_size <= context)
            memset (& ctxtab[ctxtab_size++], 0, sizeof(struct uid_gid_tuple));
    }
}

/*
 * A list of all the papi metrics we support - 
 */
static pmdaMetric metrictab[] = {

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.total_inst */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.enable_counters */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L1_DCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L1_ICM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L2_DCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L2_ICM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L3_DCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L3_ICM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L1_TCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L2_TCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L3_TCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.TLB_DM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.TLB_IM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.TLB_TL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L1_LDM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L1_STM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L2_LDM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L2_STM */

};

int
check_eventset(int context, int item)
{
    int retval = 0;
    if(enablers[item] == 0){
	//	__pmNotifyErr(LOG_DEBUG, "check_eventset first: %d %d %d\n", retval, pmdaGetContext(), item);
	switch (item){
	case 0:
	    retval = PAPI_add_event(EventSet, PAPI_TOT_INS);
	    //	    __pmNotifyErr(LOG_DEBUG, "check_eventset: %d %d %d\n", retval, pmdaGetContext(), item);
	    break;
	case 1:
	    //	    retval = PAPI_add_event(EventSet, PAPI_TOT_CYC); //XXX add me
	    break;
	case 2:
	    retval = PAPI_add_event(EventSet, PAPI_L1_DCM);
	    break;
	case 3:
	    retval = PAPI_add_event(EventSet, PAPI_L1_ICM);
	    break;
	case 4:
	    retval = PAPI_add_event(EventSet, PAPI_L2_DCM);
	    break;
	case 5:
	    retval = PAPI_add_event(EventSet, PAPI_L2_ICM);
	    break;
	case 6:
	    retval = PAPI_add_event(EventSet, PAPI_L3_DCM);
	    break;
	case 7:
	    retval = PAPI_add_event(EventSet, PAPI_L3_ICM);
	    break;
	case 8:
	    retval = PAPI_add_event(EventSet, PAPI_L1_TCM);
	    break;
	case 9:
	    retval = PAPI_add_event(EventSet, PAPI_L2_TCM);
	    break;
	case 10:
	    retval = PAPI_add_event(EventSet, PAPI_L3_TCM);
	    break;
	case 11:
	    retval = PAPI_add_event(EventSet, PAPI_TLB_DM);
	    break;
	case 12:
	    retval = PAPI_add_event(EventSet, PAPI_TLB_IM);
	    break;
	case 13:
	    retval = PAPI_add_event(EventSet, PAPI_TLB_TL);
	    break;
	case 14:
	    retval = PAPI_add_event(EventSet, PAPI_L1_LDM);
	    break;
	case 15:
	    retval = PAPI_add_event(EventSet, PAPI_L1_STM);
	    break;
	case 16:
	    retval = PAPI_add_event(EventSet, PAPI_L2_LDM);
	    break;
	case 17:
	    retval = PAPI_add_event(EventSet, PAPI_L2_STM);
	    break;

	default:
	    break;
	}
	if(retval == PAPI_OK || retval == PAPI_EISRUN){
	    PAPI_start(EventSet); //check this value as well
	    //	    __pmNotifyErr(LOG_DEBUG, "entered proper retval area\n");    
	    enablers[item] = 1;
	    ctxtab[context].trackers[item] = 1;
	    return PM_ERR_PMDANOTREADY;
	}
	else
	    return PM_ERR_NODATA;
    }
    else if(enablers[item] == 1 && ctxtab[context].trackers[item] == 0){
	ctxtab[context].trackers[item] = 1;
	return 0;
    }
    else if(enablers[item] == 1 && ctxtab[context].trackers[item] == 1)
	return 0;
    else{
	return PM_ERR_NODATA;
    }
}

static int
papi_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int sts;
    /* this will probably need to be expanded to fit the domains as well */
    sts = check_eventset(pmdaGetContext(), idp->item);
    if (sts < 0){
	return sts;
    }
    switch (idp->cluster) {
    case CLUSTER_PAPI:
	//switch indom statement will end up being here
	switch (idp->item) {
	case 0:
	    // might not be the first to have this metric being used so set tracker value
	    PAPI_read(EventSet, values);
	    atom->ull = values[0]; /* papi.kernel.total_inst */
	    break;
		
	case 1:
	    atom->ul = enable_counters;
	    break;

	case 2:
	    PAPI_read(EventSet, values);
	    atom->ull = values[1]; /* papi.kernel.L1_DCM */
	    break;

	case 3:
	    PAPI_read(EventSet, values);
	    atom->ull = values[2]; /* papi.kernel.L1_ICM */
	    break;

	case 4:
	    PAPI_read(EventSet, values);
	    atom->ull = values[3]; /* papi.kernel.L2_DCM */
	    break;

	case 5:
	    PAPI_read(EventSet, values);
	    atom->ull = values[4]; /* papi.kernel.L2_ICM */
	    break;

	case 6:
	    PAPI_read(EventSet, values);
	    atom->ull = values[5]; /* papi.kernel.L3_DCM */
	    break;

	case 7:
	    PAPI_read(EventSet, values);
	    atom->ull = values[6]; /* papi.kernel.L3_ICM */
	    break;

	case 8:
	    PAPI_read(EventSet, values);
	    atom->ull = values[7]; /* papi.kernel.L1_TCM */
	    break;

	case 9:
	    PAPI_read(EventSet, values);
	    atom->ull = values[8]; /* papi.kernel.L2_TCM */
	    break;

	case 10:
	    PAPI_read(EventSet, values);
	    atom->ull = values[9]; /* papi.kernel.L3_TCM */
	    break;

	case 11:
	    PAPI_read(EventSet, values);
	    atom->ull = values[10]; /* papi.kernel.TLB_DM */
	    break;

	case 12:
	    PAPI_read(EventSet, values);
	    atom->ull = values[11]; /* papi.kernel.TLB_IM */
	    break;

	case 13:
	    PAPI_read(EventSet, values);
	    atom->ull = values[12]; /* papi.kernel.TLB_TL */
	    break;

	case 14:
	    PAPI_read(EventSet, values);
	    atom->ull = values[13]; /* papi.kernel.L1_LDM */
	    break;

	case 15:
	    PAPI_read(EventSet, values);
	    atom->ull = values[14]; /* papi.kernel.L1_STM */
	    break;

	case 16:
	    PAPI_read(EventSet, values);
	    atom->ull = values[15]; /* papi.kernel.L2_LDM */
	    break;

	case 17:
	    PAPI_read(EventSet, values);
	    atom->ull = values[16]; /* papi.kernel.L2_STM */
	    break;

	default:
	    return 0;
	} // item switch
	break;
    default:
	return 0;
    } // cluster switch
	return PMDA_FETCH_STATIC;
}

static int
papi_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    if ( ctxtab[pmda->e_context].uid == 0 || ctxtab[pmda->e_context].gid == 0 )
	return pmdaFetch(numpmid, pmidlist, resp, pmda);
    else
	return PM_ERR_PERMISSION;
}

#if 0
static int
papi_store(pmResult *result, pmdaExt *pmda)
{
    int sts = 0;
    int i = 0;
    //XXX add check for access
    //XXX need to add proper handling of pmstore 0 when it hasn't already been enabled
    for (i = 0; i < result->numpmid; i++){
	pmValueSet *vsp = result->vset[i];
	__pmID_int *idp = (__pmID_int *)&(vsp->pmid);
	pmAtomValue av;
	switch (idp->item){
	case 1:
	    if((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				    PM_TYPE_U32, &av, PM_TYPE_U32)) >= 0){
		switch(av.ul){
		case 0:
		case 1:
		    //		    if (av.ul == 0 && enable_counters == 1){
		    if (av.ul == 0){
			enable_counters = av.ul;
			if(PAPI_stop(EventSet,values) != PAPI_OK){
			    __pmNotifyErr(LOG_DEBUG, "PAPI_stop in pmstore failed.\n");
			    return 1; 
			}
			else{
			    memset(enablers, 0, NumEvents*sizeof(enablers[0]));
			    __pmNotifyErr(LOG_DEBUG, "PAPI_stop in pmstore succeeded.\n");
			}
		    }
		    //		    else if (av.ul == 1 && enable_counters == 0){
		    else if (av.ul == 1){
			enable_counters = av.ul;
			if (PAPI_start(EventSet) != PAPI_OK){
			    __pmNotifyErr(LOG_DEBUG, "PAPI_start in pmstore failed.\n");
			    return 1; // XXX check return value correct
			}			
		    }
		    else // no need to change
			break;
		default:
		    sts = PM_ERR_CONV;
		    break;
		} // else
	    } //if sts
	default:
	    sts = PM_ERR_PMID;
	    break;
	}//switch item
    }

    PAPI_read(EventSet, values);
    return 0;
}
#endif

int papi_internal_init()
{
    int retval;
    retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT) {
	__pmNotifyErr(LOG_DEBUG, "PAPI library init error!\n");
	return retval;
    }

    /* Check to see if the preset, PAPI_TOT_INS, exists */
    retval = PAPI_query_event( PAPI_TOT_INS );
    if (retval != PAPI_OK) {
    __pmNotifyErr(LOG_DEBUG, "No instruction counter? How lame.\n");
	return retval;
    }

    if(PAPI_create_eventset(&EventSet) != PAPI_OK){
    __pmNotifyErr(LOG_DEBUG, "create eventset error!\n");
	return retval;
    }

    /*
    if (PAPI_add_event(EventSet, PAPI_TOT_INS) != PAPI_OK){
	__pmNotifyErr(LOG_DEBUG, "couldn't add PAPI_TOT_INS to event set\n");
	return retval;
	}*/
    /* We can't just assume everything is here, add checks
       all this papi stuff should probably be done in its own function
       The papi_add_event can probably just be done in papi_add_events
       I need to think of a better, more maliable way to specify what
       event counts we want to enable, having a 'enable ALL the counters'
       approach isn't proper */
    /*    PAPI_add_event(EventSet, PAPI_L1_DCM);
    PAPI_add_event(EventSet, PAPI_L1_ICM);
    PAPI_add_event(EventSet, PAPI_L2_DCM);
    PAPI_add_event(EventSet, PAPI_L2_ICM);
    PAPI_add_event(EventSet, PAPI_L3_DCM);
    PAPI_add_event(EventSet, PAPI_L3_ICM);
    PAPI_add_event(EventSet, PAPI_L1_TCM);
    PAPI_add_event(EventSet, PAPI_L2_TCM);
    PAPI_add_event(EventSet, PAPI_L3_TCM);
    PAPI_add_event(EventSet, PAPI_TLB_DM);
    PAPI_add_event(EventSet, PAPI_TLB_IM);
    PAPI_add_event(EventSet, PAPI_TLB_TL);
    PAPI_add_event(EventSet, PAPI_L1_LDM);
    PAPI_add_event(EventSet, PAPI_L1_STM);
    PAPI_add_event(EventSet, PAPI_L2_LDM);
    PAPI_add_event(EventSet, PAPI_L2_STM);*/

    /*    if(PAPI_start(EventSet) != PAPI_OK){
	__pmNotifyErr(LOG_DEBUG, "PAPI_start failed.\n");
	return 1; 
	}*/

    return retval;

}

/* use documented in pmdaAttribute(3) */
static int
papi_contextAttributeCallBack(int context, int attr,
			      const char *value, int length, pmdaExt *pmda)
{
    static int rootlike_gids_found = 0;
    static int adm_gid = -1;
    static int wheel_gid = -1;
    int id;
    /* Look up root-like gids if needed.  A later PCP client that
       matches any of these group-id's is treated as if root/adm,
       i.e., journal records are not filtered for them (wildcard_p).
       XXX: we could  examine group-membership lists and check against
       uid to also set wildcard_p. */
    if (! rootlike_gids_found) {
        struct group *grp;
        grp = getgrnam("adm");
        if (grp) adm_gid = grp->gr_gid;
        grp = getgrnam("wheel");
        if (grp) wheel_gid = grp->gr_gid;
        rootlike_gids_found = 1;
    }

    enlarge_ctxtab(context);
    assert (ctxtab != NULL && context < ctxtab_size);

    /* NB: we maintain separate uid_p and gid_p for filtering
       purposes; it's possible that a pcp client might send only
       PCP_ATTR_USERID, leaving gid=0, possibly leading us to
       misinterpret that as GROUPID=0 (root) and sending back _GID=0
       records. */
    switch (attr) {
    case PCP_ATTR_USERID:
        ctxtab[context].uid_p = 1;
        id = atoi(value);
        ctxtab[context].uid = id;
        if (id == 0) /* root */
            ctxtab[context].wildcard_p = 1;
        break;

    case PCP_ATTR_GROUPID:
        ctxtab[context].gid_p = 1;
        id = atoi(value);
        ctxtab[context].gid = id;
        if (id == adm_gid ||
            id == wheel_gid )
            ctxtab[context].wildcard_p = 1;
        break;
    }

    if(atoi(value) != 0)
	__pmNotifyErr(LOG_DEBUG, "non-root attempted access, uid: %d\n", atoi(value));
    else
	__pmNotifyErr(LOG_DEBUG, "root attempted access uid: %d\n", atoi(value));
    return 0;
}

static void
papi_end_contextCallBack(int context)
{
    int i, j;
    for(i = 0; i < NumEvents; i++){
	if (ctxtab[context].trackers[i] == 1){
	    ctxtab[context].trackers[i] = 0; // turn it off for us
	    for(j = 0; j < ctxtab_size; j++){
		if(ctxtab[j].trackers[i] == 1){
		    return; // this shouldn't be return, but we should skip this metric for now, continue on and check there aren't any other value's we're no longer using, and that nobody else is either.
		}
		// if nobody else is using the metric, we can turn it off now
	    }
	    enablers[i] = 0;
	    switch (i) {
	    case 0:
		PAPI_remove_event(EventSet, PAPI_TOT_INS);
		break;

	    case 2:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[1]; /* papi.kernel.L1_DCM */
		break;

	    case 3:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[2]; /* papi.kernel.L1_ICM */
		break;

	    case 4:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[3]; /* papi.kernel.L2_DCM */
		break;

	    case 5:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[4]; /* papi.kernel.L2_ICM */
		break;

	    case 6:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[5]; /* papi.kernel.L3_DCM */
		break;

	    case 7:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[6]; /* papi.kernel.L3_ICM */
		break;

	    case 8:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[7]; /* papi.kernel.L1_TCM */
		break;

	    case 9:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[8]; /* papi.kernel.L2_TCM */
		break;

	    case 10:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[9]; /* papi.kernel.L3_TCM */
		break;

	    case 11:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[10]; /* papi.kernel.TLB_DM */
		break;

	    case 12:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[11]; /* papi.kernel.TLB_IM */
		break;

	    case 13:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[12]; /* papi.kernel.TLB_TL */
		break;

	    case 14:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[13]; /* papi.kernel.L1_LDM */
		break;

	    case 15:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[14]; /* papi.kernel.L1_STM */
		break;

	    case 16:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[15]; /* papi.kernel.L2_LDM */
		break;

	    case 17:
		PAPI_remove_event(EventSet, values);
		atom->ull = values[16]; /* papi.kernel.L2_STM */
		break;

	    default:
		break;
	    }//switch i
	}//if ctxtab[context].trackers
    }//for
}

void
papi_init(pmdaInterface *dp)
{
    
    int nummetrics = sizeof(metrictab)/sizeof(metrictab[0]);

    if (isDSO){
	int sep = __pmPathSeparator();
	snprintf(mypath, sizeof(mypath), "%s%c" "papi" "%c" "help",
		 pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_6, "papi DSO", mypath);
    } else {
	__pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    dp->comm.flags |= PDU_FLAG_AUTH;

    int internal_init_ret = 0;
    if (internal_init_ret = papi_internal_init() != 0){
	__pmNotifyErr(LOG_DEBUG, "papi_internal_init returned %d\n", internal_init_ret);
	return;
    }
    
    dp->version.six.fetch = papi_fetch;
    //    dp->version.six.store = papi_store;
    dp->version.six.attribute = papi_contextAttributeCallBack;
    pmdaSetEndContextCallBack(dp, papi_end_contextCallBack);
    pmdaSetFetchCallBack(dp, papi_fetchCallBack);
    pmdaInit(dp, NULL, 0, metrictab, nummetrics);

}

static void
usage(void)
{
    fprintf(stderr,
	    "Usage: %s [options]\n\n"
	    "Options:\n"
	    "  -d domain  use domain (numeric) for metrics domain of PMDA\n"
	    "  -l logfile write log into logfile rather than using default log name\n",
	    pmProgname);
    exit(1);
}

/*
 * Set up agent if running as daemon.
 */

int
main(int argc, char **argv)
{
    int sep = __pmPathSeparator();
    int err = 0;
    int c;
    pmdaInterface dispatch;
    char helppath[MAXPATHLEN];

    isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    snprintf(helppath, sizeof(helppath), "%s%c" "papi" "%c" "help",
	     pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_6, pmProgname, PAPI, "papi.log", helppath);    
    while ((c = pmdaGetOpt(argc, argv, "D:d:l:U:?", &dispatch, &err)) != EOF) {
	switch(c) {
	default:
	    err++;
	    break;
	}
    }
    if (err)
	usage();
 
    pmdaOpenLog(&dispatch);
    papi_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
}
