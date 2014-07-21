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
#include <string.h>


typedef struct {
    unsigned int papi_event_code; //the PAPI_ eventcode
    char *papi_string_code;
    //    char *long_descr;
    //    char *short_descr;
} papi_m_user_tuple;

papi_m_user_tuple papi_info[] = {
    { PAPI_TOT_INS, "TOT_INS" },
    { PAPI_TOT_CYC, "TOT_CYC" },
    { PAPI_L1_DCM, "L1_DCM" },
    { PAPI_L1_ICM, "L1_ICM" },
    { PAPI_L2_DCM, "L2_DCM" },
    { PAPI_L2_ICM, "L2_ICM" },
    { PAPI_L3_DCM, "L3_DCM" },
    { PAPI_L3_ICM, "L3_ICM" },
    { PAPI_L1_TCM, "L1_TCM" },
    { PAPI_L2_TCM, "L2_TCM" },
    { PAPI_L3_TCM, "L3_TCM" },
    { PAPI_TLB_DM, "TLB_DM" },
    { PAPI_TLB_IM, "TLB_IM" },
    { PAPI_TLB_TL, "TLB_TL" },
    { PAPI_L1_LDM, "L1_LDM" },
    { PAPI_L1_STM, "L1_STM" },
    { PAPI_L2_LDM, "L2_LDM" },
    { PAPI_L2_STM, "L2_STM" },
};
#define CLUSTER_PAPI 0 //we should define this in a header for when these exand possible values
#define NumEvents 20
static char     *enable_string;
static char     *disable_string;
static char     *username = "root";
static char     mypath[MAXPATHLEN];
static char     isDSO = 1; /* == 0 if I am a daemon */
static int      EventSet = PAPI_NULL;
//static int      NumEvents = 17;
static long_long values[NumEvents] = {(long_long) 0};
static int enablers[NumEvents] = {0};
static unsigned int enable_counters = 0;
struct uid_gid_tuple {
    char uid_p; char gid_p; /* uid/gid received flags. */
    int uid; int gid; /* uid/gid received from PCP_ATTR_* */
    int trackers[20];
}; 
static struct uid_gid_tuple *ctxtab = NULL;
int ctxtab_size = 0;
int number_of_counters = 0;
unsigned int number_of_active_counters = 0;

int permission_check(int context)
{
    if ( ctxtab[context].uid == 0 || ctxtab[context].gid == 0 )
	return 1;
    else
	return 0;
}
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

int
check_papi_state(int state)
{
    int retval = 0;
    retval = PAPI_state(EventSet, &state);
    if (retval != PAPI_OK)
	return PM_ERR_NODATA;
    return state;
}

char*
papi_string_status()
{
    int state, retval;
    retval = PAPI_state(EventSet, &state);
    if (retval != PAPI_OK)
	return "PAPI_state error.";
    switch (state) {
    case 1:
	return "Papi is stopped.";
    case 2:
	return "Papi is running.";
    case 4:
	return "Papi is paused";
    case 8:
	return "Papi eventset is defined but not initialized.";
    case 16:
	return "Papi eventset has overflowing enabled";
    case 32:
	return "Papi eventset has profiling enabled";
    case 64:
	return "Papi eventset has multiplexing enabled";
    case 128:
	return "Papi is attached to another process/thread";
    case 256:
	return "Papi is attached to a specific CPU.";
    default:
	return "PAPI_state error.";
    }
}

/*void size_user_tuple()
{
    //this will eventually be used in tandom with PAPI_enum_event for
    // 'dynamic' growth of pmsn
    int i;
    size_t result = NumEvents * sizeof(struct papi_m_user_tuple);
    papi_info = realloc(papi_info, result);
    if (papi_info == NULL)
	__pmNoMem("papi_info struct", result, PM_FATAL_ERR);
	}*/

/*
 * A list of all the papi metrics we support - 
 */
