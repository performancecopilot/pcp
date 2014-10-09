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
#if defined(HAVE_GRP_H)
#include <grp.h>
#endif
#include <string.h>


enum {
    CLUSTER_PAPI = 0,	// hardware event counters
    CLUSTER_CONTROL,	// control variables
    CLUSTER_AVAILABLE,	// available hardware
};

typedef struct {
    char papi_string_code[PAPI_HUGE_STR_LEN]; //same length as the papi 'symbol' or name of the event
    pmID pmid;
    int position;
    long_long prev_value;
    PAPI_event_info_t info;
} papi_m_user_tuple;

static papi_m_user_tuple *papi_info;
static unsigned int number_of_events; /* cardinality of papi_info[] */

static char     isDSO = 1; /* == 0 if I am a daemon */
static int      EventSet = PAPI_NULL;
static long_long *values;
struct uid_gid_tuple {
    char uid_p; char gid_p; /* uid/gid received flags. */
    int uid; int gid; /* uid/gid received from PCP_ATTR_* */
}; 
static struct uid_gid_tuple *ctxtab;
static int ctxtab_size;
static int number_of_counters;
static unsigned int number_of_active_counters;
static unsigned int size_of_active_counters;
static unsigned int number_of_events;
static __pmnsTree *papi_tree;

static char helppath[MAXPATHLEN];

static int
permission_check(int context)
{
    if (ctxtab[context].uid_p && ctxtab[context].uid == 0)
	return 1;
    return 0;
}

static void
expand_papi_info(int size)
{
    if (number_of_events <= size) {
	size_t new_size = (size + 1) * sizeof(papi_m_user_tuple);
	papi_info = realloc(papi_info, new_size);
	if (papi_info == NULL)
	    __pmNoMem("papi_info tuple", new_size, PM_FATAL_ERR);
	while (number_of_events <= size)
	    memset(&papi_info[number_of_events++], 0, sizeof(papi_m_user_tuple));
    }
}

static void
expand_values(int size)  // XXX: collapse into expand_papi_info()
{
    if (size_of_active_counters <= size) {
	size_t new_size = (size + 1) * sizeof(long_long);
	values = realloc(values, new_size);
	if (values == NULL)
	    __pmNoMem("values", new_size, PM_FATAL_ERR);
	while (size_of_active_counters <= size) {
	    memset(&values[size_of_active_counters++], 0, sizeof(long_long));
	    if (pmDebug & DBG_TRACE_APPL0) {
		__pmNotifyErr(LOG_DEBUG, "memsetting to zero, %d counters\n",
				size_of_active_counters);
	    }
	}
    }
}

static void
enlarge_ctxtab(int context)
{
    /* Grow the context table if necessary. */
    if (ctxtab_size /* cardinal */ <= context /* ordinal */) {
        size_t need = (context + 1) * sizeof(struct uid_gid_tuple);
        ctxtab = realloc(ctxtab, need);
        if (ctxtab == NULL)
            __pmNoMem("papi ctx table", need, PM_FATAL_ERR);
        /* Blank out new entries. */
        while (ctxtab_size <= context)
            memset(&ctxtab[ctxtab_size++], 0, sizeof(struct uid_gid_tuple));
    }
}

static int
check_papi_state()
{
    int state = 0;
    int retval = 0;
    retval = PAPI_state(EventSet, &state);
    if (retval != PAPI_OK)
	return retval;
    return state;
}

/*
 * A list of all the papi metrics we support - 
 */
