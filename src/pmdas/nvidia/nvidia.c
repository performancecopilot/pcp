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
    char		*name;
    char		*busid;
    int			temp;
    int			fanspeed;
    int			perfstate;
    nvmlUtilization_t	active;
    nvmlMemory_t	memory;
    unsigned long long	memused;
    unsigned long long	memtotal;

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

static int
refresh(pcp_nvinfo_t *pcp_nvinfo)
{
    nvmlReturn_t result;
    unsigned int device_count, i;
    nvmlDevice_t device;
    char name[NVML_DEVICE_NAME_BUFFER_SIZE];
    nvmlPciInfo_t pci;
    unsigned int fanspeed;
    unsigned int temperature;
    nvmlUtilization_t utilization;
    nvmlMemory_t memory;
    nvmlPstates_t pstate;

    result = localNvmlInit();
    result = localNvmlDeviceGetCount(&device_count);

    pcp_nvinfo->numcards = device_count;

    for (i = 0; i < device_count && i < pcp_nvinfo->maxcards; i++) {

        result = localNvmlDeviceGetHandleByIndex(i, &device);

        result = localNvmlDeviceGetName(device, name, sizeof(name));
        
        result = localNvmlDeviceGetPciInfo(device, &pci);

        result = localNvmlDeviceGetFanSpeed(device, &fanspeed);

        result = localNvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature);

        result = localNvmlDeviceGetUtilizationRates(device, &utilization);

        result = localNvmlDeviceGetMemoryInfo(device, &memory);

        result = localNvmlDeviceGetPerformanceState(device, &pstate);

        pcp_nvinfo->nvinfo[i].cardid = i;
        if (pcp_nvinfo->nvinfo[i].name == NULL)
            pcp_nvinfo->nvinfo[i].name = strdup(name);
        if (pcp_nvinfo->nvinfo[i].busid == NULL)
            pcp_nvinfo->nvinfo[i].busid = strdup(pci.busId);
        pcp_nvinfo->nvinfo[i].temp = temperature;
        pcp_nvinfo->nvinfo[i].fanspeed = fanspeed;
        pcp_nvinfo->nvinfo[i].perfstate = pstate;
        pcp_nvinfo->nvinfo[i].active = utilization;	/* struct copy */
        pcp_nvinfo->nvinfo[i].memory = memory;		/* struct copy */
    }

    result = localNvmlShutdown();
    return 0;
}

static int
nvidia_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int	*idp = (__pmID_int *)&(mdesc->m_desc.pmid);

    refresh(&pcp_nvinfo);

    if (idp->cluster != 0)
	return PM_ERR_PMID;
    if (idp->item != 0 && inst > indomtab[GCARD_INDOM].it_numinst)
	return PM_ERR_INST;

    switch (idp->item){
        case 0:
            atom->ul = pcp_nvinfo.numcards;
            break;
        case 1:
            atom->ul = pcp_nvinfo.nvinfo[inst].cardid;
            break;
        case 2:
            atom->cp = pcp_nvinfo.nvinfo[inst].name;
            break;
        case 3:
            atom->cp = pcp_nvinfo.nvinfo[inst].busid;
            break;
        case 4:
            atom->ul = pcp_nvinfo.nvinfo[inst].temp;
            break;
        case 5:
            atom->ul = pcp_nvinfo.nvinfo[inst].fanspeed;
            break;
        case 6:
            atom->ul = pcp_nvinfo.nvinfo[inst].perfstate;
            break;
        case 7:
            atom->ul = pcp_nvinfo.nvinfo[inst].active.gpu;
            break;
        case 8:
            atom->ul = pcp_nvinfo.nvinfo[inst].active.memory;
            break;
        case 9:
            atom->ull = pcp_nvinfo.nvinfo[inst].memory.used;
            break;
        case 10:
            atom->ull = pcp_nvinfo.nvinfo[inst].memory.total;
            break;
        case 11:
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
nvidia_init(pmdaInterface *dp)
{
    unsigned int i, device_count;
    char gpuname[32];
    size_t nvsize;

    if (isDSO) {
    	initializeHelpPath();
    	pmdaDSO(dp, PMDA_INTERFACE_2, "nvidia DSO", mypath);
    }

    if (dp->status != 0)
	return;

    nvmlReturn_t result;
    result = localNvmlInit();
    if (NVML_SUCCESS != result) {
	dp->status = -EIO;
	return;
    }

    // Initialize instance domain and instances.
    result = localNvmlDeviceGetCount(&device_count);

    pmdaIndom *idp = &indomtab[GCARD_INDOM];
    pcp_nvinfo.nvindom = idp;
    pcp_nvinfo.nvindom->it_numinst = device_count;
    pcp_nvinfo.nvindom->it_set = (pmdaInstid *)malloc(device_count * sizeof(pmdaInstid));

    for (i = 0; i < device_count; i++) {
	pcp_nvinfo.nvindom->it_set[i].i_inst = i;
	snprintf(gpuname, sizeof(gpuname), "gpu%d", i);
	pcp_nvinfo.nvindom->it_set[i].i_name = strdup(gpuname);
    }

    nvsize = pcp_nvinfo.nvindom->it_numinst * sizeof(nvinfo_t);
    pcp_nvinfo.nvinfo = (nvinfo_t*)malloc(nvsize);
    memset(pcp_nvinfo.nvinfo, 0, nvsize);

    pcp_nvinfo.numcards = 0;
    pcp_nvinfo.maxcards = device_count;

    // Set fetch callback function.
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
