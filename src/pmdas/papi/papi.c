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
    char papi_string_code[8];
    int position;
    int pmns_position;
    //    char *long_descr;
    //    char *short_descr;
} papi_m_user_tuple;

static papi_m_user_tuple *papi_info = NULL;
#define CLUSTER_PAPI 0 //we should define this in a header for when these exand possible values
#define NumEvents 100
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
unsigned int number_of_events = 0;

void set_pmns_position(unsigned int i)
{
    switch (papi_info[i].papi_event_code) {
    case PAPI_TOT_INS:
	papi_info[i].pmns_position = 0;
	break;
    case PAPI_TOT_CYC:
	papi_info[i].pmns_position = 1;
	break;
    case PAPI_L1_DCM:
	papi_info[i].pmns_position = 2;
	break;
    case PAPI_L1_ICM:
	papi_info[i].pmns_position = 3;
	break;
    case PAPI_L2_DCM:
	papi_info[i].pmns_position = 4;
	break;
    case PAPI_L2_ICM:
	papi_info[i].pmns_position = 5;
	break;
    case PAPI_L3_DCM:
	papi_info[i].pmns_position = 6;
	break;
    case PAPI_L3_ICM:
	papi_info[i].pmns_position = 7;
	break;
    case PAPI_L1_TCM:
	papi_info[i].pmns_position = 8;
	break;
    case PAPI_L2_TCM:
	papi_info[i].pmns_position = 9;
	break;
    case PAPI_L3_TCM:
	papi_info[i].pmns_position = 10;
	break;
    case PAPI_TLB_DM:
	papi_info[i].pmns_position = 11;
	break;
    case PAPI_TLB_IM:
	papi_info[i].pmns_position = 12;
	break;
    case PAPI_TLB_TL:
	papi_info[i].pmns_position = 13;
	break;
    case PAPI_L1_LDM:
	papi_info[i].pmns_position = 14;
	break;
    case PAPI_L1_STM:
	papi_info[i].pmns_position = 15;
	break;
    case PAPI_L2_LDM:
	papi_info[i].pmns_position = 16;
	break;
    case PAPI_L2_STM:
	papi_info[i].pmns_position = 17;
	break;
    case PAPI_CA_SNP:
	papi_info[i].pmns_position = 18;
	break;
    case PAPI_CA_SHR:
	papi_info[i].pmns_position = 19;
	break;
    case PAPI_CA_CLN:
	papi_info[i].pmns_position = 20;
	break;
    case PAPI_CA_INV:
	papi_info[i].pmns_position = 21;
	break;
    case PAPI_CA_ITV:
	papi_info[i].pmns_position = 22;
	break;
    case PAPI_L3_LDM:
	papi_info[i].pmns_position = 23;
	break;
    case PAPI_L3_STM:
	papi_info[i].pmns_position = 24;
	break;
    case PAPI_BRU_IDL:
	papi_info[i].pmns_position = 25;
	break;
    case PAPI_FXU_IDL:
	papi_info[i].pmns_position = 26;
	break;
    case PAPI_FPU_IDL:
	papi_info[i].pmns_position = 27;
	break;
    case PAPI_LSU_IDL:
	papi_info[i].pmns_position = 28;
	break;
    case PAPI_BTAC_M:
	papi_info[i].pmns_position = 29;
	break;
    case PAPI_PRF_DM:
	papi_info[i].pmns_position = 30;
	break;
    case PAPI_L3_DCH:
	papi_info[i].pmns_position = 31;
	break;
    case PAPI_TLB_SD:
	papi_info[i].pmns_position = 32;
	break;
    case PAPI_CSR_FAL:
	papi_info[i].pmns_position = 33;
	break;
    case PAPI_CSR_SUC:
	papi_info[i].pmns_position = 34;
	break;
    case PAPI_CSR_TOT:
	papi_info[i].pmns_position = 35;
	break;
    case PAPI_MEM_SCY:
	papi_info[i].pmns_position = 36;
	break;
    case PAPI_MEM_RCY:
	papi_info[i].pmns_position = 37;
	break;
    case PAPI_MEM_WCY:
	papi_info[i].pmns_position = 38;
	break;
    case PAPI_STL_ICY:
	papi_info[i].pmns_position = 39;
	break;
    case PAPI_FUL_ICY:
	papi_info[i].pmns_position = 40;
	break;
    case PAPI_STL_CCY:
	papi_info[i].pmns_position = 41;
	break;
    case PAPI_FUL_CCY:
	papi_info[i].pmns_position = 42;
	break;
    case PAPI_HW_INT:
	papi_info[i].pmns_position = 43;
	break;
    case PAPI_BR_UCN:
	papi_info[i].pmns_position = 44;
	break;
    case PAPI_BR_CN:
	papi_info[i].pmns_position = 45;
	break;
    case PAPI_BR_TKN:
	papi_info[i].pmns_position = 46;
	break;
    case PAPI_BR_NTK:
	papi_info[i].pmns_position = 47;
	break;
    case PAPI_BR_MSP:
	papi_info[i].pmns_position = 48;
	break;
    case PAPI_BR_PRC:
	papi_info[i].pmns_position = 49;
	break;
    case PAPI_FMA_INS:
	papi_info[i].pmns_position = 50;
	break;
    case PAPI_TOT_IIS:
	papi_info[i].pmns_position = 51;
	break;
    case PAPI_INT_INS:
	papi_info[i].pmns_position = 52;
	break;
    case PAPI_FP_INS:
	papi_info[i].pmns_position = 53;
	break;
    case PAPI_LD_INS:
	papi_info[i].pmns_position = 54;
	break;
    case PAPI_SR_INS:
	papi_info[i].pmns_position = 55;
	break;
    case PAPI_BR_INS:
	papi_info[i].pmns_position = 56;
	break;
    case PAPI_VEC_INS:
	papi_info[i].pmns_position = 57;
	break;
    case PAPI_RES_STL:
	papi_info[i].pmns_position = 58;
	break;
    case PAPI_FP_STAL:
	papi_info[i].pmns_position = 59;
	break;
    case PAPI_LST_INS:
	papi_info[i].pmns_position = 60;
	break;
    case PAPI_SYC_INS:
	papi_info[i].pmns_position = 61;
	break;
    case PAPI_L1_DCH:
	papi_info[i].pmns_position = 62;
	break;
    case PAPI_L2_DCH:
	papi_info[i].pmns_position = 63;
	break;
    case PAPI_L1_DCA:
	papi_info[i].pmns_position = 64;
	break;
    case PAPI_L2_DCA:
	papi_info[i].pmns_position = 65;
	break;
    case PAPI_L3_DCA:
	papi_info[i].pmns_position = 66;
	break;
    case PAPI_L1_DCR:
	papi_info[i].pmns_position = 67;
	break;
    case PAPI_L2_DCR:
	papi_info[i].pmns_position = 68;
	break;
    case PAPI_L3_DCR:
	papi_info[i].pmns_position = 69;
	break;
    case PAPI_L1_DCW:
	papi_info[i].pmns_position = 70;
	break;
    case PAPI_L2_DCW:
	papi_info[i].pmns_position = 71;
	break;
    case PAPI_L3_DCW:
	papi_info[i].pmns_position = 72;
	break;
    case PAPI_L1_ICH:
	papi_info[i].pmns_position = 73;
	break;
    case PAPI_L2_ICH:
	papi_info[i].pmns_position = 74;
	break;
    case PAPI_L3_ICH:
	papi_info[i].pmns_position = 75;
	break;
    case PAPI_L1_ICA:
	papi_info[i].pmns_position = 76;
	break;
    case PAPI_L2_ICA:
	papi_info[i].pmns_position = 77;
	break;
    case PAPI_L3_ICA:
	papi_info[i].pmns_position = 78;
	break;
    case PAPI_L1_ICR:
	papi_info[i].pmns_position = 79;
	break;
    case PAPI_L2_ICR:
	papi_info[i].pmns_position = 80;
	break;
    case PAPI_L3_ICR:
	papi_info[i].pmns_position = 81;
	break;
    case PAPI_L1_ICW:
	papi_info[i].pmns_position = 82;
	break;
    case PAPI_L2_ICW:
	papi_info[i].pmns_position = 83;
	break;
    case PAPI_L3_ICW:
	papi_info[i].pmns_position = 84;
	break;
    case PAPI_L1_TCH:
	papi_info[i].pmns_position = 85;
	break;
    case PAPI_L2_TCH:
	papi_info[i].pmns_position = 86;
	break;
    case PAPI_L3_TCH:
	papi_info[i].pmns_position = 87;
	break;
    case PAPI_L1_TCA:
	papi_info[i].pmns_position = 88;
	break;
    case PAPI_L2_TCA:
	papi_info[i].pmns_position = 89;
	break;
    case PAPI_L3_TCA:
	papi_info[i].pmns_position = 90;
	break;
    case PAPI_L1_TCR:
	papi_info[i].pmns_position = 91;
	break;
    case PAPI_L2_TCR:
	papi_info[i].pmns_position = 92;
	break;
    case PAPI_L3_TCR:
	papi_info[i].pmns_position = 93;
	break;
    case PAPI_L1_TCW:
	papi_info[i].pmns_position = 94;
	break;
    case PAPI_L2_TCW:
	papi_info[i].pmns_position = 95;
	break;
    case PAPI_L3_TCW:
	papi_info[i].pmns_position = 96;
	break;
    case PAPI_FML_INS:
	papi_info[i].pmns_position = 97;
	break;
    case PAPI_FAD_INS:
	papi_info[i].pmns_position = 98;
	break;
    case PAPI_FDV_INS:
	papi_info[i].pmns_position = 99;
	break;
    case PAPI_FSQ_INS:
	papi_info[i].pmns_position = 100;
	break;
    case PAPI_FNV_INS:
	papi_info[i].pmns_position = 101;
	break;
    case PAPI_FP_OPS:
	papi_info[i].pmns_position = 102;
	break;
    case PAPI_SP_OPS:
	papi_info[i].pmns_position = 103;
	break;
    case PAPI_DP_OPS:
	papi_info[i].pmns_position = 104;
	break;
    case PAPI_VEC_SP:
	papi_info[i].pmns_position = 105;
	break;
    case PAPI_VEC_DP:
	papi_info[i].pmns_position = 106;
	break;
    case PAPI_REF_CYC:
	papi_info[i].pmns_position = 107;
	break;
    default:
	papi_info[i].pmns_position = -1;
	break;
    }
}