static pmdaMetric metrictab[] = {
    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.TOT_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.TOT_CYC */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_DCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_ICM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_DCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_ICM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_DCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_ICM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_TCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_TCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_TCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.TLB_DM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.TLB_IM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.TLB_TL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_LDM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_STM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_LDM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_STM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.CA_SNP */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.CA_SHR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.CA_CLN */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.CA_INV */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.CA_ITV */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_LDM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_STM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.BRU_IDL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.FXU_IDL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.FPU_IDL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.LSU_IDL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.BTAC_M */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.PRF_DM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_DCH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.TLB_SD */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.CSR_FAL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.CSR_SUC */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.CSR_TOT */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.MEM_SCY */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.MEM_RCY */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.MEM_WCY */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.STL_ICY */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.FUL_ICY */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.STL_CCY */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.FUL_CCY */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,43), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.HW_INT */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,44), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.BR_UCN */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.BR_CN */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,46), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.BR_TKN */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,47), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.BR_NTK */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.BR_MSP */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,49), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.BR_PRC */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,50), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.FMA_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.TOT_IIS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.INT_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.FP_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.LD_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.SR_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,56), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.BR_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,57), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.VEC_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,58), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.RES_STL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,59), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.FP_STAL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.LST_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.SYC_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,62), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_DCH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_DCH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,64), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_DCA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,65), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_DCA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,66), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_DCA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,67), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_DCR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,68), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_DCR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,69), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_DCR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,70), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_DCW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,71), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_DCW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,72), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_DCW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,73), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_ICH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,74), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_ICH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,75), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_ICH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,76), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_ICA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,77), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_ICA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,78), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_ICA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,79), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_ICR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,80), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_ICR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,81), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_ICR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,82), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_ICW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,83), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_ICW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,84), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_ICW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,85), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_TCH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,86), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_TCH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,87), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_TCH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,88), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_TCA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,89), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_TCA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,90), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_TCA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,91), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_TCR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,92), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_TCR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,93), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_TCR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,94), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L1_TCW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,95), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L2_TCW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,96), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.L3_TCW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,97), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.FML_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,98), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.FAD_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,99), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.FDV_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,100), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.FSQ_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,101), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.FNV_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,102), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.FP_OPS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,103), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.SP_OPS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,104), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.DP_OPS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,105), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.VEC_SP */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,106), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.VEC_DP */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,107), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.REF_CYC */

    { NULL,
      { PMDA_PMID(CLUSTER_CONTROL,0), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.control.enable */

    { NULL,
      { PMDA_PMID(CLUSTER_CONTROL,1), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.control.reset */

    { NULL,
      { PMDA_PMID(CLUSTER_CONTROL,2), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.control.disable */

    { NULL,
      { PMDA_PMID(CLUSTER_CONTROL,3), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.control.status */

    { NULL,
      { PMDA_PMID(CLUSTER_AVAILABLE,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.available.num_counters */

};

static void
papi_endContextCallBack(int context)
{
    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "end context %d received\n", context);

    /* ensure clients re-using this slot re-authenticate */
    if (context >= 0 && context < ctxtab_size) {
	ctxtab[context].uid_p = 0;
	ctxtab[context].gid_p = 0;
    }
}

static int
papi_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int *idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int running = 0;
    int retval = 0;
    int i;
    int state;
    char local_string[32];
    char status_string[4096];
    int first_metric = 0;
    retval = check_papi_state();
    if (retval & PAPI_RUNNING && idp->cluster == CLUSTER_PAPI) {
	retval = PAPI_read(EventSet, values);
	if (retval != PAPI_OK) {
	    __pmNotifyErr(LOG_ERR, "PAPI_read: %s\n", PAPI_strerror(retval));
	    return PM_ERR_VALUE;
	}
	running = 1;
    }

    switch (idp->cluster) {
    case CLUSTER_PAPI:
	if (!running)
	    return PMDA_FETCH_NOVALUES;
	if (idp->item >= 0 && idp->item <= number_of_events) {
	    // the 'case' && 'idp->item' value we get is the pmns_position
	    if (papi_info[idp->item].position >= 0) {
		atom->ull = papi_info[idp->item].prev_value + values[papi_info[idp->item].position];
		return PMDA_FETCH_STATIC;
	    }
	    else
		return PMDA_FETCH_NOVALUES;
	}

	return PM_ERR_PMID;

    case CLUSTER_CONTROL:
	switch (idp->item) {
	case 0:
	    atom->cp = ""; /* papi.control.enable */
	    return PMDA_FETCH_STATIC;

	case 1:
	    /* papi.control.reset */
	    atom->cp = "";
	    return PMDA_FETCH_STATIC;

	case 2:
	    /* papi.control.disable */
	    atom->cp = "";
	    if ((retval = check_papi_state()) & PAPI_RUNNING)
		return PMDA_FETCH_STATIC;
	    return 0;

	case 3:
	    retval = PAPI_state(EventSet, &state);
	    if (retval != PAPI_OK)
		return PM_ERR_VALUE;
	    strcpy(status_string, "Papi ");
	    if(state & PAPI_STOPPED)
		strcat(status_string, "is stopped");
	    if (state & PAPI_RUNNING)
		strcat(status_string, "is running, ");
	    if (state & PAPI_PAUSED)
		strcat(status_string,"is paused, ");
	    if (state & PAPI_NOT_INIT)
		strcat(status_string, "is defined but not initialized, ");
	    if (state & PAPI_OVERFLOWING)
		strcat(status_string, "has overflowing enabled, ");
	    if (state & PAPI_PROFILING)
		strcat(status_string, "eventset has profiling enabled, ");
	    if (state & PAPI_MULTIPLEXING)
		strcat(status_string,"has multiplexing enabled, ");
	    if (state & PAPI_ATTACHED)
	        strcat(status_string, "is attached to another process/thread, ");
	    if (state & PAPI_CPU_ATTACHED)
		strcat(status_string, "is attached to a specific CPU, ");

	    for(i = 0; i < number_of_events; i++){
		strcpy(local_string, "");
		if(papi_info[i].position >= 0 && papi_info[i].info.event_code && first_metric == 1){
		    sprintf(local_string, ", %s: %lld", papi_info[i].papi_string_code, (papi_info[i].prev_value + values[papi_info[i].position]));
		    strcat(status_string, local_string);
		}
		if(papi_info[i].position >= 0 && papi_info[i].info.event_code && first_metric == 0){
		    sprintf(local_string, "%s: %lld", papi_info[i].papi_string_code, (papi_info[i].prev_value + values[papi_info[i].position]));
		    first_metric = 1;
		    strcat(status_string, local_string);
		}
	    }
	    atom->cp = status_string;
	    return PMDA_FETCH_STATIC;

	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_AVAILABLE:
	if (idp->item == 0) {
	    atom->ul = number_of_counters; /* papi.available.num_counters */
	    return PMDA_FETCH_STATIC;
	}
	return PM_ERR_PMID;

    default:
	return PM_ERR_PMID;
    }

    return PMDA_FETCH_NOVALUES;
}

static int
papi_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    if (permission_check(pmda->e_context))
	return pmdaFetch(numpmid, pmidlist, resp, pmda);
    return PM_ERR_PERMISSION;
}

static void
handle_papi_error(int error)
{
    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_ERR, "Papi error: %s\n", PAPI_strerror(error));
}

static int
stop_papi()
{
    int retval = 0;
    int state = 0;
    int i;
    state = check_papi_state();
    if (state & PAPI_RUNNING)
	retval = PAPI_stop(EventSet, values);
    if (retval == PAPI_OK) {
	for (i = 0; i < size_of_active_counters; i++){
	    if(papi_info[i].position >= 0 && papi_info[i].info.event_code)
		papi_info[i].prev_value += values[papi_info[i].position];
	}
    }
    else if (retval != PAPI_OK)
	handle_papi_error(retval);
    return retval;
}

static int
remove_metric(int event)
{
    int retval = 0;
    int state = 0;
    int i;
    int position = papi_info[event].position;

    /* check to make sure papi is running, otherwise do nothing */
    retval = stop_papi();
    if (retval != PAPI_OK)
	return PM_ERR_VALUE;
    /* workaround a papi bug: fully destroy the eventset and restart it */
    memset(values, 0, sizeof(values[0])*size_of_active_counters);
    retval = PAPI_cleanup_eventset(EventSet);
    if (retval != PAPI_OK) {
	handle_papi_error(retval);
	return PM_ERR_VALUE;
    }

    retval = PAPI_destroy_eventset(&EventSet);
    if (retval != PAPI_OK) {
	handle_papi_error(retval);
	return PM_ERR_VALUE;
    }

    number_of_active_counters--;
    retval = PAPI_create_eventset(&EventSet);
    if (retval != PAPI_OK) {
	handle_papi_error(retval);
	return PM_ERR_VALUE;
    }

    papi_info[event].prev_value = 0;
    papi_info[event].position = -1;

    for (i = 0; i < number_of_events; i++) {

	if (papi_info[i].position > position)
	    papi_info[i].position--;

	if (papi_info[i].position >= 0 && papi_info[i].info.event_code) {
	    retval = PAPI_add_event(EventSet, papi_info[i].info.event_code);
	    if (retval != PAPI_OK) {
		handle_papi_error(retval);
		return PM_ERR_VALUE;
	    }
	}
    }
    if (number_of_active_counters > 0) {
	retval = PAPI_start(EventSet);
	if (retval != PAPI_OK) {
	    handle_papi_error(retval);
	    return PM_ERR_VALUE;
	}
    }
    return 0;
}

static int
add_metric(unsigned int event)
{
    int retval = 0;
    int state = 0;
    char eventname[PAPI_MAX_STR_LEN];

    retval = stop_papi();
    if (retval != PAPI_OK)
	return PM_ERR_VALUE;
	/* add metric */
    retval = PAPI_add_event(EventSet, event); //XXX possibly switch this to add_events
    if (retval != PAPI_OK) {
	if (pmDebug & DBG_TRACE_APPL0) {
	    PAPI_event_code_to_name(event, &eventname);
	    __pmNotifyErr(LOG_DEBUG, "Unable to add: %s due to error: %s\n", eventname, PAPI_strerror(retval));
	}
	if (number_of_active_counters > 0){
	    retval = PAPI_start(EventSet);
	    if (retval != PAPI_OK)
		handle_papi_error(retval);
	}
	return PM_ERR_VALUE;
    }
    number_of_active_counters++;
    retval = PAPI_start(EventSet);
    if (retval != PAPI_OK) {
	handle_papi_error(retval);
	return PM_ERR_VALUE;
    }
    return 0; //PAPI_OK is equal to zero, so we can compare against that for returns
}


static int
papi_store(pmResult *result, pmdaExt *pmda)
{
    int retval;
    int i, j, len;
    const char *delim = " ,";
    char *substring;

    if (!permission_check(pmda->e_context))
	return PM_ERR_PERMISSION;
    for (i = 0; i < result->numpmid; i++) {
	pmValueSet *vsp = result->vset[i];
	__pmID_int *idp = (__pmID_int *)&(vsp->pmid);
	pmAtomValue av;
	if (idp->cluster != CLUSTER_CONTROL)
	    return PM_ERR_PERMISSION;

	switch (idp->item) {
	case 0: //papi.enable
	    if ((retval = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				 PM_TYPE_STRING, &av, PM_TYPE_STRING)) < 0)
		return retval;
	    len = strlen(av.cp);
	    substring = strtok(av.cp, delim);
	    while (substring != NULL) {
		for (j = 0; j < number_of_events; j++) {
		    if (!strcmp(substring, papi_info[j].papi_string_code) && papi_info[j].position < 0) {
			// add the metric to the set if it's not already there
			retval = add_metric(papi_info[j].info.event_code);
			if (retval == PAPI_OK)
			    papi_info[j].position = number_of_active_counters-1;
			break;
		    }
		    if (j == size_of_active_counters) {
			if (pmDebug & DBG_TRACE_APPL0)
			    __pmNotifyErr(LOG_DEBUG, "metric name %s does not match any known metrics and will not be added\n", substring);
			retval = 1;
		    }
                }
                if (j == number_of_events) {
                    if (pmDebug & DBG_TRACE_APPL0)
                        __pmNotifyErr(LOG_DEBUG, "metric name %s does not match any known metrics\n", substring);
                    retval = 1;
                    /* NB: continue for other event names that may succeed */
		}
		substring = strtok(NULL, delim);
	    }
	    if (retval)
		return PM_ERR_CONV;
	    break;

	case 1: //papi.reset
	    retval = PAPI_reset(EventSet);
	    if (pmDebug & DBG_TRACE_APPL0)
		__pmNotifyErr(LOG_DEBUG, "reset: %d\n", retval);
	    if (retval != PAPI_OK)
		return PM_ERR_VALUE;
	    for (i = 0; i < number_of_events; i++)
		papi_info[i].prev_value = 0;
	    return 0;

	case 2: //papi.disable
	    if ((retval = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				PM_TYPE_STRING, &av, PM_TYPE_STRING)) < 0)
		return retval;
	    len = strlen(av.cp);
	    substring = strtok(av.cp, delim);
	    while (substring != NULL) {
		for (j = 0; j < size_of_active_counters; j++) {
		    if (!strcmp(substring, papi_info[j].papi_string_code)) {
			// remove the metric from the set
			retval = remove_metric(j);
			if (retval == PAPI_OK)
			    papi_info[j].position = -1;
			break; //we've found the correct metric, break;
		    }
		}
		if (j == size_of_active_counters) {
		    if (pmDebug & DBG_TRACE_APPL0)
			__pmNotifyErr(LOG_DEBUG, "metric name %s does not match any known metrics\n", substring);
		    retval = 1;
		}
		substring = strtok(NULL, delim);
	    }
	    if (retval)
		return PM_ERR_CONV;
	    break;

	default:
	    return PM_ERR_PMID;
	}
    }
    return 0;
}

static int
papi_text(int ident, int type, char **buffer, pmdaExt *ep)
{
    __pmID_int *pmidp = (__pmID_int*)&ident;

    /* no indoms - we only deal with metric help text */
    if ((type & PM_TEXT_PMID) != PM_TEXT_PMID)
	return PM_ERR_TEXT;

    if(pmidp->cluster == CLUSTER_PAPI){
	if(pmidp->item < number_of_events){
	    if (type & PM_TEXT_ONELINE)
		*buffer = papi_info[pmidp->item].info.short_descr;
	    else
		*buffer = papi_info[pmidp->item].info.long_descr;
	    return 0;
	}
    }
    else
	return pmdaText(ident, type, buffer, ep);
}

static int
papi_name_lookup(const char *name, pmID *pmid, pmdaExt *pmda)
{
    return pmdaTreePMID(papi_tree, name, pmid);
}

static int
papi_children(const char *name, int traverse, char ***offspring, int **status, pmdaExt *pmda)
{
    return pmdaTreeChildren(papi_tree, name, traverse, offspring, status);
}

static int
papi_internal_init(pmdaInterface *dp)
{
    int ec;
    int retval;
    PAPI_event_info_t info;
    char entry[PAPI_HUGE_STR_LEN]; // the length papi uses for the symbol name
    unsigned int i = 0;
    pmID pmid;

    if ((retval = __pmNewPMNS(&papi_tree)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s failed to create dynamic papi pmns: %s\n",
		      pmProgname, pmErrStr(retval));
	papi_tree = NULL;
	return PM_ERR_GENERIC;
    }

    number_of_counters = PAPI_num_counters();
    if (number_of_counters < 0){
	__pmNotifyErr(LOG_ERR, "hardware does not support hardware counters\n");
	return 1;
    }

    ec = 0 | PAPI_PRESET_MASK;
    retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT) {
	__pmNotifyErr(LOG_DEBUG, "PAPI_library_init error!\n");
	return PM_ERR_GENERIC;
    }

    if ((retval = PAPI_multiplex_init()) != PAPI_OK) {
	__pmNotifyErr(LOG_ERR, "Could not init multiplexing (%s)\n", PAPI_strerror(retval));
	return PM_ERR_GENERIC;
    }

    retval = PAPI_set_domain(PAPI_DOM_ALL);
    if (retval != PAPI_OK) {
	__pmNotifyErr(LOG_DEBUG, "Cannot set the domain to PAPI_DOM_ALL.\n");
	return PM_ERR_GENERIC;
    }

    PAPI_enum_event(&ec, PAPI_ENUM_FIRST);
    do {
	if (PAPI_get_event_info(ec, &info) == PAPI_OK) {
	    if (info.count && PAPI_PRESET_ENUM_AVAIL){
		expand_papi_info(i);
		memcpy(&papi_info[i].info, &info, sizeof(PAPI_event_info_t));
		memcpy(&papi_info[i].papi_string_code, info.symbol + 5, strlen(info.symbol)-5);
		snprintf(entry, sizeof(entry),"papi.system.%s", papi_info[i].papi_string_code);
		pmid = pmid_build(dp->domain, CLUSTER_PAPI, i);
		__pmAddPMNSNode(papi_tree, pmid, entry);
		memset(&entry[0], 0, sizeof(entry));
		papi_info[i].position = -1;

		expand_values(i);
		i++;
	    }
	}
    } while(PAPI_enum_event(&ec, 0) == PAPI_OK);
    pmdaTreeRebuildHash(papi_tree, number_of_events);

    if (PAPI_create_eventset(&EventSet) != PAPI_OK) {
	__pmNotifyErr(LOG_ERR, "PAPI_create_eventset error!\n");
	return PM_ERR_GENERIC;
    }
    if ((retval = PAPI_assign_eventset_component(EventSet, 0))) {
	__pmNotifyErr(LOG_ERR, "Could not assign eventset component. %s\n", PAPI_strerror(retval));
	return PM_ERR_GENERIC;
    }
    if ((retval = PAPI_set_multiplex(EventSet)) != PAPI_OK) {
	__pmNotifyErr(LOG_ERR, "Could not turn on eventset multiplexing. %s %d", PAPI_strerror(retval), retval);
	return PM_ERR_GENERIC;
    }
    return 0;

}

/* use documented in pmdaAttribute(3) */
static int
papi_contextAttributeCallBack(int context, int attr,
			      const char *value, int length, pmdaExt *pmda)
{
    int id = -1;

    enlarge_ctxtab(context);
    assert(ctxtab != NULL && context < ctxtab_size);

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

    default:
	return 0;
    }

    if (id != 0) {
	if (pmDebug & DBG_TRACE_AUTH)
	    __pmNotifyErr(LOG_DEBUG, "access denied attr=%d id=%d\n", attr, id);
	return PM_ERR_PERMISSION;
    }
    else if (pmDebug & DBG_TRACE_AUTH)
	__pmNotifyErr(LOG_DEBUG, "access granted attr=%d id=%d\n", attr, id);

    return 0;
}

void
__PMDA_INIT_CALL
papi_init(pmdaInterface *dp)
{
    int nummetrics = sizeof(metrictab)/sizeof(metrictab[0]);
    int retval;

    if (isDSO) {
	int	sep = __pmPathSeparator();

	snprintf(helppath, sizeof(helppath), "%s%c" "papi" "%c" "help",
		 pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_6, "papi DSO", helppath);
    }

    if (dp->status != 0)
	return;

    dp->comm.flags |= PDU_FLAG_AUTH;

    if ((retval = papi_internal_init(dp)) != 0) {
	__pmNotifyErr(LOG_ERR, "papi_internal_init returned %d\n", retval);
	dp->status = PM_ERR_GENERIC;
	return;
    }

    dp->version.six.fetch = papi_fetch;
    dp->version.six.store = papi_store;
    dp->version.six.attribute = papi_contextAttributeCallBack;
    dp->version.any.text = papi_text;
    dp->version.four.pmid = papi_name_lookup;
    dp->version.four.children = papi_children;
    pmdaSetFetchCallBack(dp, papi_fetchCallBack);
    pmdaSetEndContextCallBack(dp, papi_endContextCallBack);
    pmdaInit(dp, NULL, 0, metrictab, nummetrics);

}

static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

static pmdaOptions opts = {
    .short_options = "D:d:l:?",
    .long_options = longopts,
};

/*
 * Set up agent if running as daemon.
 */
int
main(int argc, char **argv)
{
    int sep = __pmPathSeparator();
    pmdaInterface dispatch;

    isDSO = 0;
    __pmSetProgname(argv[0]);

    snprintf(helppath, sizeof(helppath), "%s%c" "papi" "%c" "help",
	     pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_6, pmProgname, PAPI, "papi.log", helppath);    
    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
 
    pmdaOpenLog(&dispatch);
    papi_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    free(ctxtab);
    free(papi_info);
    free(values);

    exit(0);
}
