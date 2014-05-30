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

#define CLUSTER_PAPI 0 //we should define this in a header for when these exand possible values
static char     *username;
static char     mypath[MAXPATHLEN];
static char     isDSO = 1; /* == 0 if I am a daemon */
static int      EventSet = PAPI_NULL;
static long_long values[17] = {(long_long) 0};
static unsigned int enable_counters = 0;

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

/*
 * A list of all the papi metrics we support - 
 */
static pmdaMetric metrictab[] = {

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.total_inst */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.enable_counters */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L1_DCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L1_ICM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L2_DCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L2_ICM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L3_DCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L3_ICM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L1_TCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L2_TCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L3_TCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.TLB_DM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.TLB_IM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.TLB_TL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L1_LDM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L1_STM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L2_LDM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.kernel.L2_STM */

};

static int
papi_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    // XXX is a straight PAPI_read appropriate? what if the values overflow?
    // should we be using PAPI_accum instead?
    switch (idp->cluster) {
    case CLUSTER_PAPI:
	//switch indom statement will end up being here
	switch (idp->item) {
	case 0:
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
	    atom->ull = values[15]; /* papi.kernel.L1_LDM */
	    break;

	case 17:
	    PAPI_read(EventSet, values);
	    atom->ull = values[16]; /* papi.kernel.L1_STM */
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
    PAPI_read(EventSet, values);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

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
	if(idp->item == 1) {
	    if((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				    PM_TYPE_U32, &av, PM_TYPE_U32)) >= 0){
		if (av.ul > 1) /* only one or zero allowed */
		    sts = PM_ERR_CONV;
		else {
		    enable_counters = av.ul;
		    if (av.ul == 0){
			if(PAPI_stop(EventSet,values) != PAPI_OK){
			    __pmNotifyErr(LOG_DEBUG, "init was hit\n");
			    printf("papi shutdown error\n");
			    return 1; 
			}
		    }
		    else {
			if (PAPI_start(EventSet) != PAPI_OK){
			    __pmNotifyErr(LOG_DEBUG, "init was hit\n");
			    fprintf(stderr, "couldn't start\n");
			    return 1; // XXX check return value correct
			}			
		    }
		}
	    }
	}
    }

    PAPI_read(EventSet, values);
    return 0;
}

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

    if (PAPI_add_event(EventSet, PAPI_TOT_INS) != PAPI_OK){
	__pmNotifyErr(LOG_DEBUG, "couldn't add PAPI_TOT_INS to event set\n");
	return retval;
    }
    /* We can't just assume everything is here, add checks
       all this papi stuff should probably be done in its own function
       The papi_add_event can probably just be done in papi_add_events
       I need to think of a better, more maliable way to specify what
       event counts we want to enable, having a 'enable ALL the counters'
       approach isn't proper */
    PAPI_add_event(EventSet, PAPI_L1_DCM);
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
    PAPI_add_event(EventSet, PAPI_L2_STM);

    return retval;

}
void
papi_init(pmdaInterface *dp)
{
    
    int nummetrics = sizeof(metrictab)/sizeof(metrictab[0]);

    if (isDSO){
	int sep = __pmPathSeparator();
	snprintf(mypath, sizeof(mypath), "%s%c" "papi" "%c" "help",
		 pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_2, "papi DSO", mypath);
    } else {
	__pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    if (papi_internal_init() != 0){
	__pmNotifyErr(LOG_DEBUG, "No instruction counter? How lame.\n");
	return;
    }
    
    dp->version.any.fetch = papi_fetch;
    dp->version.any.store = papi_store;
    pmdaSetFetchCallBack(dp, papi_fetchCallBack);

    pmdaInit(dp, NULL, 0, metrictab, nummetrics);

}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain  use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile write log into logfile rather than using default log name\n"
	  "  -U username  user account to run under (default \"pcp\")\n",
	  stderr);		
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

    snprintf(helppath, sizeof(helppath), "%s%c" "linux" "%c" "help",
	     pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_6, pmProgname, PAPI, "papi.log", helppath);    
    while ((c = pmdaGetOpt(argc, argv, "D:d:l:U:?", &dispatch, &err)) != EOF) {
	switch(c) {
	case 'U':
	    username = optarg;
	    break;
	default:
	    err++;
	}
    }
    if (err)
	usage();

    pmdaOpenLog(&dispatch);
    pmdaConnect(&dispatch);
    papi_init(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
}