int permission_check(int context)
{
    if ( ctxtab[context].uid == 0 || ctxtab[context].gid == 0 )
	return 1;
    else
	return 0;
}

void expand_papi_info(int size)
{
    if (number_of_events <= size)
	{
	    size_t new_size = (size + 1) * sizeof(papi_m_user_tuple);
	    papi_info = realloc(papi_info, new_size);
	    if (papi_info == NULL)
		__pmNoMem("papi_info tuple", new_size, PM_FATAL_ERR);
	    while(number_of_events <= size)
		memset(&papi_info[number_of_events++], 0, sizeof(papi_m_user_tuple));
	}
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
    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.TOT_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.TOT_CYC */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_DCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_ICM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_DCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_ICM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_DCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_ICM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_TCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_TCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_TCM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.TLB_DM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.TLB_IM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.TLB_TL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_LDM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_STM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_LDM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_STM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.CA_SNP */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.CA_SHR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.CA_CLN */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.CA_INV */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.CA_ITV */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_LDM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_STM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.BRU_IDL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.FXU_IDL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.FPU_IDL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.LSU_IDL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.BTAC_M */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.PRF_DM */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_DCH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.TLB_SD */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.CSR_FAL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.CSR_SUC */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.CSR_TOT */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.MEM_SCY */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.MEM_RCY */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.MEM_WCY */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.STL_ICY */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.FUL_ICY */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.STL_CCY */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.FUL_CCY */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,43), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.HW_INT */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,44), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.BR_UCN */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.BR_CN */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,46), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.BR_TKN */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,47), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.BR_NTK */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.BR_MSP */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,49), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.BR_PRC */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,50), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.FMA_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.TOT_IIS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.INT_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.FP_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.LD_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.SR_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,56), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.BR_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,57), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.VEC_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,58), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.RES_STL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,59), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.FP_STAL */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.LST_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.SYC_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,62), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_DCH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_DCH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,64), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_DCA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,65), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_DCA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,66), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_DCA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,67), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_DCR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,68), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_DCR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,69), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_DCR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,70), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_DCW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,71), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_DCW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,72), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_DCW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,73), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_ICH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,74), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_ICH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,75), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_ICH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,76), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_ICA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,77), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_ICA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,78), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_ICA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,79), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_ICR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,80), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_ICR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,81), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_ICR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,82), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_ICW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,83), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_ICW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,84), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_ICW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,85), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_TCH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,86), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_TCH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,87), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_TCH */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,88), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_TCA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,89), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_TCA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,90), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_TCA */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,91), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_TCR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,92), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_TCR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,93), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_TCR */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,94), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L1_TCW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,95), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L2_TCW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,96), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.L3_TCW */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,97), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.FML_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,98), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.FAD_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,99), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.FDV_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,100), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.FSQ_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,101), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.FNV_INS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,102), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.FP_OPS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,103), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.SP_OPS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,104), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.DP_OPS */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,105), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.VEC_SP */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,106), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.VEC_DP */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,107), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.preset.REF_CYC */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,1000), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.enable */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,1001), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.reset */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,1002), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.disable */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,1003), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.status */

    { NULL,
      { PMDA_PMID(CLUSTER_PAPI,1004), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }, /* papi.num_counters */

};

