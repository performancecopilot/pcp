/*
 * Copyright (c) 2014,2019,2021 Red Hat.
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
#include "libpcp.h"
#include "localnvml.h"

/* InDom table (set of graphics cards, set of processes+devices) */
enum { GCARD_INDOM = 0, GPROC_INDOM, PROC_INDOM };
pmdaIndom indomtab[] = {
    { GCARD_INDOM, 0, NULL },
    { GPROC_INDOM, 0, NULL },
    { PROC_INDOM, 0, NULL },
};

/* List of metric item numbers - increasing from zero, no holes */
enum {
    NVIDIA_NUMCARDS = 0,
    NVIDIA_CARDID,
    NVIDIA_CARDNAME,
    NVIDIA_BUSID,
    NVIDIA_TEMPERATURE,
    NVIDIA_FANSPEED,
    NVIDIA_PERFSTATE,
    NVIDIA_GPUACTIVE,
    NVIDIA_MEMACTIVE,
    NVIDIA_MEMUSED,
    NVIDIA_MEMTOTAL,
    NVIDIA_MEMFREE,
    NVIDIA_GPROC_SAMPLES,
    NVIDIA_GPROC_MEMUSED,
    NVIDIA_GPROC_MEMACCUM,
    NVIDIA_GPROC_GPUACTIVE,
    NVIDIA_GPROC_MEMACTIVE,
    NVIDIA_GPROC_TIME,
    NVIDIA_GPROC_RUNNING,
    NVIDIA_CARDUUID,
    NVIDIA_ENERGY,
    NVIDIA_POWER,
    NVIDIA_NPROCS,
    NVIDIA_SAMPLES,
    NVIDIA_GPUACTIVE_ACCUM,
    NVIDIA_MEMACTIVE_ACCUM,
    NVIDIA_MEMUSED_ACCUM,

    NVIDIA_METRIC_COUNT
};

/* List of metrics item numbers for the per-process clusters */
enum {
    NVIDIA_PROC_SAMPLES,
    NVIDIA_PROC_MEMUSED,
    NVIDIA_PROC_MEMACCUM,
    NVIDIA_PROC_GPUACTIVE,
    NVIDIA_PROC_MEMACTIVE,
    NVIDIA_PROC_TIME,
    NVIDIA_PROC_RUNNING,
    NVIDIA_PROC_GPULIST,
    NVIDIA_PROC_NGPUS,
};

/* Flags indicating modes of the Nvidia library and other internal PMDA state */
enum {
    HASCOMPUTE	= 1<<0,
    COMPUTE	= 1<<1,
    HASGRAPHICS	= 1<<2,
    GRAPHICS	= 1<<3,
    HASACCOUNT	= 1<<4,
    ACCOUNT	= 1<<5,
};

