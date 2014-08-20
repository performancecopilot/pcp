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

/* InDom table (just one row - corresponding to the set of graphics cards) */
enum { GCARD_INDOM = 0 };
pmdaIndom indomtab[] = {
    { GCARD_INDOM, 0, NULL },
};

/* List of metric item numbers - increasing from zero, no holes */
enum {
    NVIDIA_NUMCARDS = 0,
    NVIDIA_CARDID,
    NVIDIA_CARDNAME,
    NVIDIA_BUSID,
    NVIDIA_TEMP,
    NVIDIA_FANSPEED,
    NVIDIA_PERFSTATE,
    NVIDIA_GPUACTIVE,
    NVIDIA_MEMACTIVE,
    NVIDIA_MEMUSED,
    NVIDIA_MEMTOTAL,
    NVIDIA_MEMFREE,

    NVIDIA_METRIC_COUNT
};

/* Table of metrics exported by this PMDA */
static pmdaMetric metrictab[] = {
    { NULL, { PMDA_PMID(0, NVIDIA_NUMCARDS), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_CARDID), PM_TYPE_U32, GCARD_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_CARDNAME), PM_TYPE_STRING, GCARD_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_BUSID), PM_TYPE_STRING, GCARD_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_TEMP), PM_TYPE_U32, GCARD_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_FANSPEED), PM_TYPE_U32, GCARD_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_PERFSTATE), PM_TYPE_U32, GCARD_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_GPUACTIVE), PM_TYPE_U32, GCARD_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_MEMACTIVE), PM_TYPE_U32, GCARD_INDOM,
        PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_MEMUSED), PM_TYPE_U64, GCARD_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_MEMTOTAL), PM_TYPE_U64, GCARD_INDOM,
	PM_SEM_DISCRETE, PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_MEMFREE), PM_TYPE_U64, GCARD_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
};

/* GCARD_INDOM struct, stats that are per card */
typedef struct {
    int			cardid;
    int			failed[NVIDIA_METRIC_COUNT];
    char		*name;
    char		*busid;
    int			temp;
    int			fanspeed;
    int			perfstate;
    nvmlUtilization_t	active;
    nvmlMemory_t	memory;
} nvinfo_t;

/* overall struct, holds instance values, indom and instance struct arrays */
typedef struct {
    int			numcards;
    int			 maxcards;
    nvinfo_t		*nvinfo;
    pmdaIndom		*nvindom;
} pcp_nvinfo_t;

static pcp_nvinfo_t	pcp_nvinfo;
static char		mypath[MAXPATHLEN];
static int		isDSO = 1;
static int		nvmlDSO_loaded;

static int
setup_gcard_indom(void)
{
    unsigned int	device_count = 0;
    pmdaIndom		*idp = &indomtab[GCARD_INDOM];
    char		gpuname[32], *name;
    size_t		size;
    int			i, sts;

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
    unsigned int	device_count;
    nvmlDevice_t	device;
    char		name[NVML_DEVICE_NAME_BUFFER_SIZE];
    nvmlPciInfo_t	pci;
    unsigned int	fanspeed;
    unsigned int	temperature;
    nvmlUtilization_t	utilization;
    nvmlMemory_t	memory;
    nvmlPstates_t	pstate;
    int			i, sts;

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
	if ((sts = localNvmlDeviceGetHandleByIndex(i, &device))) {
	    __pmNotifyErr(LOG_ERR, "nvmlDeviceGetHandleByIndex: %s",
			localNvmlErrStr(sts));
	    memset(pcp_nvinfo->nvinfo[i].failed, 1, NVIDIA_METRIC_COUNT);
	    continue;
	}
	memset(pcp_nvinfo->nvinfo[i].failed, 0, NVIDIA_METRIC_COUNT);
	if ((sts = localNvmlDeviceGetName(device, name, sizeof(name))))
	    pcp_nvinfo->nvinfo[i].failed[NVIDIA_CARDNAME] = 1;
        if ((sts = localNvmlDeviceGetPciInfo(device, &pci)))
	    pcp_nvinfo->nvinfo[i].failed[NVIDIA_BUSID] = 1;
        if ((sts = localNvmlDeviceGetFanSpeed(device, &fanspeed)))
	    pcp_nvinfo->nvinfo[i].failed[NVIDIA_FANSPEED] = 1;
        if ((sts = localNvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature)))
	    pcp_nvinfo->nvinfo[i].failed[NVIDIA_TEMP] = 1;
        if ((sts = localNvmlDeviceGetUtilizationRates(device, &utilization))) {
	    pcp_nvinfo->nvinfo[i].failed[NVIDIA_GPUACTIVE] = 1;
	    pcp_nvinfo->nvinfo[i].failed[NVIDIA_MEMACTIVE] = 1;
	}
        if ((sts = localNvmlDeviceGetMemoryInfo(device, &memory))) {
	    pcp_nvinfo->nvinfo[i].failed[NVIDIA_MEMUSED] = 1;
	    pcp_nvinfo->nvinfo[i].failed[NVIDIA_MEMTOTAL] = 1;
	    pcp_nvinfo->nvinfo[i].failed[NVIDIA_MEMFREE] = 1;
	}
	if ((sts = localNvmlDeviceGetPerformanceState(device, &pstate)))
	    pcp_nvinfo->nvinfo[i].failed[NVIDIA_PERFSTATE] = 1;

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
        case NVIDIA_NUMCARDS:
            atom->ul = pcp_nvinfo.numcards;
            break;
        case NVIDIA_CARDID:
            atom->ul = pcp_nvinfo.nvinfo[inst].cardid;
            break;
        case NVIDIA_CARDNAME:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_CARDNAME])
		return PM_ERR_VALUE;
            atom->cp = pcp_nvinfo.nvinfo[inst].name;
            break;
        case NVIDIA_BUSID:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_BUSID])
		return PM_ERR_VALUE;
            atom->cp = pcp_nvinfo.nvinfo[inst].busid;
            break;
        case NVIDIA_TEMP:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_TEMP])
		return PM_ERR_VALUE;
            atom->ul = pcp_nvinfo.nvinfo[inst].temp;
            break;
        case NVIDIA_FANSPEED:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_FANSPEED])
		return PM_ERR_VALUE;
            atom->ul = pcp_nvinfo.nvinfo[inst].fanspeed;
            break;
        case NVIDIA_PERFSTATE:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_PERFSTATE])
		return PM_ERR_VALUE;
            atom->ul = pcp_nvinfo.nvinfo[inst].perfstate;
            break;
        case NVIDIA_GPUACTIVE:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_GPUACTIVE])
		return PM_ERR_VALUE;
            atom->ul = pcp_nvinfo.nvinfo[inst].active.gpu;
            break;
        case NVIDIA_MEMACTIVE:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_MEMACTIVE])
		return PM_ERR_VALUE;
            atom->ul = pcp_nvinfo.nvinfo[inst].active.memory;
            break;
        case NVIDIA_MEMUSED:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_MEMUSED])
		return PM_ERR_VALUE;
            atom->ull = pcp_nvinfo.nvinfo[inst].memory.used;
            break;
        case NVIDIA_MEMTOTAL:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_MEMTOTAL])
		return PM_ERR_VALUE;
            atom->ull = pcp_nvinfo.nvinfo[inst].memory.total;
            break;
        case NVIDIA_MEMFREE:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_MEMFREE])
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
    pmdaConnect(&desc);
    nvidia_init(&desc);
    pmdaMain(&desc);

    exit(0);
}
