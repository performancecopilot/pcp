/*
 * Copyright (c) 2024 Red Hat.
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

#include "drm.h"
#include <libdrm/amdgpu.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/syslog.h>

/* InDom table (set of graphics cards) */
enum {
    GCARD_INDOM = 0,
    INDOM_COUNT,
};
pmdaIndom indomtab[] = {
    {GCARD_INDOM, 0, NULL},
};

enum {
    GCARD_CLUSTER = 0,
    MEMORY_CLUSTER,
    GPU_CLUSTER,

    CLUSTER_COUNT
};

/* List of metric item numbers - increasing from zero, no holes.
 * Double check against `pmns` definition file.
 */
enum {
  AMDGPU_NUMCARDS = 0,
  AMDGPU_CARDNAME,
  AMDGPU_CARDID,
  AMDGPU_MEMORY,
  AMDGPU_GPU,

  AMDGPU_METRIC_COUNT
};

enum {
  AMDGPU_MEMORY_USED = 0,
  AMDGPU_MEMORY_TOTAL,
  AMDGPU_MEMORY_FREE,
  AMDGPU_MEMORY_USED_ACCUM,
  AMDGPU_MEMORY_CLOCK,
  AMDGPU_MEMORY_CLOCK_MAX,

  AMDGPU_MEMORY_METRIC_COUNT
};

enum {
  AMDGPU_GPU_TEMPERATURE,
  AMDGPU_GPU_LOAD,
  AMDGPU_GPU_AVG_PWR,
  AMDGPU_GPU_CLOCK,
  AMDGPU_GPU_CLOCK_MAX,

  AMDGPU_GPU_METRIC_COUNT
};
#define MAX_ITEM_COUNT AMDGPU_MEMORY_METRIC_COUNT

enum {
  AMDGPU_NAME_REFRESHER,
  AMDGPU_GPU_INFO_REFRESHER,
  AMDGPU_GPU_CLOCK_REFRESHER,
  AMDGPU_GPU_TEMPERATURE_REFRESHER,
  AMDGPU_GPU_LOAD_REFRESHER,
  AMDGPU_GPU_AVG_PWR_REFRESHER,
  AMDGPU_MEMORY_INFO_REFRESHER,
  AMDGPU_MEMORY_CLOCK_REFRESHER,

  AMDGPU_REFRESHER_COUNT
};

struct {
    int (*refresher)(amdgpu_device_handle, void *);
    uint32_t needs_refresh;
} amd_refresher[AMDGPU_REFRESHER_COUNT] = {
      { .refresher = &DRMDeviceGetName},
      { .refresher = &DRMDeviceGetGPUInfo},
      { .refresher = &DRMDeviceGetGPUClock},
      { .refresher = &DRMDeviceGetTemperature},
      { .refresher = &DRMDeviceGetGPULoad},
      { .refresher = &DRMDeviceGetGPUAveragePower},
      { .refresher = &DRMDeviceGetMemoryInfo},
      { .refresher = &DRMDeviceGetMemoryClock},

},*refresher_list[CLUSTER_COUNT][MAX_ITEM_COUNT] = {
      {
	NULL, /* There is no refresher for the card number */
	&amd_refresher[AMDGPU_NAME_REFRESHER],
	NULL, /* There is no refresher for the card ID */
      },
      {
	&amd_refresher[AMDGPU_MEMORY_INFO_REFRESHER], /* Memory used*/
	&amd_refresher[AMDGPU_MEMORY_INFO_REFRESHER], /* Total Memory */
	&amd_refresher[AMDGPU_MEMORY_INFO_REFRESHER], /* Memory free */
	&amd_refresher[AMDGPU_MEMORY_INFO_REFRESHER], /* Accumulated used memory */
	&amd_refresher[AMDGPU_MEMORY_CLOCK_REFRESHER], /* Memory clock, current */
	&amd_refresher[AMDGPU_GPU_INFO_REFRESHER], /* Memory clock, max */
      },
      {
	&amd_refresher[AMDGPU_GPU_TEMPERATURE_REFRESHER], /* GPU temperature */
	&amd_refresher[AMDGPU_GPU_LOAD_REFRESHER], /* GPU load (percent) */
	&amd_refresher[AMDGPU_GPU_AVG_PWR_REFRESHER], /* GPU Average power */
	&amd_refresher[AMDGPU_GPU_CLOCK_REFRESHER], /* GPU clock, current */
	&amd_refresher[AMDGPU_GPU_INFO_REFRESHER], /* GPU clock, max */
      },
};

