/*
 * Copyright (c) 2014,2019 Red Hat.
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
#include "pmda.h"
#include "domain.h"
#include "localnvml.h"

/* InDom table (set of graphics cards, set of processes+devices) */
enum { GCARD_INDOM = 0, GPROC_INDOM };
pmdaIndom indomtab[] = {
    { GCARD_INDOM, 0, NULL },
    { GPROC_INDOM, 0, NULL },
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
    NVIDIA_PROC_SAMPLES,
    NVIDIA_PROC_MEMUSED,
    NVIDIA_PROC_MEMACCUM,
    NVIDIA_PROC_GPUACTIVE,
    NVIDIA_PROC_MEMACTIVE,
    NVIDIA_PROC_TIME,

    NVIDIA_METRIC_COUNT
};

/* Flags indicating modes of the nvidia library */
enum {
    COMPUTE	= 0x1,
    ACCOUNT	= 0x2,
    PERSIST	= 0x4,
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
    { NULL, { PMDA_PMID(1, NVIDIA_PROC_SAMPLES), PM_TYPE_U64, GPROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
    { NULL, { PMDA_PMID(1, NVIDIA_PROC_MEMUSED), PM_TYPE_U64, GPROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
    { NULL, { PMDA_PMID(1, NVIDIA_PROC_MEMACCUM), PM_TYPE_U64, GPROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
    { NULL, { PMDA_PMID(1, NVIDIA_PROC_GPUACTIVE), PM_TYPE_U32, GPROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(1, NVIDIA_PROC_MEMACTIVE), PM_TYPE_U32, GPROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(1, NVIDIA_PROC_TIME), PM_TYPE_U64, GPROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0) } },
};

/* GPROC_INDOM struct, stats that are per process, per card */
typedef struct {
    unsigned int	pid;
    unsigned int	cardid;
    unsigned long long	samples;
    unsigned long long	memused;
    unsigned long long	memaccum;
    nvmlAccountingStats_t acct;
} nvproc_t;

/* GCARD_INDOM struct, stats that are per card */
typedef struct {
    int			cardid;
    int			flags;
    int			failed[NVIDIA_METRIC_COUNT];
    char		*name;
    char		*busid;
    int			temp;
    int			fanspeed;
    int			perfstate;
    int			processes;
    nvproc_t		*nvproc;
    nvmlUtilization_t	active;
    nvmlMemory_t	memory;
} nvinfo_t;

/* overall struct, holds instance values, indom and instance struct arrays */
typedef struct {
    int			numcards;
    int			maxcards;
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
    nvmlDevice_t	device;
    unsigned int	device_count = 0, count;
    pmdaIndom		*idp = &indomtab[GCARD_INDOM];
    char		gpuname[32], *name;
    int			i, sts;

    /* Initialize instance domain and instances. */
    if ((sts = localNvmlDeviceGetCount(&device_count)) != NVML_SUCCESS) {
	pmNotifyErr(LOG_ERR, "nvmlDeviceGetCount: %s",
			localNvmlErrStr(sts));
	return sts;
    }

    pcp_nvinfo.nvindom = idp;
    pcp_nvinfo.nvindom->it_numinst = 0;
    pcp_nvinfo.nvindom->it_set = (pmdaInstid *)calloc(device_count, sizeof(pmdaInstid));
    if (!pcp_nvinfo.nvindom->it_set) {
	pmNoMem("gcard indom", device_count * sizeof(pmdaInstid), PM_RECOV_ERR);
	return -ENOMEM;
    }

    if ((pcp_nvinfo.nvinfo = (nvinfo_t *)calloc(device_count, sizeof(nvinfo_t))) == NULL) {
	pmNoMem("gcard values", device_count * sizeof(nvinfo_t), PM_RECOV_ERR);
	free(pcp_nvinfo.nvindom->it_set);
	return -ENOMEM;
    }

    for (i = 0; i < device_count; i++) {
	pcp_nvinfo.nvindom->it_set[i].i_inst = i;
	pmsprintf(gpuname, sizeof(gpuname), "gpu%d", i);
	if ((name = strdup(gpuname)) == NULL) {
	    pmNoMem("gcard instname", strlen(gpuname), PM_RECOV_ERR);
	    while (--i)
		free(pcp_nvinfo.nvindom->it_set[i].i_name);
	    free(pcp_nvinfo.nvindom->it_set);
	    free(pcp_nvinfo.nvinfo);
	    return -ENOMEM;
	}
	pcp_nvinfo.nvindom->it_set[i].i_name = name;
    }
    for (i = 0; i < device_count; i++) {
	if ((sts = localNvmlDeviceGetHandleByIndex(i, &device))) {
	    pmNotifyErr(LOG_ERR, "nvmlDeviceGetHandleByIndex: %s",
			localNvmlErrStr(sts));
	    continue;
	}
	count = 0;
	sts = localNvmlDeviceGetComputeRunningProcesses(device, &count, NULL);
	if (sts == NVML_SUCCESS)
	    pcp_nvinfo.nvinfo[i].flags |= COMPUTE;
	sts = localNvmlDeviceSetAccountingMode(device, NVML_FEATURE_ENABLED);
	if (sts == NVML_SUCCESS)
	    pcp_nvinfo.nvinfo[i].flags |= ACCOUNT;
	sts = localNvmlDeviceSetPersistenceMode(device, NVML_FEATURE_ENABLED);
	if (sts == NVML_SUCCESS)
	    pcp_nvinfo.nvinfo[i].flags |= PERSIST;
    }

    pcp_nvinfo.numcards = 0;
    pcp_nvinfo.maxcards = device_count;
    pcp_nvinfo.nvindom->it_numinst = device_count;
    return 0;
}

static void
refresh_proc(nvmlDevice_t device, pmInDom indom, const char *name,
		unsigned int cardid, nvinfo_t *nvinfo)
{
    char		pname[NVML_DEVICE_NAME_BUFFER_SIZE+64];	/* + for pid::cardid:: */
    int			i, sts, inst;
    nvproc_t		*nvproc;
    unsigned int	count = 0;
    nvmlAccountingStats_t stats;
    static int		ninfos;		/* local high-water mark */
    static nvmlProcessInfo_t *infos, *tmp;

    /* extract size of process list for this device */
    localNvmlDeviceGetComputeRunningProcesses(device, &count, NULL);
    if (count > ninfos) {
	if ((tmp = realloc(infos, count * sizeof(*infos))) == NULL)
	    return;	/* out-of-memory */
	infos = tmp;
	ninfos = count;
    }
    count = ninfos;

    /* extract actual process list for this device now */
    if ((sts = localNvmlDeviceGetComputeRunningProcesses(device, &count, infos)))
	return;
    for (i = 0; i < count; i++) {
	/* extract the per-process stats now if available */
	memset(&stats, 0, sizeof(stats));
	if (nvinfo->flags & ACCOUNT)
	    localNvmlDeviceGetAccountingStats(device, infos[i].pid, &stats);

	/* build instance name (device + PID) */
	pmsprintf(pname, sizeof(pname), "%u::%u %s", cardid, infos[i].pid, name);

	/* lookup struct for this instance, create new one if none */
	if (pmdaCacheLookupName(indom, pname, &inst, (void **)&nvproc) < 0) {
	    if ((nvproc = (nvproc_t *)calloc(1, sizeof(*nvproc))) == NULL)
		continue;	/* out-of-memory */
	    nvproc->pid = infos[i].pid;
	    nvproc->cardid = cardid;
	}
	nvproc->memused = infos[i].usedGpuMemory;
	nvproc->memaccum += infos[i].usedGpuMemory;
	memcpy(&nvproc->acct, &stats, sizeof(stats));
	nvproc->samples++;

	pmdaCacheStore(indom, PMDA_CACHE_ADD, pname, nvproc);
    }
}

static int
refresh(pcp_nvinfo_t *pcp_nvinfo, int need_processes)
{
    unsigned int	device_count;
    nvmlDevice_t	device;
    pmInDom		indom = indomtab[GPROC_INDOM].it_indom;
    char		name[NVML_DEVICE_NAME_BUFFER_SIZE];
    nvmlPciInfo_t	pci;
    unsigned int	fanspeed;
    unsigned int	temperature;
    nvmlUtilization_t	utilization;
    nvmlMemory_t	memory;
    nvmlPstates_t	pstate;
    int			i, j, sts;

    if (!nvmlDSO_loaded) {
	if (localNvmlInit() == NVML_ERROR_LIBRARY_NOT_FOUND)
	    return 0;
	setup_gcard_indom();
	nvmlDSO_loaded = 1;
    }

    /* mark full cache inactive, later iterate over active processes */
    if (need_processes)
	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    if ((sts = localNvmlDeviceGetCount(&device_count)) != 0) {
	pmNotifyErr(LOG_ERR, "nvmlDeviceGetCount: %s",
			localNvmlErrStr(sts));
	return sts;
    }
    pcp_nvinfo->numcards = device_count;

    for (i = 0; i < device_count && i < pcp_nvinfo->maxcards; i++) {
	pcp_nvinfo->nvinfo[i].cardid = i;
	if ((sts = localNvmlDeviceGetHandleByIndex(i, &device))) {
	    pmNotifyErr(LOG_ERR, "nvmlDeviceGetHandleByIndex: %s",
			localNvmlErrStr(sts));
	    for (j = 0; j < NVIDIA_METRIC_COUNT; j++)
		pcp_nvinfo->nvinfo[i].failed[j] = 1;
	    continue;
	}
	for (j = 0; j < NVIDIA_METRIC_COUNT; j++)
	    pcp_nvinfo->nvinfo[i].failed[j] = 0;
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

	if (pcp_nvinfo->nvinfo[i].name == NULL &&
	    pcp_nvinfo->nvinfo[i].failed[NVIDIA_CARDNAME] == 0)
	    pcp_nvinfo->nvinfo[i].name = strdup(name);
	if (pcp_nvinfo->nvinfo[i].busid == NULL &&
	    pcp_nvinfo->nvinfo[i].failed[NVIDIA_BUSID] == 0)
	    pcp_nvinfo->nvinfo[i].busid = strdup(pci.busId);
	pcp_nvinfo->nvinfo[i].temp = temperature;
	pcp_nvinfo->nvinfo[i].fanspeed = fanspeed;
	pcp_nvinfo->nvinfo[i].perfstate = pstate;
	pcp_nvinfo->nvinfo[i].active = utilization;	/* struct copy */
	pcp_nvinfo->nvinfo[i].memory = memory;		/* struct copy */

	if (need_processes && !pcp_nvinfo->nvinfo[i].failed[NVIDIA_CARDNAME])
	    refresh_proc(device, indom, name, i, &pcp_nvinfo->nvinfo[i]);
    }

    /* walk and cull any entries that remain inactive at this point */
    if (need_processes)
	pmdaCacheOp(indom, PMDA_CACHE_SAVE);
    pmdaCachePurge(indom, 120);

    return 0;
}

static int
nvidia_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    if (pmInDom_serial(indom) == GPROC_INDOM)
	refresh(&pcp_nvinfo, 1);
    return pmdaInstance(indom, inst, name, result, pmda);
}

/*
 * Wrapper for pmdaFetch which refresh the set of values once per fetch
 * PDU.  The fetchCallback is then called once per-metric/instance pair
 * to perform the actual filling of the pmResult (via each pmAtomValue).
 */
static int
nvidia_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int		i, need_processes = 0;

    for (i = 0; i < numpmid; i++) {
	if (pmID_cluster(pmidlist[i]) == 1)
		need_processes = 1;
    }
    refresh(&pcp_nvinfo, need_processes);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
nvidia_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    unsigned int	cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int	item = pmID_item(mdesc->m_desc.pmid);
    nvproc_t		*nvproc;

    if (cluster != 0 && cluster != 1)
	return PM_ERR_PMID;
    if (item != 0 && cluster == 0 && inst > indomtab[GCARD_INDOM].it_numinst)
	return PM_ERR_INST;

    if (cluster == 0) {
	switch (item) {
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
    } else if (cluster == 1) {
	if (pmdaCacheLookup(mdesc->m_desc.indom, inst, NULL, (void **)&nvproc) < 0)
	    return PM_ERR_INST;
	switch (item) {
	case NVIDIA_PROC_SAMPLES:
	    atom->ull = nvproc->samples;
	    break;
	case NVIDIA_PROC_MEMUSED:
	    atom->ull = nvproc->memused;
	    break;
	case NVIDIA_PROC_MEMACCUM:
	    atom->ull = nvproc->memaccum;
	    break;
	case NVIDIA_PROC_GPUACTIVE:
	    atom->ull = nvproc->acct.gpuUtilization;
	    break;
	case NVIDIA_PROC_MEMACTIVE:
	    atom->ull = nvproc->acct.memoryUtilization;
	    break;
	case NVIDIA_PROC_TIME:
	    atom->ull = nvproc->acct.time;
	    break;
	default:
	    return PM_ERR_PMID;
	}
    }

    return 1;
}

static int
nvidia_labelCallBack(pmInDom indom, unsigned int inst, pmLabelSet **lp)
{
    nvproc_t	*nvproc;
    int		sts;

    if (indom == PM_INDOM_NULL)
	return 0;

    switch (pmInDom_serial(indom)) {
    case GCARD_INDOM:
	return pmdaAddLabels(lp, "{\"gpu\":%u}",
				pcp_nvinfo.nvinfo[inst].cardid);
    case GPROC_INDOM:
	sts = pmdaCacheLookup(indom, inst, NULL, (void **)&nvproc);
        if (sts < 0 || sts == PMDA_CACHE_INACTIVE)
            return 0;
        return pmdaAddLabels(lp, "{\"gpu\":%u,pid\":%u}",
				nvproc->cardid, nvproc->pid);
    default:
	break;
    }
    return 0;
}

static int
nvidia_labelInDom(pmInDom indom, pmLabelSet **lp)
{
    switch (pmInDom_serial(indom)) {
    case GCARD_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":\"gpu\"}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per gpu\"}");
	return 1;
    case GPROC_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":\"gpu\"}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per processes per gpu\"}");
	return 1;
    default:
	break;
    }
    return 0;
}

static int
nvidia_label(int ident, int type, pmLabelSet **lpp, pmdaExt *pmda)
{
    int		sts;

    switch (type) {
    case PM_LABEL_INDOM:
	if ((sts = nvidia_labelInDom((pmInDom)ident, lpp)) < 0)
	    return sts;
	break;
    default:
	break;
    }
    return pmdaLabel(ident, type, lpp, pmda);
}

/**
 * Initializes the path to the help file for this PMDA.
 */
static void
initializeHelpPath()
{
    int sep = pmPathSeparator();
    pmsprintf(mypath, sizeof(mypath), "%s%c" "nvidia" "%c" "help",
            pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
}

void 
__PMDA_INIT_CALL
nvidia_init(pmdaInterface *dp)
{
    int sts;

    if (isDSO) {
    	initializeHelpPath();
    	pmdaDSO(dp, PMDA_INTERFACE_7, "nvidia DSO", mypath);
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
	pmNotifyErr(LOG_INFO, "NVIDIA NVML library currently unavailable");
    }

    dp->version.seven.instance = nvidia_instance;
    dp->version.seven.fetch = nvidia_fetch;
    dp->version.seven.label = nvidia_label;
    pmdaSetFetchCallBack(dp, nvidia_fetchCallBack);
    pmdaSetLabelCallBack(dp, nvidia_labelCallBack);

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
    pmSetProgname(argv[0]);

    initializeHelpPath();
    pmdaDaemon(&desc, PMDA_INTERFACE_7, pmGetProgname(), NVML,
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
