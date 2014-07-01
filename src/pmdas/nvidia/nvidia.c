/*
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
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "domain.h"
#include "localnvml.h"

// GCARD_INDOM struct, stats that are per card
typedef struct {

    int			cardid;
    int			failed[12];
    char		*name;
    char		*busid;
    int			temp;
    int			fanspeed;
    int			perfstate;
    nvmlUtilization_t	active;
    nvmlMemory_t	memory;

} nvinfo_t;

// overall struct, holds instance values, indom arrays and instance struct arrays
typedef struct {

    int numcards;
    int maxcards;
    nvinfo_t *nvinfo;
    pmdaIndom *nvindom;

} pcp_nvinfo_t;

// each INDOM, this only has one, corresponding to values that change per card
pmdaIndom indomtab[] = {
#define GCARD_INDOM     0
    { GCARD_INDOM, 0, NULL },
};

static pcp_nvinfo_t pcp_nvinfo;


// list of metrics we want to export
// all in cluster 0, with increasing item ids
static pmdaMetric metrictab[] = {
/* num cards , no indom */
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* card id */
    { NULL, 
      { PMDA_PMID(0,1), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* card name */
    { NULL, 
      { PMDA_PMID(0,2), PM_TYPE_STRING, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* bus id */
    { NULL, 
      { PMDA_PMID(0,3), PM_TYPE_STRING, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* temp */
    { NULL, 
      { PMDA_PMID(0,4), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* fanspeed */
    { NULL, 
      { PMDA_PMID(0,5), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* perf state */
    { NULL, 
      { PMDA_PMID(0,6), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* gpu active */
    { NULL, 
      { PMDA_PMID(0,7), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mem active */
    { NULL, 
      { PMDA_PMID(0,8), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mem used */
    { NULL, 
      { PMDA_PMID(0,9), PM_TYPE_U64, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mem total */
    { NULL, 
      { PMDA_PMID(0,10), PM_TYPE_U64, GCARD_INDOM, PM_SEM_DISCRETE, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mem free */
    { NULL, 
      { PMDA_PMID(0,11), PM_TYPE_U64, GCARD_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
};

static char	mypath[MAXPATHLEN];
static int	isDSO = 1;
static int	nvmlDSO_loaded;

static int
setup_gcard_indom(void)
{
    unsigned int device_count = 0;
    pmdaIndom *idp = &indomtab[GCARD_INDOM];
    char gpuname[32], *name;
    size_t size;
    int i, sts;

    /* Initialize instance domain and instances. */
    if ((sts = localNvmlDeviceGetCount(&device_count)) != NVML_SUCCESS) {
	__pmNotifyErr(LOG_ERR, "nvmlDeviceGetCount: %s",
			localNvmlErrStr(sts));
	return sts;
    }

    pcp_nvinfo.nvindom = idp;
    pcp_nvinfo.nvindom->it_numinst = 0;

    size = device_count * sizeof(pmdaInstid);
    pcp_nvinfo.nvindom->it_set = (pmdaInstid *)malloc(size);
    if (!pcp_nvinfo.nvindom->it_set) {
	__pmNoMem("gcard indom", size, PM_RECOV_ERR);
	return -ENOMEM;
    }

    size = device_count * sizeof(nvinfo_t);
    if ((pcp_nvinfo.nvinfo = (nvinfo_t *)malloc(size)) == NULL) {
	__pmNoMem("gcard values", size, PM_RECOV_ERR);
	free(pcp_nvinfo.nvindom->it_set);
	return -ENOMEM;
    }
    memset(pcp_nvinfo.nvinfo, 0, size);

    for (i = 0; i < device_count; i++) {
	pcp_nvinfo.nvindom->it_set[i].i_inst = i;
	snprintf(gpuname, sizeof(gpuname), "gpu%d", i);
	if ((name = strdup(gpuname)) == NULL) {
	    __pmNoMem("gcard instname", strlen(gpuname), PM_RECOV_ERR);
	    while (--i)
		free(pcp_nvinfo.nvindom->it_set[i].i_name);
	    free(pcp_nvinfo.nvindom->it_set);
	    free(pcp_nvinfo.nvinfo);
	    return -ENOMEM;
	}
	pcp_nvinfo.nvindom->it_set[i].i_name = name;
    }

    pcp_nvinfo.numcards = 0;
    pcp_nvinfo.maxcards = device_count;
    pcp_nvinfo.nvindom->it_numinst = device_count;
    return 0;
}

static int
refresh(pcp_nvinfo_t *pcp_nvinfo)
{
    unsigned int device_count, i, j;
    nvmlDevice_t device;
    char name[NVML_DEVICE_NAME_BUFFER_SIZE];
    nvmlPciInfo_t pci;
    unsigned int fanspeed;
    unsigned int temperature;
    nvmlUtilization_t utilization;
    nvmlMemory_t memory;
    nvmlPstates_t pstate;
    char *failfunc = NULL;
    int sts;

    if (!nvmlDSO_loaded) {
	if (localNvmlInit() == NVML_ERROR_LIBRARY_NOT_FOUND)
	    return 0;
	setup_gcard_indom();
	nvmlDSO_loaded = 1;
    }

    if ((sts = localNvmlDeviceGetCount(&device_count)) != 0) {
	__pmNotifyErr(LOG_ERR, "nvmlDeviceGetCount: %s",
			localNvmlErrStr(sts));
	return sts;
    }

    pcp_nvinfo->numcards = device_count;

    for (i = 0; i < device_count && i < pcp_nvinfo->maxcards; i++) {
	pcp_nvinfo->nvinfo[i].cardid = i;
	for( j = 0; j < 12 ; j++){
	    pcp_nvinfo->nvinfo[i].failed[j] = 0;
	}
	if ((sts = localNvmlDeviceGetHandleByIndex(i, &device))) {
	    failfunc = "nvmlDeviceGetHandleByIndex";
	    for( j = 0; j < 12 ; j++){
            	pcp_nvinfo->nvinfo[i].failed[j] = 1;
            }
	    goto failed;
	}
	if ((sts = localNvmlDeviceGetName(device, name, sizeof(name)))) {
	    pcp_nvinfo->nvinfo[i].failed[2] = 1;
	    //failfunc = "nvmlDeviceGetName";
	    //goto failed;
	}
        if ((sts = localNvmlDeviceGetPciInfo(device, &pci))) {
	    pcp_nvinfo->nvinfo[i].failed[3] = 1;
	    //failfunc = "nvmlDeviceGetPciInfo";
	    //goto failed;
	}
        if ((sts = localNvmlDeviceGetFanSpeed(device, &fanspeed))) {
	    pcp_nvinfo->nvinfo[i].failed[5] = 1;
	    //failfunc = "nvmlDeviceGetFanSpeed";
	    //goto failed;
	}
        if ((sts = localNvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature))) {
	    pcp_nvinfo->nvinfo[i].failed[4] = 1;
	    //failfunc = "nvmlDeviceGetTemperature";
	    //goto failed;
	}
        if ((sts = localNvmlDeviceGetUtilizationRates(device, &utilization))) {
	    pcp_nvinfo->nvinfo[i].failed[7] = 1;
	    pcp_nvinfo->nvinfo[i].failed[8] = 1;
	    //failfunc = "nvmlDeviceGetUtilizationRates";
	    //goto failed;
	}
        if ((sts = localNvmlDeviceGetMemoryInfo(device, &memory))) {
	    pcp_nvinfo->nvinfo[i].failed[9] = 1;
	    pcp_nvinfo->nvinfo[i].failed[10] = 1;
	    pcp_nvinfo->nvinfo[i].failed[11] = 1;
	    //failfunc = "nvmlDeviceGetMemoryInfo";
	    //goto failed;
	}
	if ((sts = localNvmlDeviceGetPerformanceState(device, &pstate))) {
	    pcp_nvinfo->nvinfo[i].failed[6] = 1;
	    //failfunc = "nvmlDeviceGetPerformanceState";
	    //goto failed;
	}

	if (pcp_nvinfo->nvinfo[i].name == NULL)
	    pcp_nvinfo->nvinfo[i].name = strdup(name);
	if (pcp_nvinfo->nvinfo[i].busid == NULL)
	    pcp_nvinfo->nvinfo[i].busid = strdup(pci.busId);
	pcp_nvinfo->nvinfo[i].temp = temperature;
	pcp_nvinfo->nvinfo[i].fanspeed = fanspeed;
	pcp_nvinfo->nvinfo[i].perfstate = pstate;
	pcp_nvinfo->nvinfo[i].active = utilization;	/* struct copy */
	pcp_nvinfo->nvinfo[i].memory = memory;		/* struct copy */
	continue;

    failed:
	__pmNotifyErr(LOG_ERR, "%s: %s", failfunc, localNvmlErrStr(sts));
	//pcp_nvinfo->nvinfo[i].failed = 1;
    }

    return 0;
}

/*
 * Wrapper for pmdaFetch which refresh the set of values once per fetch
 * PDU.  The fetchCallback is then called once per-metric/instance pair
 * to perform the actual filling of the pmResult (via each pmAtomValue).
 */
static int
nvidia_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    refresh(&pcp_nvinfo);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
nvidia_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int	*idp = (__pmID_int *)&(mdesc->m_desc.pmid);

    if (idp->cluster != 0)
	return PM_ERR_PMID;
    if (idp->item != 0 && inst > indomtab[GCARD_INDOM].it_numinst)
	return PM_ERR_INST;

    switch (idp->item) {
        case 0:
            atom->ul = pcp_nvinfo.numcards;
            break;
        case 1:
            atom->ul = pcp_nvinfo.nvinfo[inst].cardid;
            break;
        case 2:
	    if (pcp_nvinfo.nvinfo[inst].failed[2])
		return PM_ERR_VALUE;
            atom->cp = pcp_nvinfo.nvinfo[inst].name;
            break;
        case 3:
	    if (pcp_nvinfo.nvinfo[inst].failed[3])
		return PM_ERR_VALUE;
            atom->cp = pcp_nvinfo.nvinfo[inst].busid;
            break;
        case 4:
	    if (pcp_nvinfo.nvinfo[inst].failed[4])
		return PM_ERR_VALUE;
            atom->ul = pcp_nvinfo.nvinfo[inst].temp;
            break;
        case 5:
	    if (pcp_nvinfo.nvinfo[inst].failed[5])
		return PM_ERR_VALUE;
            atom->ul = pcp_nvinfo.nvinfo[inst].fanspeed;
            break;
        case 6:
	    if (pcp_nvinfo.nvinfo[inst].failed[6])
		return PM_ERR_VALUE;
            atom->ul = pcp_nvinfo.nvinfo[inst].perfstate;
            break;
        case 7:
	    if (pcp_nvinfo.nvinfo[inst].failed[7])
		return PM_ERR_VALUE;
            atom->ul = pcp_nvinfo.nvinfo[inst].active.gpu;
            break;
        case 8:
	    if (pcp_nvinfo.nvinfo[inst].failed[8])
		return PM_ERR_VALUE;
            atom->ul = pcp_nvinfo.nvinfo[inst].active.memory;
            break;
        case 9:
	    if (pcp_nvinfo.nvinfo[inst].failed[9])
		return PM_ERR_VALUE;
            atom->ull = pcp_nvinfo.nvinfo[inst].memory.used;
            break;
        case 10:
	    if (pcp_nvinfo.nvinfo[inst].failed[10])
		return PM_ERR_VALUE;
            atom->ull = pcp_nvinfo.nvinfo[inst].memory.total;
            break;
        case 11:
	    if (pcp_nvinfo.nvinfo[inst].failed[11])
		return PM_ERR_VALUE;
            atom->ull = pcp_nvinfo.nvinfo[inst].memory.free;
            break;
        default:
            return PM_ERR_PMID;
    }

    return 0;
}

/**
 * Initializes the path to the help file for this PMDA.
 */
static void
initializeHelpPath()
{
    int sep = __pmPathSeparator();
    snprintf(mypath, sizeof(mypath), "%s%c" "nvidia" "%c" "help",
            pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
}

void 
__PMDA_INIT_CALL
nvidia_init(pmdaInterface *dp)
{
    int sts;

    if (isDSO) {
    	initializeHelpPath();
    	pmdaDSO(dp, PMDA_INTERFACE_2, "nvidia DSO", mypath);
    }

    if (dp->status != 0)
	return;

    if ((sts = localNvmlInit()) == NVML_SUCCESS) {
	setup_gcard_indom();
	nvmlDSO_loaded = 1;
    }
    else {
	/*
	 * This is OK, just continue on until it *is* installed;
	 * until that time, simply report "no values available".
	 */
	__pmNotifyErr(LOG_INFO, "NVIDIA NVML library currently unavailable");
    }

    // Set fetch callback function.
    dp->version.any.fetch = nvidia_fetch;
    pmdaSetFetchCallBack(dp, nvidia_fetchCallBack);

    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), 
	     metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
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

int
main(int argc, char **argv)
{
    pmdaInterface	desc;

    isDSO = 0;
    __pmSetProgname(argv[0]);

    initializeHelpPath();
    pmdaDaemon(&desc, PMDA_INTERFACE_2, pmProgname, NVML,
		"nvidia.log", mypath);

    pmdaGetOptions(argc, argv, &opts, &desc);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    pmdaOpenLog(&desc);
    nvidia_init(&desc);
    pmdaConnect(&desc);
    pmdaMain(&desc);

    exit(0);
}