/* Table of metrics exported by this PMDA */
static pmdaMetric metrictab[] = {
    {NULL,
     {PMDA_PMID(0, AMDGPU_NUMCARDS), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0, 0, 0, 0, 0, 0)}},
    {NULL,
     {PMDA_PMID(0, AMDGPU_CARDNAME), PM_TYPE_STRING, GCARD_INDOM, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0, 0, 0, 0, 0, 0)}},
    {NULL,
     {PMDA_PMID(0, AMDGPU_CARDID), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0, 0, 0, 0, 0, 0)}},

    {NULL,
     {PMDA_PMID(1, AMDGPU_MEMORY_USED), PM_TYPE_U64, GCARD_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)}},
    {NULL,
     {PMDA_PMID(1, AMDGPU_MEMORY_TOTAL), PM_TYPE_U64, GCARD_INDOM, PM_SEM_DISCRETE,
      PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)}},
    {NULL,
     {PMDA_PMID(1, AMDGPU_MEMORY_FREE), PM_TYPE_U64, GCARD_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)}},
    {NULL,
     {PMDA_PMID(1, AMDGPU_MEMORY_USED_ACCUM), PM_TYPE_U64, GCARD_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
    {NULL,
     {PMDA_PMID(1, AMDGPU_MEMORY_CLOCK), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0, -1, 0, 0, PM_TIME_USEC, 0) } },
    {NULL,
     {PMDA_PMID(1, AMDGPU_MEMORY_CLOCK_MAX), PM_TYPE_U32, GCARD_INDOM, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0, -1, 0, 0, PM_TIME_USEC, 0) } },

    {NULL,
     {PMDA_PMID(2, AMDGPU_GPU_TEMPERATURE), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    {NULL,
     {PMDA_PMID(2, AMDGPU_GPU_LOAD), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    {NULL,
     {PMDA_PMID(2, AMDGPU_GPU_AVG_PWR), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    {NULL,
     {PMDA_PMID(2, AMDGPU_GPU_CLOCK), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0, -1, 0, 0, PM_TIME_USEC, 0) } },
    {NULL,
     {PMDA_PMID(2, AMDGPU_GPU_CLOCK_MAX), PM_TYPE_U32, GCARD_INDOM, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0, -1, 0, 0, PM_TIME_USEC, 0) } },
};

/* GCARD_INDOM struct, stats that are per card */
typedef struct {
  int32_t failed[CLUSTER_COUNT][MAX_ITEM_COUNT];
  char name[64];
  struct amdgpu_gpu_info gpu_info;
  uint64_t memaccum;
  drmMemory_t memory;
  uint32_t gpu_clock;
  uint32_t mem_clock;
  uint32_t temperature;
  uint32_t load;
  uint32_t avg_pwr;
  int fd;
  amdgpu_device_handle amd_device;
} amdgpu_info_t;

/* overall struct, holds instance values, indom and instance struct arrays */
typedef struct {
  uint32_t numcards;
  uint32_t maxcards;
  drmDevicePtr *devs;
  amdgpu_info_t *info;
  pmdaIndom *indom;
} pcp_amdgpuinfo_t;

static pcp_amdgpuinfo_t pcp_amdgpuinfo;
static char mypath[MAXPATHLEN];
static int isDSO = 1;
static int drm_initialized;
static int autorefresh = -1;
static struct timeval interval;

static int setup_gcard_indom(void) {
  unsigned int device_count = 0;
  pmdaIndom *idp = &indomtab[GCARD_INDOM];
  char gpuname[32], *name;
  int i, sts;

  /* Initialize instance domain and instances. */
  if ((sts = DRMDeviceGetDevices(&pcp_amdgpuinfo.devs,
				      &pcp_amdgpuinfo.maxcards,
				      &pcp_amdgpuinfo.numcards)) != DRM_SUCCESS) {
    pmNotifyErr(LOG_ERR, "DrmDeviceGetDevies: %s", DRMErrStr(sts));
    return sts;
  }

  device_count = pcp_amdgpuinfo.numcards;
  pmNotifyErr(LOG_INFO, "setup_gcard_indom: got %d cards", device_count);

  pcp_amdgpuinfo.indom = idp;
  pcp_amdgpuinfo.indom->it_numinst = 0;
  pcp_amdgpuinfo.indom->it_set =
      (pmdaInstid *)calloc(device_count, sizeof(pmdaInstid));

  if (!pcp_amdgpuinfo.indom->it_set) {
    pmNoMem("gcard indom", device_count * sizeof(pmdaInstid), PM_RECOV_ERR);
    free(pcp_amdgpuinfo.devs);
    return -ENOMEM;
  }

  if ((pcp_amdgpuinfo.info = (amdgpu_info_t *)calloc(
           device_count, sizeof(amdgpu_info_t))) == NULL) {
    pmNoMem("gcard values", device_count * sizeof(amdgpu_info_t), PM_RECOV_ERR);
    free(pcp_amdgpuinfo.devs);
    free(pcp_amdgpuinfo.indom->it_set);
    return -ENOMEM;
  }

  for (i = 0; i < device_count; i++) {
    drmDevicePtr dev = pcp_amdgpuinfo.devs[i];
    amdgpu_info_t *info = &pcp_amdgpuinfo.info[i];

    pcp_amdgpuinfo.indom->it_set[i].i_inst = i;
    pmsprintf(gpuname, sizeof(gpuname), "gpu%d", i);
    if ((name = strdup(gpuname)) == NULL) {
      pmNoMem("gcard instname", strlen(gpuname), PM_RECOV_ERR);
      while (--i)
        free(pcp_amdgpuinfo.indom->it_set[i].i_name);
      free(pcp_amdgpuinfo.devs);
      free(pcp_amdgpuinfo.indom->it_set);
      free(pcp_amdgpuinfo.info);
      return -ENOMEM;
    }
    pcp_amdgpuinfo.indom->it_set[i].i_name = name;

    info->fd = -1;
    getAMDDevice(dev, &info->amd_device, &info->fd);
    /* Get static values */
    if (DRMDeviceGetGPUInfo(info->amd_device, &info->gpu_info)) {
	info->failed[MEMORY_CLUSTER][AMDGPU_MEMORY_CLOCK_MAX] = 1;
	info->failed[GPU_CLUSTER][AMDGPU_GPU_CLOCK_MAX] = 1;
    }
  }

  pcp_amdgpuinfo.indom->it_numinst = device_count;
  return 0;
}

static int refresh(pcp_amdgpuinfo_t *amdgpuinfo, uint32_t to_refresh)
{
  int i;

  if (!drm_initialized) {
      int ret = setup_gcard_indom();

      if (ret)
	return ret;

      drm_initialized = 1;
  }

  for (i = 0; i < amdgpuinfo->numcards && i < amdgpuinfo->maxcards; i++) {
      drmMemory_t memory = {0};
      amdgpu_info_t *info = &amdgpuinfo->info[i];
      void *param = NULL;
      int ret = DRM_SUCCESS;

      switch (to_refresh) {
	case AMDGPU_NAME_REFRESHER:
	  param = info->name;
	  break;
	case AMDGPU_GPU_INFO_REFRESHER:
	  param = &info->gpu_info;
	  break;
	case AMDGPU_GPU_CLOCK_REFRESHER:
	  param = &info->gpu_clock;
	  break;
	case AMDGPU_GPU_TEMPERATURE_REFRESHER:
	  param = &info->temperature;
	  break;
	case AMDGPU_GPU_LOAD_REFRESHER:
	  param = &info->load;
	  break;
	case AMDGPU_GPU_AVG_PWR_REFRESHER:
	  param = &info->avg_pwr;
	  break;
	case AMDGPU_MEMORY_INFO_REFRESHER:
	  param = &memory;
	  break;
	case AMDGPU_MEMORY_CLOCK_REFRESHER:
	  param = &info->mem_clock;
	  break;
      }

      ret = amd_refresher[to_refresh].refresher(info->amd_device, param);

      if (ret != DRM_SUCCESS) {
	  /* Mark all metrics that depend on the same refresher as failed */
	  for (int j = 0; j < CLUSTER_COUNT; j++)
	    for (int k = 0; k < MAX_ITEM_COUNT; k++) {
		if (&amd_refresher[to_refresh] == refresher_list[j][k])
		  info->failed[j][k] = 1;
	    }

	  continue;
      }

      if (param == &memory) {
	  info->memory = memory; /* struct copy */
	  info->memaccum += memory.used;
      }
  }

  return 0;
}

static int amdgpu_instance(pmInDom indom, int inst, char *name,
                           pmInResult **result, pmdaExt *pmda) {
  return pmdaInstance(indom, inst, name, result, pmda);
}

/*
 * Wrapper for pmdaFetch which refresh the set of values once per fetch
 * PDU.  The fetchCallback is then called once per-metric/instance pair
 * to perform the actual filling of the pmResult (via each pmAtomValue).
 */
static int amdgpu_fetch(int numpmid, pmID pmidlist[], pmResult **resp,
                        pmdaExt *pmda)
{
  uint32_t i = 0;

  for (i = 0; i <= numpmid; i++) {
      unsigned int cluster = pmID_cluster(pmidlist[i]);
      unsigned int item = pmID_item(pmidlist[i]);

      if (cluster >= CLUSTER_COUNT || item >= MAX_ITEM_COUNT)
	continue;
      if (refresher_list[cluster][item] == NULL)
	continue;

      refresher_list[cluster][item]->needs_refresh = 1;
  }

  for (i = 0; i < AMDGPU_REFRESHER_COUNT; i++) {
      if (!amd_refresher[i].needs_refresh)
	continue;
      amd_refresher[i].needs_refresh = 0;
      refresh(&pcp_amdgpuinfo, i);
  }

  return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int amdgpu_fetchCallBack(pmdaMetric *mdesc, unsigned int inst,
                                pmAtomValue *atom) {
  unsigned int cluster = pmID_cluster(mdesc->m_desc.pmid);
  unsigned int item = pmID_item(mdesc->m_desc.pmid);

  if (item != 0 && cluster == 0 && inst > indomtab[GCARD_INDOM].it_numinst)
    return PM_ERR_INST;

  if (inst < indomtab[GCARD_INDOM].it_numinst &&
      pcp_amdgpuinfo.info[inst].failed[cluster][item])
    return PM_ERR_VALUE;

  switch (cluster) {
  case GCARD_CLUSTER: /* amdgpu general and per-card metrics */
    switch (item) {
    case AMDGPU_NUMCARDS:
      atom->ul = pcp_amdgpuinfo.numcards;
      break;
    case AMDGPU_CARDID:
      atom->ul = inst;
      break;
    case AMDGPU_CARDNAME:
      atom->cp = pcp_amdgpuinfo.info[inst].name;
      break;
    default:
      return PM_ERR_PMID;
    }
    break;
  case MEMORY_CLUSTER: /* Memory related metrics */
    switch (item) {
    case AMDGPU_MEMORY_USED:
      atom->ull = pcp_amdgpuinfo.info[inst].memory.used;
      break;
    case AMDGPU_MEMORY_TOTAL:
      atom->ull = pcp_amdgpuinfo.info[inst].memory.total;
      break;
    case AMDGPU_MEMORY_FREE:
      atom->ull =
	pcp_amdgpuinfo.info[inst].memory.usable -
	pcp_amdgpuinfo.info[inst].memory.used;
      break;
    case AMDGPU_MEMORY_USED_ACCUM:
      atom->ull = pcp_amdgpuinfo.info[inst].memaccum;
      break;
    case AMDGPU_MEMORY_CLOCK:
      /* The GPU speed is the memory clock (GFX_MCLK)
       * The GDDRx memory speed is the shader clock (GFX_SCLK)
       * In MHz
       */
      atom->ul = pcp_amdgpuinfo.info[inst].mem_clock;
      break;
    case AMDGPU_MEMORY_CLOCK_MAX:
      /* The GPU max speed is the max_memory_clk.
       * The GDDRx memory max speed is max_engine_clk
       * In MHz
       */
      atom->ul = pcp_amdgpuinfo.info[inst].gpu_info.max_engine_clk;
      break;
    default:
      return PM_ERR_PMID;
    }
    break;
  case GPU_CLUSTER: /* SOC related metrics */
    switch (item) {
    case AMDGPU_GPU_CLOCK:
      /* The GPU speed is the memory clock (GFX_MCLK)
       * The GDDRx memory speed is the shader clock (GFX_SCLK)
       * In MHz
       */
      atom->ul = pcp_amdgpuinfo.info[inst].gpu_clock;
      break;
    case AMDGPU_GPU_CLOCK_MAX:
      /* The GPU max speed is the max_memory_clk.
       * The GDDRx memory max speed is max_engine_clk
       * In MHz
       */
      atom->ul = pcp_amdgpuinfo.info[inst].gpu_info.max_memory_clk;
      break;
    case AMDGPU_GPU_TEMPERATURE:
      /* In millidegrees Celsius */
      atom->ul = pcp_amdgpuinfo.info[inst].temperature;
      break;
    case AMDGPU_GPU_LOAD:
      atom->ul = pcp_amdgpuinfo.info[inst].load;
      break;
    case AMDGPU_GPU_AVG_PWR:
      atom->ul = pcp_amdgpuinfo.info[inst].avg_pwr;
      break;
    default:
      return PM_ERR_PMID;
    }
    break;
  default:
    return PM_ERR_PMID;
  }

  return 1;
}

static int amdgpu_labelCallBack(pmInDom indom, unsigned int inst,
                                pmLabelSet **lp) {
  if (indom == PM_INDOM_NULL)
    return 0;

  switch (pmInDom_serial(indom)) {
  case GCARD_INDOM:
    return pmdaAddLabels(lp, "{\"gpu\":%s}", pcp_amdgpuinfo.info[inst].name);
  default:
    break;
  }
  return 0;
}

static int amdgpu_labelInDom(pmInDom indom, pmLabelSet **lp) {
  switch (pmInDom_serial(indom)) {
  case GCARD_INDOM:
    pmdaAddLabels(lp, "{\"device_type\":\"gpu\"}");
    pmdaAddLabels(lp, "{\"indom_name\":\"per gpu\"}");
    return 1;
  default:
    break;
  }
  return 0;
}

static int amdgpu_labelItem(pmID pmid, pmLabelSet **lp) {
  if (pmID_cluster(pmid) != GPU_CLUSTER)
    return 0;

  if (pmID_item(pmid) != AMDGPU_GPU_TEMPERATURE)
    return 0;

  pmdaAddLabels(lp, "{\"measure\":\"temperature\"}");
  pmdaAddLabels(lp, "{\"units\":\"millidegrees Celsius\"}");
  return 1;
}

static int amdgpu_label(int ident, int type, pmLabelSet **lpp, pmdaExt *pmda) {
  switch (type) {
  case PM_LABEL_INDOM:
    amdgpu_labelInDom((pmInDom)ident, lpp);
    break;
  case PM_LABEL_CLUSTER:
    amdgpu_labelItem((pmID)ident, lpp);
    break;
  default:
    break;
  }
  return pmdaLabel(ident, type, lpp, pmda);
}

/**
 * Initializes the path to the help file for this PMDA.
 */
static void initializeHelpPath() {
  int sep = pmPathSeparator();
  pmsprintf(mypath, sizeof(mypath),
            "%s%c"
            "amdgpu"
            "%c"
            "help",
            pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
}

void __PMDA_INIT_CALL amdgpu_init(pmdaInterface *dp) {
  if (isDSO) {
    initializeHelpPath();
    pmdaDSO(dp, PMDA_INTERFACE_7, "amdgpu DSO", mypath);
  }

  if (dp->status != 0)
    return;

  if (!drm_initialized) {
    setup_gcard_indom();
    drm_initialized = 1;
  }

  dp->version.seven.instance = amdgpu_instance;
  dp->version.seven.fetch = amdgpu_fetch;
  dp->version.seven.label = amdgpu_label;
  pmdaSetFetchCallBack(dp, amdgpu_fetchCallBack);
  pmdaSetLabelCallBack(dp, amdgpu_labelCallBack);

  pmdaInit(dp, indomtab, sizeof(indomtab) / sizeof(indomtab[0]), metrictab,
           sizeof(metrictab) / sizeof(metrictab[0]));
}

static void amdgpu_timer(int sig, void *ptr) {
  (void)sig;
  (void)ptr;
  autorefresh = 1;
}

static void amdgpu_main(pmdaInterface *dispatch) {
  fd_set readyfds, fds;
  int pmcdfd, maxfd = 0;

  if ((pmcdfd = __pmdaInFd(dispatch)) < 0)
    exit(1);
  if (pmcdfd > maxfd)
    maxfd = pmcdfd;

  FD_ZERO(&fds);
  FD_SET(pmcdfd, &fds);

  /* arm interval timer */
  if (autorefresh == 1 && __pmAFregister(&interval, NULL, amdgpu_timer) < 0) {
    pmNotifyErr(LOG_ERR, "registering event interval handler");
    exit(1);
  }

  for (;;) {
    memcpy(&readyfds, &fds, sizeof(readyfds));
    int nready = select(maxfd + 1, &readyfds, NULL, NULL, NULL);
    if (pmDebugOptions.appl2)
      pmNotifyErr(LOG_DEBUG, "select: nready=%d autorefresh=%d", nready,
                  autorefresh);
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
        exit(1); /* fatal if we lose pmcd */
      }
      if (pmDebugOptions.appl0)
        pmNotifyErr(LOG_DEBUG, "completed pmcd PDU [fd=%d]", pmcdfd);
    }
    if (autorefresh > 0) {
      autorefresh = 0;
      for (int i = 0;i < AMDGPU_REFRESHER_COUNT;i++) {
	  pmNotifyErr(LOG_ERR, "Refreshing %d", i);
	  refresh(&pcp_amdgpuinfo, i);
      }
    }
    __pmAFunblock();
  }
}

static pmLongOptions longopts[] = {PMDA_OPTIONS_HEADER("Options"),
                                   PMOPT_DEBUG,
                                   PMDAOPT_DOMAIN,
                                   PMDAOPT_LOGFILE,
                                   PMOPT_INTERVAL,
                                   PMOPT_HELP,
                                   PMDA_OPTIONS_END};

static pmdaOptions opts = {
    .short_options = "D:d:l:t:?",
    .long_options = longopts,
};

int main(int argc, char **argv) {
  pmdaInterface desc = {0};
  char *endnum = NULL;
  int c;
  struct timespec ts;

  isDSO = 0;
  pmSetProgname(argv[0]);

  initializeHelpPath();
  pmdaDaemon(&desc, PMDA_INTERFACE_7, pmGetProgname(), AMDGPU, "amdgpu.log",
             mypath);

  while ((c = pmdaGetOptions(argc, argv, &opts, &desc)) != EOF) {
    switch (c) {
    case 't':
      if (pmParseInterval(opts.optarg, &ts, &endnum) < 0) {
        fprintf(stderr, "%s: -t requires a time interval: %s\n",
                pmGetProgname(), endnum);
        free(endnum);
        opts.errors++;
      }
      pmtimevalFromtimespec(&ts, &interval);
      autorefresh = 1; /* enable timers, non-default */
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
  amdgpu_init(&desc);
  amdgpu_main(&desc);

  exit(0);
}