static int
papi_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int sts = 0;
    int i;
    /* this will probably need to be expanded to fit the domains as well */
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
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
	case 18:
	case 19:
	case 20:
	case 21:
	case 22:
	case 23:
	case 24:
	case 25:
	case 26:
	case 27:
	case 28:
	case 29:
	case 30:
	case 31:
	case 32:
	case 33:
	case 34:
	case 35:
	case 36:
	case 37:
	case 38:
	case 39:
	case 40:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 46:
	case 47:
	case 48:
	case 49:
	case 50:
	case 51:
	case 52:
	case 53:
	case 54:
	case 55:
	case 56:
	case 57:
	case 58:
	case 59:
	case 60:
	case 61:
	case 62:
	case 63:
	case 64:
	case 65:
	case 66:
	case 67:
	case 68:
	case 69:
	case 70:
	case 71:
	case 72:
	case 73:
	case 74:
	case 75:
	case 76:
	case 77:
	case 78:
	case 79:
	case 80:
	case 81:
	case 82:
	case 83:
	case 84:
	case 85:
	case 86:
	case 87:
	case 88:
	case 89:
	case 90:
	case 91:
	case 92:
	case 93:
	case 94:
	case 95:
	case 96:
	case 97:
	case 98:
	case 99:
	case 100:
	case 101:
	case 102:
	case 103:
	case 104:
	case 105:
	case 106:
	case 107:

	    // the 'case' && 'idp->item' value we get is the pmns_position
	    for (i = 0; i < number_of_events; i++)
		{
		    if (papi_info[i].pmns_position == idp->item){
			__pmNotifyErr(LOG_DEBUG, "value: %lld (%s)(%d - %d)\n", values[papi_info[i].position], papi_info[i].papi_string_code, idp->item, i);
			atom->ull = values[papi_info[i].position];
			break;
		    }
		}
	    break;


	case 1000:
	    atom->cp = enable_string; /* papi.enable */
	    return PMDA_FETCH_STATIC;

	case 1001:
	    //	    break; /* papi.reset */
	    //	    atom->cp = reset_string;
	    return PMDA_FETCH_STATIC;

	case 1002:
	    if ((sts = check_papi_state(sts)) == PAPI_RUNNING) {
		    atom->cp = disable_string; /* papi.disable */
		    return PMDA_FETCH_STATIC;
		} else
		    return 0;

	case 1003:
	    atom->cp = papi_string_status(); /* papi.status */
	    return PMDA_FETCH_STATIC;

	case 1004:
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
remove_metric(unsigned int event, int position)
{
    int retval = 0;
    int state = 0;
    int restart = 0; // bool to restart running values at the end
    int i;
    long_long new_values[NumEvents];

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
	/* first, copy the values over to new array */
	for(i = 0; i < number_of_events; i++)
	    new_values[i] = values[i];

	/* due to papi bug, we need to fully destroy the eventset and restart it*/
	memset (values, 0, sizeof(values[0])*NumEvents);
	retval = PAPI_cleanup_eventset(EventSet);
	if(retval != PAPI_OK)
	    return retval;

	retval = PAPI_destroy_eventset(&EventSet);
	if(retval != PAPI_OK)
	    return retval;

	number_of_active_counters--;
	// set event we're removing position to -1
	papi_info[position].position = -1;
	retval = PAPI_create_eventset(&EventSet);
	if(retval != PAPI_OK)
	    return retval;

	// run through all metrics and adjust position variable as needed
	for(i = 0; i < number_of_events; i++)
	    {
		__pmNotifyErr(LOG_DEBUG, "metric %s (%d)", papi_info[i].papi_string_code, papi_info[i].position);
		if(papi_info[i].position < position)
		    values[i] = new_values[i];

		if(papi_info[i].position > position) {
		    papi_info[i].position--;
		    values[i-1] = new_values[i];
		}

		if(papi_info[i].position >= 0){
		    retval = PAPI_add_event(EventSet, papi_info[i].papi_event_code);
		    if (retval != PAPI_OK)
			return retval;
		}
	    }
	if (restart && (number_of_active_counters > 0)){
	    retval = PAPI_start(EventSet);
	    if (retval != PAPI_OK)
		return retval;
	}
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
    long_long new_values[NumEvents];
    int i;
    /* check status of papi */
    state = check_papi_state(state);
    /* add check with number_of_counters */
    /* stop papi if running? */
    if (state == PAPI_RUNNING) {
	for (i = 0; i < NumEvents; i++)
	    new_values[i] = values[i];
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
	for (i = 0; i < NumEvents; i++)
	    values[i] = new_values[i];
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
	case 1000: //papi.enable
	    if((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				    PM_TYPE_STRING, &av, PM_TYPE_STRING)) >= 0){
		free(enable_string);
		enable_string = av.cp;
		substring = strtok(enable_string, delim);
		while(substring != NULL){
		    for(j = 0; j < number_of_events; j++){
			if(!strcmp(substring, papi_info[j].papi_string_code)){
			    // add the metric to the set
			    retval = add_metric(papi_info[j].papi_event_code);
			    if (retval == PAPI_OK)
				papi_info[j].position = number_of_active_counters-1; //minus one because array's start at 0 (not 1)
			}
		    }
		    substring = strtok(NULL, delim);
		}
		break;
	    } //if sts
	case 1001: //papi.reset
	    //	    sts = check_papi_state(sts);
	    //	    if(sts == PAPI_RUNNING){
	    //	    if((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
	    //				     PM_TYPE_STRING, &av, PM_TYPE_STRING)) >= 0){
		retval = PAPI_reset(EventSet);
		//		__pmNotifyErr(LOG_DEBUG, "reset: %d\n", retval);
		//		if (retval == PAPI_OK)
		    return 0;
		    //		else
		    //		    return PM_ERR_VALUE;
		    //	    }
	case 1002: //papi.disable
	    if((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				     PM_TYPE_STRING, &av, PM_TYPE_STRING)) >= 0){
		free(disable_string);
		disable_string = av.cp;
		substring = strtok(disable_string, delim);
		while(substring != NULL){
		    //		    for(j = 0; j < (sizeof(papi_info)/sizeof(papi_m_user_tuple)); j++){
		    for(j = 0; j < number_of_events; j++){
			if(!strcmp(substring, papi_info[j].papi_string_code)){
			    // remove the metric from the set
			    retval = remove_metric(papi_info[j].papi_event_code, j);
			    if (retval == PAPI_OK)
				papi_info[j].position = -1;
			}
			else
			    __pmNotifyErr(LOG_DEBUG, "Provided metric name: %s, does not match any known metrics\n", substring);
		    }
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
    int i;
    int position = -1;
    PAPI_event_info_t info;
    ec = 0 | PAPI_PRESET_MASK;
    PAPI_enum_event(&ec, PAPI_ENUM_FIRST);
    __pmID_int *pmidp = (__pmID_int*)&ident;
    for(i = 0; i < number_of_events; i++){
	if(pmidp->item == papi_info[i].pmns_position){
	    position = i;
	    break;
	}
    }

    do{
	if(PAPI_get_event_info(ec, &info) == PAPI_OK){
	    if(info.event_code == papi_info[position].papi_event_code) {
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
    unsigned int i = 0;
    number_of_counters = PAPI_num_counters();
    if (number_of_counters < 0){
	__pmNotifyErr(LOG_ERR, "hardware does not support hardware counters\n");
	return 1;
    }
    int ec;
    int addunderscore;
    PAPI_event_info_t info;
    char *substr;
    char concatstr[10] = {};
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
    do{
	if(PAPI_get_event_info(ec, &info) == PAPI_OK){
	    i++;
	    expand_papi_info(i);
	    papi_info[i-1].papi_event_code = info.event_code;
	    substr = strtok(info.symbol, "_");
	    while(substr != NULL)
		{
		    addunderscore = 0;
		    if(strcmp("PAPI",substr)){
			addunderscore = 1;
			strcat(concatstr, substr);
		    }
		    substr = strtok(NULL, "_");
		    if(substr != NULL && addunderscore){
			strcat(concatstr, "_");
		    }
		}
	    strcpy(papi_info[i-1].papi_string_code, concatstr);
	    memset(&concatstr[0], 0, sizeof(concatstr));
	    papi_info[i-1].position = -1;
	    set_pmns_position(i-1);
	    __pmNotifyErr(LOG_DEBUG, "pmns_position: %d - %s\n", papi_info[i-1].pmns_position, papi_info[i-1].papi_string_code);
	}
    }while(PAPI_enum_event(&ec, 0) == PAPI_OK);

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