/* Table of metrics exported by this PMDA */
static pmdaMetric metrictab[] = {
    { NULL, { PMDA_PMID(0, NVIDIA_NUMCARDS), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_CARDID), PM_TYPE_U32, GCARD_INDOM,
	PM_SEM_DISCRETE, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_CARDNAME), PM_TYPE_STRING, GCARD_INDOM,
	PM_SEM_DISCRETE, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_CARDUUID), PM_TYPE_STRING, GCARD_INDOM,
	PM_SEM_DISCRETE, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_BUSID), PM_TYPE_STRING, GCARD_INDOM,
	PM_SEM_DISCRETE, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_TEMPERATURE), PM_TYPE_U32, GCARD_INDOM,
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
    { NULL, { PMDA_PMID(1, NVIDIA_GPROC_SAMPLES), PM_TYPE_U64, GPROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
    { NULL, { PMDA_PMID(1, NVIDIA_GPROC_MEMUSED), PM_TYPE_U64, GPROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
    { NULL, { PMDA_PMID(1, NVIDIA_GPROC_MEMACCUM), PM_TYPE_U64, GPROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
    { NULL, { PMDA_PMID(1, NVIDIA_GPROC_GPUACTIVE), PM_TYPE_U32, GPROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(1, NVIDIA_GPROC_MEMACTIVE), PM_TYPE_U32, GPROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(1, NVIDIA_GPROC_TIME), PM_TYPE_U64, GPROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0) } },
    { NULL, { PMDA_PMID(1, NVIDIA_GPROC_RUNNING), PM_TYPE_U32, GPROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_ENERGY), PM_TYPE_U64, GCARD_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_POWER), PM_TYPE_U32, GCARD_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_NPROCS), PM_TYPE_U32, GCARD_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_SAMPLES), PM_TYPE_U64, GCARD_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_GPUACTIVE_ACCUM), PM_TYPE_U64, GCARD_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_MEMACTIVE_ACCUM), PM_TYPE_U64, GCARD_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(0, NVIDIA_MEMUSED_ACCUM), PM_TYPE_U64, GCARD_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

    /* nvidia.proc.all */
    { NULL, { PMDA_PMID(2, NVIDIA_PROC_SAMPLES), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
    { NULL, { PMDA_PMID(2, NVIDIA_PROC_MEMUSED), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
    { NULL, { PMDA_PMID(2, NVIDIA_PROC_MEMACCUM), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
    { NULL, { PMDA_PMID(2, NVIDIA_PROC_GPUACTIVE), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(2, NVIDIA_PROC_MEMACTIVE), PM_TYPE_U32, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(2, NVIDIA_PROC_TIME), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0) } },
    { NULL, { PMDA_PMID(2, NVIDIA_PROC_RUNNING), PM_TYPE_U32, PROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(2, NVIDIA_PROC_GPULIST), PM_TYPE_U32, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(2, NVIDIA_PROC_NGPUS), PM_TYPE_U32, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },

    /* nvidia.proc.compute */
    { NULL, { PMDA_PMID(3, NVIDIA_PROC_SAMPLES), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
    { NULL, { PMDA_PMID(3, NVIDIA_PROC_MEMUSED), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
    { NULL, { PMDA_PMID(3, NVIDIA_PROC_MEMACCUM), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
    { NULL, { PMDA_PMID(3, NVIDIA_PROC_GPUACTIVE), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(3, NVIDIA_PROC_MEMACTIVE), PM_TYPE_U32, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(3, NVIDIA_PROC_TIME), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0) } },
    { NULL, { PMDA_PMID(3, NVIDIA_PROC_RUNNING), PM_TYPE_U32, PROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(3, NVIDIA_PROC_GPULIST), PM_TYPE_U32, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(3, NVIDIA_PROC_NGPUS), PM_TYPE_U32, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },

    /* nvidia.proc.graphics */
    { NULL, { PMDA_PMID(4, NVIDIA_PROC_SAMPLES), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
    { NULL, { PMDA_PMID(4, NVIDIA_PROC_MEMUSED), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
    { NULL, { PMDA_PMID(4, NVIDIA_PROC_MEMACCUM), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
    { NULL, { PMDA_PMID(4, NVIDIA_PROC_GPUACTIVE), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(4, NVIDIA_PROC_MEMACTIVE), PM_TYPE_U32, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(4, NVIDIA_PROC_TIME), PM_TYPE_U64, PROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0) } },
    { NULL, { PMDA_PMID(4, NVIDIA_PROC_RUNNING), PM_TYPE_U32, PROC_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(4, NVIDIA_PROC_GPULIST), PM_TYPE_U32, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    { NULL, { PMDA_PMID(4, NVIDIA_PROC_NGPUS), PM_TYPE_U32, PROC_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
};

enum {
    PROCESS_COMPUTE = 0,
    PROCESS_GRAPHICS = 1,
    PROCESS_MODES
};

/* PROC_INDOM struct, stats that are per process, all cards */
typedef struct {
    pid_t		pid;
    unsigned int	flags;	/* COMPUTE|GRAPHICS|ACCOUNT */
    char		*name;
    struct {
	unsigned long long	memused;
	unsigned long long	memaccum;
	unsigned int		gpuutil;
	unsigned int		memutil;
	unsigned long long	time;
	unsigned long long	samples;
	unsigned int		gpulist;
	unsigned int		running;
	unsigned int		ngpus;
    } acct[PROCESS_MODES];
} process_t;

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
    int			failed[NVIDIA_METRIC_COUNT];
    char		*name;
    char		*uuid;
    char		*busid;
    unsigned int	flags;
    unsigned int	nprocs;
    unsigned int	temperature;
    unsigned int	fanspeed;
    unsigned int	perfstate;
    unsigned int	power;
    unsigned long long	energy;
    unsigned long long	samples;
    unsigned long long	memaccum;
    unsigned long long	gpuutilaccum;
    unsigned long long	memutilaccum;
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
static __pmHashCtl	processes;
static char		mypath[MAXPATHLEN];
static int		isDSO = 1;
static int		nvmlDSO_loaded;
static int		nvmlDSO_status;
static int		autorefresh = -1;
static struct timeval	interval;

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
	if (sts == NVML_SUCCESS || sts == NVML_ERROR_INSUFFICIENT_SIZE)
	    pcp_nvinfo.nvinfo[i].flags |= HASCOMPUTE;
	count = 0;
	sts = localNvmlDeviceGetGraphicsRunningProcesses(device, &count, NULL);
	if (sts == NVML_SUCCESS || sts == NVML_ERROR_INSUFFICIENT_SIZE)
	    pcp_nvinfo.nvinfo[i].flags |= HASGRAPHICS;
	sts = localNvmlDeviceSetAccountingMode(device, NVML_FEATURE_ENABLED);
	if (sts == NVML_SUCCESS)
	    pcp_nvinfo.nvinfo[i].flags |= HASACCOUNT;
	localNvmlDeviceSetPersistenceMode(device, NVML_FEATURE_ENABLED);
    }

    pcp_nvinfo.numcards = 0;
    pcp_nvinfo.maxcards = device_count;
    pcp_nvinfo.nvindom->it_numinst = device_count;
    return 0;
}

static void
update_process(pid_t pid, int mode, unsigned int cardid,
		nvmlProcessInfo_t *info, nvmlAccountingStats_t *stats)
{
    __pmHashNode	*node;
    process_t		*process;
    char		name[32];

    if ((node = __pmHashSearch(pid, &processes)) == NULL) {
	if ((process = (process_t *)calloc(1, sizeof(process_t))) == NULL)
	    return;
	process->pid = pid;
	pmsprintf(name, sizeof(name), "%06d", pid);
	process->name = strdup(name);
	__pmHashAdd(pid, process, &processes);
    } else {
	process = (process_t *)node->data;
    }
    if (mode == PROCESS_COMPUTE)
	process->flags |= COMPUTE;
    if (mode == PROCESS_GRAPHICS)
	process->flags |= GRAPHICS;
    process->acct[mode].memused = info->usedGpuMemory;
    process->acct[mode].memaccum += info->usedGpuMemory;
    process->acct[mode].memutil = stats->memoryUtilization;
    process->acct[mode].gpuutil = stats->gpuUtilization;
    process->acct[mode].running = stats->isRunning;
    process->acct[mode].time = stats->time;
    if (cardid < 32)
	process->acct[mode].gpulist |= (1 << cardid);
    process->acct[mode].samples++;
    process->acct[mode].ngpus++;
}

static void
update_processes(nvmlDevice_t device, pmInDom gpuproc_indom,
		const char *name, unsigned int cardid,
		unsigned int count, nvmlProcessInfo_t *infos,
		nvinfo_t *nvinfo, int mode)
{
    char		pname[NVML_DEVICE_NAME_BUFFER_SIZE+64];	/* + for pid::cardid:: */
    int			i, inst;

    for (i = 0; i < count; i++) {
	nvmlAccountingStats_t	stats = {0};
	nvproc_t		*nvproc;

	/* extract the per-process stats now if available */
	if ((nvinfo->flags & HASACCOUNT))
	    localNvmlDeviceGetAccountingStats(device, infos[i].pid, &stats);

	/* handle the per-process instance domain first */
	update_process(infos[i].pid, mode, cardid, &infos[i], &stats);

	if (name == NULL)
	    continue;

	/* build instance name (device + PID) */
	pmsprintf(pname, sizeof(pname), "gpu%u::%u", cardid, infos[i].pid);

	/* lookup struct for this instance, create new one if none */
	if (pmdaCacheLookupName(gpuproc_indom, pname, &inst, (void **)&nvproc) < 0) {
	    if ((nvproc = (nvproc_t *)calloc(1, sizeof(*nvproc))) == NULL)
		continue;	/* out-of-memory */
	    nvproc->pid = infos[i].pid;
	    nvproc->cardid = cardid;
	}
	nvproc->memused = infos[i].usedGpuMemory;
	nvproc->memaccum += infos[i].usedGpuMemory;
	memcpy(&nvproc->acct, &stats, sizeof(stats));
	nvproc->samples++;

	pmdaCacheStore(gpuproc_indom, PMDA_CACHE_ADD, pname, nvproc);
    }
}

static int
refresh_proc(nvmlDevice_t device, pmInDom gpuproc_indom,
		const char *name, unsigned int cardid, nvinfo_t *nvinfo)
{
    static nvmlProcessInfo_t *infos, *tmp;
    static int		ninfos;		/* local high-water mark */
    unsigned int	count, total = 0;

    if ((nvinfo->flags & HASCOMPUTE)) {
	/* extract size of compute process list for this device */
	count = 0;
	localNvmlDeviceGetComputeRunningProcesses(device, &count, NULL);
	if (count > ninfos) {
	    if ((tmp = realloc(infos, count * sizeof(*infos))) == NULL)
		return total;	/* out-of-memory */
	    infos = tmp;
	    ninfos = count;
	}
	/* extract actual list of processes using compute on this device now */
	if (count > 0) {
	    count = ninfos;
	    localNvmlDeviceGetComputeRunningProcesses(device, &count, infos);
	    if (count > 0) {
		update_processes(device, gpuproc_indom, name, cardid,
				 count, infos, nvinfo, PROCESS_COMPUTE);
		total += count;
	    }
	}
    }

    if ((nvinfo->flags & HASGRAPHICS)) {
	/* extract size of graphics process list for this device */
	count = 0;
	localNvmlDeviceGetGraphicsRunningProcesses(device, &count, NULL);
	if (count > ninfos) {
	    if ((tmp = realloc(infos, count * sizeof(*infos))) == NULL)
		return total;	/* out-of-memory */
	    infos = tmp;
	    ninfos = count;
	}
	/* extract actual list of processes using graphics on this device now */
	if (count > 0) {
	    count = ninfos;
	    localNvmlDeviceGetGraphicsRunningProcesses(device, &count, infos);
	    if (count > 0) {
		update_processes(device, gpuproc_indom, name, cardid,
				 count, infos, nvinfo, PROCESS_GRAPHICS);
		total += count;
	    }
	}
    }

    return total;
}

static int
pid_compare(const void *a, const void *b)
{
    const pmdaInstid	*ap = (const pmdaInstid *)a;
    const pmdaInstid	*bp = (const pmdaInstid *)b;

    if (ap->i_inst < bp->i_inst)
	return -1;
    if (ap->i_inst > bp->i_inst)
	return 1;
    return 0;
}

static int
refresh(pcp_nvinfo_t *nvinfo, int need_processes)
{
    unsigned int	device_count;
    nvmlDevice_t	device;
    process_t		*proc;
    __pmHashNode	*node;
    pmInDom		gpuproc_indom = indomtab[GPROC_INDOM].it_indom;
    char		name[NVML_DEVICE_NAME_BUFFER_SIZE];
    char		uuid[NVML_DEVICE_UUID_BUFFER_SIZE];
    nvmlPciInfo_t	pci;
    unsigned int	nproc = 0;
    unsigned int	power = 0;
    unsigned long long	energy = 0;
    unsigned int	fanspeed = 0;
    unsigned int	temperature = 0;
    nvmlUtilization_t	utilization;
    nvmlMemory_t	memory = {0};
    nvmlPstates_t	pstate = 0;
    nvinfo_t		*info;
    int			i, j, sts;

    if (!nvmlDSO_loaded) {
	sts = nvmlDSO_status;
	nvmlDSO_status = localNvmlInit();
	if (nvmlDSO_status == NVML_ERROR_LIBRARY_NOT_FOUND) {
	    return 0;
	} else if (nvmlDSO_status != NVML_SUCCESS) {
	    if (nvmlDSO_status != sts)
		pmNotifyErr(LOG_ERR, "nvmlInit: %s", localNvmlErrStr(sts));
	    return 0;
	}
	setup_gcard_indom();
	nvmlDSO_loaded = 1;
    }

    /* mark caches inactive, later iterate over active processes */
    if (need_processes) {
	/* gpu+proc indom */
	pmdaCacheOp(gpuproc_indom, PMDA_CACHE_INACTIVE);
	/* proc indom */
	for (i = 0; i < processes.hsize; i++) {
	    for (node = processes.hash[i]; node != NULL; node = node->next) {
		proc = (process_t *)node->data;
		proc->acct[PROCESS_COMPUTE].ngpus = 0;
		proc->acct[PROCESS_GRAPHICS].ngpus = 0;
		proc->acct[PROCESS_COMPUTE].running = 0;
		proc->acct[PROCESS_GRAPHICS].running = 0;
		proc->acct[PROCESS_COMPUTE].gpulist = 0;
		proc->acct[PROCESS_GRAPHICS].gpulist = 0;
		proc->flags &= ~(COMPUTE|GRAPHICS|ACCOUNT);
	    }
	}
    }

    if ((sts = localNvmlDeviceGetCount(&device_count)) != 0) {
	pmNotifyErr(LOG_ERR, "nvmlDeviceGetCount: %s",
			localNvmlErrStr(sts));
	return sts;
    }
    nvinfo->numcards = device_count;

    for (i = 0; i < device_count && i < nvinfo->maxcards; i++) {
	info = &nvinfo->nvinfo[i];
	info->cardid = i;
	if ((sts = localNvmlDeviceGetHandleByIndex(i, &device))) {
	    pmNotifyErr(LOG_ERR, "nvmlDeviceGetHandleByIndex: %s",
			localNvmlErrStr(sts));
	    for (j = 0; j < NVIDIA_METRIC_COUNT; j++)
		info->failed[j] = 1;
	    continue;
	}
	for (j = 0; j < NVIDIA_METRIC_COUNT; j++)
	    info->failed[j] = 0;
	if ((sts = localNvmlDeviceGetName(device, name, sizeof(name))))
	    info->failed[NVIDIA_CARDNAME] = 1;
	if ((sts = localNvmlDeviceGetUUID(device, uuid, sizeof(uuid))))
	    info->failed[NVIDIA_CARDUUID] = 1;
	if ((sts = localNvmlDeviceGetPciInfo(device, &pci)))
	    info->failed[NVIDIA_BUSID] = 1;
	if ((sts = localNvmlDeviceGetFanSpeed(device, &fanspeed)))
	    info->failed[NVIDIA_FANSPEED] = 1;
	if ((sts = localNvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature)))
	    info->failed[NVIDIA_TEMPERATURE] = 1;
	if ((sts = localNvmlDeviceGetUtilizationRates(device, &utilization))) {
	    info->failed[NVIDIA_GPUACTIVE] = 1;
	    info->failed[NVIDIA_MEMACTIVE] = 1;
	}
	if ((sts = localNvmlDeviceGetMemoryInfo(device, &memory))) {
	    info->failed[NVIDIA_MEMUSED] = 1;
	    info->failed[NVIDIA_MEMTOTAL] = 1;
	    info->failed[NVIDIA_MEMFREE] = 1;
	}
	if ((sts = localNvmlDeviceGetPerformanceState(device, &pstate)))
	    info->failed[NVIDIA_PERFSTATE] = 1;
	if ((sts = localNvmlDeviceGetTotalEnergyConsumption(device, &energy)))
	    info->failed[NVIDIA_ENERGY] = 1;
	if ((sts = localNvmlDeviceGetPowerUsage(device, &power)))
	    info->failed[NVIDIA_POWER] = 1;

	if (info->name == NULL &&
	    info->failed[NVIDIA_CARDNAME] == 0)
	    info->name = strdup(name);
	if (info->uuid == NULL &&
	    info->failed[NVIDIA_CARDUUID] == 0)
	    info->uuid = strdup(uuid);
	if (info->busid == NULL &&
	    info->failed[NVIDIA_BUSID] == 0)
	    info->busid = strdup(pci.busId);
	info->temperature = temperature;
	info->fanspeed = fanspeed;
	info->perfstate = pstate;
	info->active = utilization;	/* struct copy */
	info->memutilaccum += utilization.memory;
	info->gpuutilaccum += utilization.gpu;
	info->memory = memory; 		/* struct copy */
	info->memaccum += memory.used;
	info->energy = energy;
	info->power = power;
	info->nprocs = 0;
	info->samples++;

	if (need_processes) {
	    info->nprocs = refresh_proc(device, gpuproc_indom, name, i, info);
	    nproc += info->nprocs;
	}
    }

    /* update indoms, cull old entries that remain inactive */
    if (need_processes) {
	pmdaIndom	*proc_indomp = &indomtab[PROC_INDOM];
	pmdaInstid	*it_set = NULL;
	size_t		bytes = nproc * sizeof(pmdaInstid);

	if (bytes > 0) {
	    it_set = (pmdaInstid *)realloc(proc_indomp->it_set, bytes);
	    if (it_set == NULL)
		free(proc_indomp->it_set);
	} else if (proc_indomp->it_set != NULL) {
	    free(proc_indomp->it_set);
	}

	if ((proc_indomp->it_set = it_set) != NULL) {
	    for (i = j = 0; i < processes.hsize && j < nproc; i++) {
		for (node = processes.hash[i]; node; node = node->next) {
		    proc = (process_t *)node->data;
		    proc_indomp->it_set[j].i_inst = node->key;
		    proc_indomp->it_set[j].i_name = proc->name;
		    if (++j >= nproc)
			break;
		}
	    }
	    qsort(proc_indomp->it_set, j, sizeof(pmdaInstid), pid_compare);
	    proc_indomp->it_numinst = j;
	} else {
	    proc_indomp->it_numinst = 0;
	}
    }
    pmdaCachePurge(gpuproc_indom, 120);

    return 0;
}

static int
nvidia_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    unsigned int	serial = pmInDom_serial(indom);

    if (serial == GPROC_INDOM || serial == PROC_INDOM)
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
    int		i, item, cluster, need_processes = 0;

    for (i = 0; i < numpmid; i++) {
	item = pmID_item(pmidlist[i]);
	cluster = pmID_cluster(pmidlist[i]);
	if ((cluster == 0 && item == NVIDIA_NPROCS) ||
	    (cluster == 1 || cluster == 2 || cluster == 3 || cluster == 4))
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
    __pmHashNode	*node;
    process_t		*proc;
    nvproc_t		*nvproc;
    int			mode;

    if (cluster > 4)
	return PM_ERR_PMID;
    if (item != 0 && cluster == 0 && inst > indomtab[GCARD_INDOM].it_numinst)
	return PM_ERR_INST;

    switch (cluster) {
    case 0:	/* nvidia general and per-card metrics */
	switch (item) {
	case NVIDIA_NUMCARDS:
	    atom->ul = pcp_nvinfo.numcards;
	    break;
	case NVIDIA_CARDID:
	    atom->ul = pcp_nvinfo.nvinfo[inst].cardid;
	    break;
	case NVIDIA_SAMPLES:
	    atom->ull = pcp_nvinfo.nvinfo[inst].samples;
	    break;
	case NVIDIA_CARDNAME:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_CARDNAME])
		return PM_ERR_VALUE;
	    atom->cp = pcp_nvinfo.nvinfo[inst].name;
	    break;
	case NVIDIA_CARDUUID:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_CARDUUID])
		return PM_ERR_VALUE;
	    atom->cp = pcp_nvinfo.nvinfo[inst].uuid;
	    break;
	case NVIDIA_BUSID:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_BUSID])
		return PM_ERR_VALUE;
	    atom->cp = pcp_nvinfo.nvinfo[inst].busid;
	    break;
	case NVIDIA_TEMPERATURE:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_TEMPERATURE])
		return PM_ERR_APPVERSION;
	    atom->ul = pcp_nvinfo.nvinfo[inst].temperature;
	    break;
	case NVIDIA_FANSPEED:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_FANSPEED])
		return PM_ERR_APPVERSION;
	    atom->ul = pcp_nvinfo.nvinfo[inst].fanspeed;
	    break;
	case NVIDIA_PERFSTATE:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_PERFSTATE])
		return PM_ERR_APPVERSION;
	    atom->ul = pcp_nvinfo.nvinfo[inst].perfstate;
	    break;
	case NVIDIA_ENERGY:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_ENERGY])
		return PM_ERR_APPVERSION;
	    atom->ull = pcp_nvinfo.nvinfo[inst].energy;
	    break;
	case NVIDIA_POWER:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_POWER])
		return PM_ERR_APPVERSION;
	    atom->ul = pcp_nvinfo.nvinfo[inst].power;
	    break;
	case NVIDIA_NPROCS:
	    atom->ul = pcp_nvinfo.nvinfo[inst].nprocs;
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
	case NVIDIA_GPUACTIVE_ACCUM:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_GPUACTIVE_ACCUM])
		return PM_ERR_VALUE;
	    atom->ull = pcp_nvinfo.nvinfo[inst].gpuutilaccum;
	    break;
	case NVIDIA_MEMACTIVE_ACCUM:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_MEMACTIVE_ACCUM])
		return PM_ERR_VALUE;
	    atom->ull = pcp_nvinfo.nvinfo[inst].memutilaccum;
	    break;
	case NVIDIA_MEMUSED_ACCUM:
	    if (pcp_nvinfo.nvinfo[inst].failed[NVIDIA_MEMUSED_ACCUM])
		return PM_ERR_VALUE;
	    atom->ull = pcp_nvinfo.nvinfo[inst].memaccum;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case 1:	/* nvidia.proc - per-card, per-process metrics */
	if (pmdaCacheLookup(mdesc->m_desc.indom, inst, NULL, (void **)&nvproc) < 0)
	    return PM_ERR_INST;
	switch (item) {
	case NVIDIA_GPROC_SAMPLES:
	    atom->ull = nvproc->samples;
	    break;
	case NVIDIA_GPROC_MEMUSED:
	    atom->ull = nvproc->memused;
	    break;
	case NVIDIA_GPROC_MEMACCUM:
	    atom->ull = nvproc->memaccum;
	    break;
	case NVIDIA_GPROC_GPUACTIVE:
	    atom->ul = nvproc->acct.gpuUtilization;
	    break;
	case NVIDIA_GPROC_MEMACTIVE:
	    atom->ul = nvproc->acct.memoryUtilization;
	    break;
	case NVIDIA_GPROC_TIME:
	    atom->ull = nvproc->acct.time;
	    break;
	case NVIDIA_GPROC_RUNNING:
	    atom->ul = nvproc->acct.isRunning;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case 2:	/* nvidia.proc.all per-process metrics */
	if ((node = __pmHashSearch(inst, &processes)) == NULL)
	    return PM_ERR_INST;
	proc = (process_t *)node->data;
	if ((proc->flags & (COMPUTE|GRAPHICS)) == 0)
	    return 0;
	switch (item) {
	case NVIDIA_PROC_SAMPLES:
	    atom->ull = proc->acct[PROCESS_COMPUTE].samples +
			proc->acct[PROCESS_GRAPHICS].samples;
	    break;
	case NVIDIA_PROC_MEMUSED:
	    atom->ull = proc->acct[PROCESS_COMPUTE].memused +
			proc->acct[PROCESS_GRAPHICS].memused;
	    break;
	case NVIDIA_PROC_MEMACCUM:
	    atom->ull = proc->acct[PROCESS_COMPUTE].memaccum +
			proc->acct[PROCESS_GRAPHICS].memaccum;
	    break;
	case NVIDIA_PROC_GPUACTIVE:
	    atom->ul  = proc->acct[PROCESS_COMPUTE].gpuutil +
			proc->acct[PROCESS_GRAPHICS].gpuutil;
	    break;
	case NVIDIA_PROC_MEMACTIVE:
	    atom->ul  = proc->acct[PROCESS_COMPUTE].memutil +
			proc->acct[PROCESS_GRAPHICS].memutil;
	    break;
	case NVIDIA_PROC_TIME:
	    atom->ull = proc->acct[PROCESS_COMPUTE].time +
			proc->acct[PROCESS_GRAPHICS].time;
	    break;
	case NVIDIA_PROC_GPULIST:
	    atom->ull = proc->acct[PROCESS_COMPUTE].gpulist |
			proc->acct[PROCESS_GRAPHICS].gpulist;
	    break;
	case NVIDIA_PROC_RUNNING:
	    atom->ul  = proc->acct[PROCESS_COMPUTE].running |
			proc->acct[PROCESS_GRAPHICS].running;
	    break;
	case NVIDIA_PROC_NGPUS:
	    atom->ul  = proc->acct[PROCESS_COMPUTE].ngpus +
			proc->acct[PROCESS_GRAPHICS].ngpus;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case 3:	/* nvidia.proc.compute per-process metrics */
    case 4:	/* nvidia.proc.graphics per-process metrics */
	if ((node = __pmHashSearch(inst, &processes)) == NULL)
	    return PM_ERR_INST;
	proc = (process_t *)node->data;
	mode = (cluster == 3)? COMPUTE : GRAPHICS;
	if ((proc->flags & mode) == 0)
	    return 0;
	mode = (cluster == 3)? PROCESS_COMPUTE : PROCESS_GRAPHICS;
	switch (item) {
	case NVIDIA_PROC_SAMPLES:
	    atom->ull = proc->acct[mode].samples;
	    break;
	case NVIDIA_PROC_MEMUSED:
	    atom->ull = proc->acct[mode].memused;
	    break;
	case NVIDIA_PROC_MEMACCUM:
	    atom->ull = proc->acct[mode].memaccum;
	    break;
	case NVIDIA_PROC_GPUACTIVE:
	    atom->ul  = proc->acct[mode].gpuutil;
	    break;
	case NVIDIA_PROC_MEMACTIVE:
	    atom->ul  = proc->acct[mode].memutil;
	    break;
	case NVIDIA_PROC_TIME:
	    atom->ull = proc->acct[mode].time;
	    break;
	case NVIDIA_PROC_GPULIST:
	    atom->ull = proc->acct[mode].gpulist;
	    break;
	case NVIDIA_PROC_RUNNING:
	    atom->ul  = proc->acct[mode].running;
	    break;
	case NVIDIA_PROC_NGPUS:
	    atom->ul  = proc->acct[mode].ngpus;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;
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
	return pmdaAddLabels(lp, "{\"gpu\":%u,\"uuid\":\"%s\"}",
				pcp_nvinfo.nvinfo[inst].cardid,
				pcp_nvinfo.nvinfo[inst].uuid);
    case GPROC_INDOM:
	sts = pmdaCacheLookup(indom, inst, NULL, (void **)&nvproc);
	if (sts < 0 || sts == PMDA_CACHE_INACTIVE)
	    return 0;
	return pmdaAddLabels(lp, "{\"gpu\":%u,\"pid\":%u}",
				nvproc->cardid, nvproc->pid);
    default:
	break;
    }
    return 0;
}

static int
nvidia_labelItem(pmID pmid, pmLabelSet **lp)
{
    if (pmID_cluster(pmid) != 0)
	return 0;
    switch (pmID_item(pmid)) {
    case NVIDIA_TEMPERATURE:
	pmdaAddLabels(lp, "{\"units\":\"degrees celsius\"}");
	return 1;
    case NVIDIA_ENERGY:
	pmdaAddLabels(lp, "{\"units\":\"millijoules\"}");
	return 1;
    case NVIDIA_POWER:
	pmdaAddLabels(lp, "{\"units\":\"milliwatts\"}");
	return 1;
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
    case PM_LABEL_ITEM:
	if ((sts = nvidia_labelItem((pmID)ident, lpp)) < 0)
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

static void
nvidia_timer(int sig, void *ptr)
{
    (void)sig; (void)ptr;
    autorefresh = 1;
}

static void
nvidia_main(pmdaInterface *dispatch)
{
    fd_set		readyfds, fds;
    int			nready, pmcdfd, maxfd = 0;

    if ((pmcdfd = __pmdaInFd(dispatch)) < 0)
	exit(1);
    if (pmcdfd > maxfd)
	maxfd = pmcdfd;

    FD_ZERO(&fds);
    FD_SET(pmcdfd, &fds);

    /* arm interval timer */
    if (autorefresh == 1 &&
	__pmAFregister(&interval, NULL, nvidia_timer) < 0) {
	pmNotifyErr(LOG_ERR, "registering event interval handler");
	exit(1);
    }

    for (;;) {
	memcpy(&readyfds, &fds, sizeof(readyfds));
	nready = select(maxfd+1, &readyfds, NULL, NULL, NULL);
	if (pmDebugOptions.appl2)
	    pmNotifyErr(LOG_DEBUG, "select: nready=%d autorefresh=%d",
			nready, autorefresh);
	if (nready < 0) {
	    if (neterror() != EINTR) {
		pmNotifyErr(LOG_ERR, "select failure: %s", netstrerror());
		exit(1);
	    } else if (autorefresh == 0) {
		continue;
	    }
	}

	__pmAFblock();
	if (nready > 0 && FD_ISSET(pmcdfd, &readyfds)) {
	    if (pmDebugOptions.appl0)
		pmNotifyErr(LOG_DEBUG, "processing pmcd PDU [fd=%d]", pmcdfd);
	    if (__pmdaMainPDU(dispatch) < 0) {
		__pmAFunblock();
		exit(1);        /* fatal if we lose pmcd */
	    }
	    if (pmDebugOptions.appl0)
		pmNotifyErr(LOG_DEBUG, "completed pmcd PDU [fd=%d]", pmcdfd);
	}
	if (autorefresh > 0) {
	    autorefresh = 0;
	    refresh(&pcp_nvinfo, 1);
	}
        __pmAFunblock();
    }
}

static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMOPT_INTERVAL,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

static pmdaOptions opts = {
    .short_options = "D:d:l:t:?",
    .long_options = longopts,
};

int
main(int argc, char **argv)
{
    pmdaInterface	desc;
    char		*endnum;
    int			c;

    isDSO = 0;
    pmSetProgname(argv[0]);

    initializeHelpPath();
    pmdaDaemon(&desc, PMDA_INTERFACE_7, pmGetProgname(), NVML,
		"nvidia.log", mypath);

    while ((c = pmdaGetOptions(argc, argv, &opts, &desc)) != EOF) {
	switch (c) {
	    case 't':
		if (pmParseInterval(opts.optarg, &interval, &endnum) < 0) {
		    fprintf(stderr, "%s: -s requires a time interval: %s\n",
			    pmGetProgname(), endnum);
		    free(endnum);
		    opts.errors++;
		}
		autorefresh = 1;	/* enable timers, non-default */
		break;
	    default:
		opts.errors++;
		break;
	}
    }
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    pmdaOpenLog(&desc);
    pmdaConnect(&desc);
    nvidia_init(&desc);
    nvidia_main(&desc);

    exit(0);
}