static pmdaMetric metrictab[] = {
    { &papi_info[1],
      { PMDA_PMID(CLUSTER_PAPI,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.TOT_INS */

    { &papi_info[2],
      { PMDA_PMID(CLUSTER_PAPI,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.TOT_CYC */

    { &papi_info[3],
      { PMDA_PMID(CLUSTER_PAPI,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_DCM */

    { &papi_info[4],
      { PMDA_PMID(CLUSTER_PAPI,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_ICM */

    { &papi_info[5],
      { PMDA_PMID(CLUSTER_PAPI,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_DCM */

    { &papi_info[6],
      { PMDA_PMID(CLUSTER_PAPI,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_ICM */

    { &papi_info[7],
      { PMDA_PMID(CLUSTER_PAPI,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_DCM */

    { &papi_info[8],
      { PMDA_PMID(CLUSTER_PAPI,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_ICM */

    { &papi_info[9],
      { PMDA_PMID(CLUSTER_PAPI,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_TCM */

    { &papi_info[10],
      { PMDA_PMID(CLUSTER_PAPI,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_TCM */

    { &papi_info[11],
      { PMDA_PMID(CLUSTER_PAPI,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_TCM */

    { &papi_info[12],
      { PMDA_PMID(CLUSTER_PAPI,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.TLB_DM */

    { &papi_info[13],
      { PMDA_PMID(CLUSTER_PAPI,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.TLB_IM */

    { &papi_info[14],
      { PMDA_PMID(CLUSTER_PAPI,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.TLB_TL */

    { &papi_info[15],
      { PMDA_PMID(CLUSTER_PAPI,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_LDM */

    { &papi_info[16],
      { PMDA_PMID(CLUSTER_PAPI,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_STM */

    { &papi_info[17],
      { PMDA_PMID(CLUSTER_PAPI,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_LDM */

    { &papi_info[18],
      { PMDA_PMID(CLUSTER_PAPI,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_STM */

    { &papi_info,
      { PMDA_PMID(CLUSTER_PAPI,18), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.enable */

    { &papi_info,
      { PMDA_PMID(CLUSTER_PAPI,19), PM_TYPE_UNKNOWN, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.reset */

    { &papi_info,
      { PMDA_PMID(CLUSTER_PAPI,20), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.disable */

    { &papi_info,
      { PMDA_PMID(CLUSTER_PAPI,21), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.status */

    { &papi_info,
      { PMDA_PMID(CLUSTER_PAPI,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.num_counters */

};

static int
papi_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int sts = 0;
    int retval = 0;
    /* this will probably need to be expanded to fit the domains as well */
    __pmNotifyErr(LOG_DEBUG, "case: %d\n", idp->item);
    sts = check_papi_state(sts);
    //    if (sts != PAPI_RUNNING){
    //	__pmNotifyErr(LOG_DEBUG, "sts: %d\n");
	//	return PM_ERR_AGAIN;
    //	return 0;
    //    }
    if(sts == PAPI_RUNNING){
	sts = PAPI_read(EventSet, values);
	if (sts != PAPI_OK){
	    __pmNotifyErr(LOG_DEBUG, "sts: %d - %s\n", sts, PAPI_strerror(sts));
	    return PM_ERR_VALUE;
	}
    }
    switch (idp->cluster) {
    case CLUSTER_PAPI:
	//switch indom statement will end up being here
	switch (idp->item) {
	case 0:
	    // might not be the first to have this metric being used so set tracker value
	    atom->ull = values[0]; /* papi.preset.TOT_INS */
	    break;
		
	case 1:
	    atom->ull = values[1]; /* papi.preset.TOT_CYC */
	    break;

	case 2:
	    atom->ull = values[2]; /* papi.preset.L1_DCM */
	    break;

	case 3:
	    atom->ull = values[3]; /* papi.preset.L1_ICM */
	    break;

	case 4:
	    atom->ull = values[4]; /* papi.preset.L2_DCM */
	    break;

	case 5:
	    atom->ull = values[5]; /* papi.preset.L2_ICM */
	    break;

	case 6:
	    atom->ull = values[6]; /* papi.preset.L3_DCM */
	    break;

	case 7:
	    atom->ull = values[7]; /* papi.preset.L3_ICM */
	    break;

	case 8:
	    atom->ull = values[8]; /* papi.preset.L1_TCM */
	    break;

	case 9:
	    atom->ull = values[9]; /* papi.preset.L2_TCM */
	    break;

	case 10:
	    atom->ull = values[10]; /* papi.preset.L3_TCM */
	    break;

	case 11:
	    atom->ull = values[11]; /* papi.preset.TLB_DM */
	    break;

	case 12:
	    atom->ull = values[12]; /* papi.preset.TLB_IM */
	    break;

	case 13:
	    atom->ull = values[13]; /* papi.preset.TLB_TL */
	    break;

	case 14:
	    atom->ull = values[14]; /* papi.preset.L1_LDM */
	    break;

	case 15:
	    atom->ull = values[15]; /* papi.preset.L1_STM */
	    break;

	case 16:
	    atom->ull = values[16]; /* papi.preset.L2_LDM */
	    break;

	case 17:
	    atom->ull = values[17]; /* papi.preset.L2_STM */
	    break;

	case 18:
	    atom->cp = enable_string; /* papi.enable */
	    return PMDA_FETCH_STATIC;

	case 19:
	    break; /* papi.reset */

	case 20:
	    atom->cp = disable_string; /* papi.disable */
	    return PMDA_FETCH_STATIC;

	case 21:
	    atom->cp = papi_string_status(); /* papi.status */
	    return PMDA_FETCH_STATIC;

	case 22:
	    atom->ul = number_of_counters; /* papi.num_counters */
	    return PMDA_FETCH_STATIC;

	default:
	    return 0;
	} // item switch
	break;
    default:
	return 0;
    } // cluster switch
    if (sts == PAPI_OK ) //otherwise it's simply not running, so no papi-metric returned
	return PMDA_FETCH_STATIC;

    return 0;
}

static int
papi_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    if (permission_check(pmda->e_context))
	return pmdaFetch(numpmid, pmidlist, resp, pmda);
    else
	return PM_ERR_PERMISSION;
}

int
remove_metric(unsigned int event)
{
    int retval = 0;
    int state = 0;
    int restart = 0; // bool to restart running values at the end
    /* check to make sure papi is running, otherwise do nothing */
    state = check_papi_state(state);
    if (state == PAPI_RUNNING) {
	restart = 1;
	retval = PAPI_stop(EventSet, values);
	if(retval != PAPI_OK)
	    return retval;
    }
    state = check_papi_state(state);
    if (state == PAPI_STOPPED) {
	/* add metric */
	retval = PAPI_remove_event(EventSet, event); //XXX possibly switch this to remove_events
	if(retval != PAPI_OK)
	    return retval;
	number_of_active_counters--;
	if (restart && (number_of_active_counters > 0))
	    retval = PAPI_start(EventSet);
	return retval;
    }
    else
	return PM_ERR_VALUE; //we couldn't get the metric, proper err value?
    //should not get here
    return PM_ERR_VALUE;
}

int
add_metric(unsigned int event)
{
    int retval = 0;
    int state = 0;
    /* check status of papi */
    state = check_papi_state(state);
    /* add check with number_of_counters */
    /* stop papi if running? */
    if (state == PAPI_RUNNING) {
	retval = PAPI_stop(EventSet, values);
	if(retval != PAPI_OK)
	    return retval;
    }
    state = check_papi_state(state);
    if (state == PAPI_STOPPED) {
	/* add metric */
	retval = PAPI_add_event(EventSet, event); //XXX possibly switch this to add_events
	if(retval != PAPI_OK)
	    return retval;
	number_of_active_counters++;
	retval = PAPI_start(EventSet);
	return retval;
    }
    else
	return PM_ERR_VALUE; //we couldn't get the metric, proper err value?
    //should not get here
    return PM_ERR_VALUE;
}

static int
papi_store(pmResult *result, pmdaExt *pmda)
{
    int sts = 0;
    int i = 0;
    int j = 0;
    int retval;
    const char *delim = " ,";
    char *substring;

    if (!permission_check(pmda->e_context))
	return PM_ERR_PERMISSION;

    for (i = 0; i < result->numpmid; i++){
	pmValueSet *vsp = result->vset[i];
	__pmID_int *idp = (__pmID_int *)&(vsp->pmid);
	pmAtomValue av;
	switch (idp->item){
	case 18: //enable
	    if((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				    PM_TYPE_STRING, &av, PM_TYPE_STRING)) >= 0){
		free(enable_string);
		enable_string = av.cp;
		substring = strtok(enable_string, delim);
		while(substring != NULL){
		    for(j = 0; j < (sizeof(papi_info)/sizeof(papi_m_user_tuple)); j++){
			if(!strcmp(substring, papi_info[j].papi_string_code)){
			    // add the metric to the set
			    retval = add_metric(papi_info[j].papi_event_code);
			}
			else
			    __pmNotifyErr(LOG_DEBUG, "Provided metric name: %s, does not match any known metrics\n", substring);
		    }
		    substring = strtok(NULL, delim);
		}
		break;
	    } //if sts
	case 19: //reset
	    retval = PAPI_reset(EventSet);
	    if (retval == PAPI_OK)
		break;
	    else
		return PM_ERR_VALUE;
	case 20: //disable
	    if((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				     PM_TYPE_STRING, &av, PM_TYPE_STRING)) >= 0){
		free(disable_string);
		disable_string = av.cp;
		substring = strtok(disable_string, delim);
		while(substring != NULL){
		    for(j = 0; j < (sizeof(papi_info)/sizeof(papi_m_user_tuple)); j++){
			if(!strcmp(substring, papi_info[j].papi_string_code)){
			    // remove the metric from the set
			    retval = remove_metric(papi_info[j].papi_event_code);
			}
			else
			    __pmNotifyErr(LOG_DEBUG, "Provided metric name: %s, does not match any known metrics\n", substring);
		    substring = strtok(NULL, delim);
		}

		break;
	    }
	default:
	    sts = PM_ERR_PMID;
	    break;
	}//switch item
    }
    return 0;
}

static int
papi_text(int ident, int type, char **buffer, pmdaExt *ep)
{
    int sts;
    int ec;
    PAPI_event_info_t info;
    int i;
    ec = 0 | PAPI_PRESET_MASK;
    PAPI_enum_event(&ec, PAPI_ENUM_FIRST);
    __pmID_int *pmidp = (__pmID_int*)&ident;
    do{
	if(PAPI_get_event_info(ec, &info) == PAPI_OK){
		if(info.event_code == papi_info[pmidp->item].papi_event_code) {
		    if (type == 5)
			*buffer = info.short_descr;
		    else if(type == 6)
			*buffer = info.long_descr;
		    return 0;
		}
	}
    } while (PAPI_enum_event(&ec, 0)==PAPI_OK);
    sts = pmdaText(ident, type, buffer, ep);
    return sts;
}

int papi_internal_init()
{
    int retval = 0;
    number_of_counters = PAPI_num_counters();
    if (number_of_counters < 0){
	__pmNotifyErr(LOG_ERR, "hardware does not support hardware counters\n");
	return 1;
    }
    int ec;
    PAPI_event_info_t info;
    ec = 0 | PAPI_PRESET_MASK;
    retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT) {
	__pmNotifyErr(LOG_DEBUG, "PAPI library init error!\n");
	return retval;
    }
    /* hack similar to sample pmda */
   enable_string = (char *)malloc(1);
   strcpy(enable_string, "");
   disable_string = (char *)malloc(1);
   strcpy(disable_string, "");
    PAPI_enum_event(&ec, PAPI_ENUM_FIRST);
    if(PAPI_create_eventset(&EventSet) != PAPI_OK){
	__pmNotifyErr(LOG_DEBUG, "create eventset error!\n");
	return retval;
    }
    /* will be used for when we dynamically create the pmns */
    /*    do{
	if (PAPI_get_event_info(ec, &info) == PAPI_OK) {
	    if ( (info.count && PAPI_PRESET_ENUM_AVAIL) ){
		retval = PAPI_add_event(EventSet, info.event_code);
		retval = PAPI_remove_event(EventSet, info.event_code);
	    }
	}
	} while (PAPI_enum_event(&ec, 0) == PAPI_OK);*/
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
    return 0;

}

/* use documented in pmdaAttribute(3) */
static int
papi_contextAttributeCallBack(int context, int attr,
			      const char *value, int length, pmdaExt *pmda)
{
    int id;

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
        break;

    case PCP_ATTR_GROUPID:
        ctxtab[context].gid_p = 1;
        id = atoi(value);
        ctxtab[context].gid = id;
        break;
    }

    if(atoi(value) != 0){
	__pmNotifyErr(LOG_DEBUG, "non-root attempted access, uid: %d\n", atoi(value));
	return PM_ERR_PERMISSION;
    }
    else
	__pmNotifyErr(LOG_DEBUG, "root pmdapapi access uid: %d\n", atoi(value));
    return 0;
}

#if 0
static void
papi_end_contextCallBack(int context)
{
    /*    int i, j;
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
		PAPI_remove_event(EventSet, PAPI_L1_DCM);
		break;
	    case 3:
		PAPI_remove_event(EventSet, PAPI_L1_ICM);
		break;
	    case 4:
		PAPI_remove_event(EventSet, PAPI_L2_DCM);
		break;
	    case 5:
		PAPI_remove_event(EventSet, PAPI_L2_ICM);
		break;
	    case 6:
		PAPI_remove_event(EventSet, PAPI_L3_DCM);
		break;
	    case 7:
		PAPI_remove_event(EventSet, PAPI_L3_ICM);
		break;
	    case 8:
		PAPI_remove_event(EventSet, PAPI_L1_TCM);
		break;
	    case 9:
		PAPI_remove_event(EventSet, PAPI_L2_TCM);
		break;
	    case 10:
		PAPI_remove_event(EventSet, PAPI_L3_TCM);
		break;
	    case 11:
		PAPI_remove_event(EventSet, PAPI_TLB_DM);
		break;
	    case 12:
		PAPI_remove_event(EventSet, PAPI_TLB_IM);
		break;
	    case 13:
		PAPI_remove_event(EventSet, PAPI_TLB_TL);
		break;
	    case 14:
		PAPI_remove_event(EventSet, PAPI_L1_LDM);
		break;
	    case 15:
		PAPI_remove_event(EventSet, PAPI_L1_STM);
		break;
	    case 16:
		PAPI_remove_event(EventSet, PAPI_L2_LDM);
		break;
	    case 17:
		PAPI_remove_event(EventSet, PAPI_L2_STM);
		break;
	    default:
		break;
	    }//switch i
	}//if ctxtab[context].trackers
    }//for
    for(i = 0; i < NumEvents; i++)
	{
	    if(enablers[i] != 0)
		break;
	    if(i++ == NumEvents) //we're at the end
		PAPI_stop(EventSet, values);
	}
    */
    //    __pmNotifyErr(LOG_DEBUG, "hit papi_end_contextcallback\n");
}
#endif

void
papi_init(pmdaInterface *dp)
{
    
    int nummetrics = sizeof(metrictab)/sizeof(metrictab[0]);
    //    size_user_tuple();
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
    if ((internal_init_ret = papi_internal_init()) != 0){
	__pmNotifyErr(LOG_DEBUG, "papi_internal_init returned %d\n", internal_init_ret);
	return;
    }
    
    dp->version.six.fetch = papi_fetch;
    dp->version.six.store = papi_store;
    dp->version.six.attribute = papi_contextAttributeCallBack;
    dp->version.any.text = papi_text;
    //    pmdaSetEndContextCallBack(dp, papi_end_contextCallBack);
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
