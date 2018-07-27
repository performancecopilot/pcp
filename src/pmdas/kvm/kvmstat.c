/*
 * kvm, configurable PMDA
 *
 * Copyright (c) 2018 Fujitsu.
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
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include "pmapi.h"
#include "libpcp.h"
#include "impl.h"
#include "pmda.h"
#include "domain.h"
#include "kvmstat.h"
#include "kvm_debug.h"

#define MAX_TRACE_NUM 200

static kvmstat_t		kvmstat;

static pmdaMetric *trace_metrics;
static __pmnsTree *pmns;

static char sep;
static int ntrace = 0;
static char *trace_nametab[MAX_TRACE_NUM];
static int cpus=0;
static int *group_fd;
static   char path[MAXPATHLEN];
static pmdaMetric *metrictab_t;

/* default trace fs path */
static char trace_path[BUFSIZ]="/sys/kernel/debug/tracing/events/kvm/";
static pmdaIndom indomtab[] = {
    { TRACE_INDOM, 0, NULL },
};

static pmInDom	*trace_indom = &indomtab[TRACE_INDOM].it_indom;

/* command line option handling - both short and long options */
static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_TEXT("\nExactly one of the following options may appear:"),
    PMDAOPT_INET,
    PMDAOPT_PIPE,
    PMDAOPT_UNIX,
    PMDAOPT_IPV6,
    PMDA_OPTIONS_END
};
static pmdaOptions opts = {
    .short_options = "D:d:i:l:pu:U:6:?",
    .long_options = longopts,
};

typedef struct {
    long long *value[MAX_TRACE_NUM];
} trace_value;

void refresh_kvm_trace()
{
    int i, j, sts;
    long long tmp_values[MAX_TRACE_NUM] = {0};
    //memset(tmp_values, 0, sizeof(tmp_values));
    char cpu[6];
    trace_value *trace_v;
    int changed = 0;
    for(i = 0; i < cpus; i++) {
        pmsprintf(cpu, sizeof(cpu), "cpu%d", i);
        if (pmdaCacheLookupName(*trace_indom, cpu, NULL, (void **)&trace_v) < 0 ||
            trace_v == NULL) {
            trace_v = (trace_value *)calloc(1, sizeof(trace_value));
            changed = 1;
        }
        if(read(group_fd[i], tmp_values, sizeof(tmp_values)) <= 0)
        {
            pmNotifyErr(LOG_ERR, "Read trace fd error.\n");
        }
        for(j = 0; j < ntrace; j++) {
            trace_v->value[j] = (long long *)tmp_values[j+1];
	}
        sts = pmdaCacheStore(*trace_indom, PMDA_CACHE_ADD, cpu, (void *)trace_v);
        if (sts < 0) {
            pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
        }
        memset(tmp_values, 0, sizeof(tmp_values));
        if (changed)
	     pmdaCacheOp(*trace_indom, PMDA_CACHE_SAVE);
    }

} 

long perf_event_open(struct perf_event_attr *kvm_event, pid_t pid,
                int cpu, int group_fd, unsigned long flags)
{
    int ret;
    ret = syscall(__NR_perf_event_open, kvm_event, pid, cpu, group_fd, flags);
    return ret;
}

int perf_event(int cpus, int *group_fd)
{
    int sts = 0;
    struct perf_event_attr pe;
    FILE *pFile = NULL;
    char temp[MAX_TRACE_NUM] = {0};
    int fd = 0;
    int cpu;
    int flag; 
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_TRACEPOINT;
    pe.size = sizeof(struct perf_event_attr);
    pe.sample_type = PERF_SAMPLE_RAW | PERF_SAMPLE_TIME | PERF_SAMPLE_CPU;
    pe.sample_period = 1;
    pe.read_format = PERF_FORMAT_GROUP; 
    DIR *kvm_dir;
    struct dirent *de;
    int offset = 0;    
    char path[MAX_TRACE_NUM];
    int i = 0;

    snprintf(path, sizeof(path), trace_path);
    path[sizeof(path)-1] = '\0';

    if ((kvm_dir = opendir(path)) == NULL)
        sts=1;

    for(cpu = 0; cpu < cpus ; cpu++) {
        flag = 0;
        group_fd[cpu] = -1;
        for (i = 0; i < ntrace; i ++)
        {
            while ((de = readdir(kvm_dir)) != NULL) {
                if (offset == 0)
                    offset = telldir(kvm_dir);
                if (!strncmp(de->d_name, ".", 1))
                    continue;
                if (!strncmp(de->d_name, "enable", 6) 
                     || !strncmp(de->d_name, "filter", 6))
                    continue;
                if (!strcmp(de->d_name, trace_nametab[i]))
                {
                    sprintf(path, "%s%s/id", trace_path, de->d_name);
                    pFile = fopen(path, "r");
                    pe.config = atoi(fgets(temp, sizeof(temp), pFile));
                    fclose(pFile);
                    fd = perf_event_open(&pe, -1, cpu, group_fd[cpu], 0);	     
                    if (fd == -1) {
	                pmNotifyErr(LOG_ERR, "perf_event_open error!\n");
	                sts = 1;
                    }
                    if (flag == 0) {
                        group_fd[cpu] = fd;
                        flag = 1;
                    }
                    if(ioctl(fd, PERF_EVENT_IOC_RESET, 0) == -1 ||
                        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) == -1)
                        pmNotifyErr(LOG_ERR, "ioctl failed 'PERF_EVENT_IOC_ENABLE'.\n");
                    break;
                }
            }
            seekdir(kvm_dir, offset);
        }
    }
    closedir(kvm_dir);
    return sts;
}

static pmdaMetric metrictab[] = {
/* kvm.efer_reload */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,0), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.exits */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,1), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.fpu_reload */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,2), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.halt_attempted_poll */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,3), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.halt_exits */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,4), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.halt_successful_poll */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,5), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.halt_wakeup */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,6), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.host_state_reload */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,7), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.hypercalls */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,8), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.insn_emulation */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,9), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.insn_emulation_fail */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,10), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.invlpg */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,11), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.io_exits */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,12), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.irq_exits */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,13), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.irq_injections */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,14), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.irq_window */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,15), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.largepages */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,16), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.mmio_exits */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,17), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.mmu_cache_miss */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,18), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.mmu_flooded */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,19), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.mmu_pde_zapped */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,20), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.mmu_pte_updated */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,21), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.mmu_pte_write */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,22), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.mmu_recycled */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,23), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.mmu_shadow_zapped */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,24), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.mmu_unsync */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,25), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.nmi_injections */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,26), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.nmi_window */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,27), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.pf_fixed */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,28), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.pf_guest */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,29), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.remote_tlb_flush */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,30), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.request_irq */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,31), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.signal_exits */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,32), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* kvm.tlb_flush */
    { NULL,
      { PMDA_PMID(CLUSTER_DEBUG,33), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
};

/*
 * Refresh Metrics' data
 * pmda         : pmdaExt
 * need_refresh : check clusters whether to be refresh or not.
 * return       : void
 */
static void
kvm_refresh(pmdaExt *pmda, int *need_refresh)
{
    if (need_refresh[CLUSTER_DEBUG])
        refresh_kvm(&kvmstat);
}

/*
 * callback provided to pmdaFetch
 * mdesc  : pmdaMetric transferred from pmcd.
 * inst   : instance transferred from pmcd.
 * atom   : return data buffer.
 * return : check whether the fetchCallBack get a valid data or not.
 *          0 means invalid data.
 *          1 means valid data
 *          other return please cheak pmapi.h
 */
static int
kvm_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    pmID		*idp = (pmID *)&(mdesc->m_desc.pmid);
    unsigned int	cluster = pmID_cluster(*idp);
    int sts = 0;
    trace_value *va;
    char *name;
    switch (cluster) {
    case CLUSTER_TRACE:
	if ((sts = pmdaCacheLookup(*trace_indom, inst, &name, (void **)&va)) != PMDA_CACHE_ACTIVE) {
	    if (sts < 0) {
	        pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst, pmErrStr(sts));
		return PM_ERR_INST;
	    }
        }
	atom->ll = (__int64_t)va->value[pmID_item(*idp)];
        break;
    case CLUSTER_DEBUG:
        switch (pmID_item(*idp)) {
        case 0:	/* kvm.efer_reload */
	    atom->ll = kvmstat.debug[0];
	    break;
        case 1:	/* kvm.exits */
	    atom->ll = kvmstat.debug[1];
	    break;
        case 2:	/* kvm.fpu_reload */
	    atom->ll = kvmstat.debug[2];
	    break;
        case 3:	/* kvm.halt_attempted_poll */
	    atom->ll = kvmstat.debug[3];
	    break;
        case 4:	/* kvm.halt_exits */
	    atom->ll = kvmstat.debug[4];
	    break;
        case 5:	/* kvm.halt_successful_poll */
	    atom->ll = kvmstat.debug[5];
	    break;
        case 6:	/* kvm.halt_wakeup */
	    atom->ll = kvmstat.debug[6];
	    break;
        case 7:	/* kvm.host_state_reload */
	    atom->ll = kvmstat.debug[7];
	    break;
        case 8:	/* kvm.hypercalls */
	    atom->ll = kvmstat.debug[8];
	    break;
        case 9:	/* kvm.insn_emulation */
	    atom->ll = kvmstat.debug[9];
	    break;
        case 10:/* kvm.insn_emulation_fail */
	    atom->ll = kvmstat.debug[10];
	    break;
        case 11:/* kvm.invlpg */
	    atom->ll = kvmstat.debug[11];
	    break;
        case 12:	/* kvm.io_exits */
	    atom->ll = kvmstat.debug[12];
	    break;
        case 13:	/* kvm.irq_exits */
	    atom->ll = kvmstat.debug[13];
	    break;
        case 14:	/* kvm.irq_injections */
	    atom->ll = kvmstat.debug[14];
	    break;
        case 15:	/* kvm.irq_window */
	    atom->ll = kvmstat.debug[15];
	    break;
        case 16:	/* kvm.largepages */
	    atom->ll = kvmstat.debug[16];
	    break;
        case 17:	/* kvm.mmio_exits */
	    atom->ll = kvmstat.debug[17];
	    break;
        case 18:	/* kvm.mmu_cache_miss */
	    atom->ll = kvmstat.debug[18];
	    break;
        case 19:	/* kvm.mmu_flooded */
	    atom->ll = kvmstat.debug[19];
	    break;
        case 20:	/* kvm.mmu_pde_zapped */
	    atom->ll = kvmstat.debug[20];
	    break;
        case 21:	/* kvm.mmu_pte_updated */
	    atom->ll = kvmstat.debug[21];
	    break;
        case 22:	/* kvm.mmu_pte_write */
	    atom->ll = kvmstat.debug[22];
	    break;
        case 23:	/* kvm.mmu_recycled */
	    atom->ll = kvmstat.debug[23];
	    break;
        case 24:	/* kvm.mmu_shadow_zapped */
	    atom->ll = kvmstat.debug[24];
	    break;
        case 25:	/* kvm.mmu_unsync */
	    atom->ll = kvmstat.debug[25];
	    break;
        case 26:	/* kvm.nmi_injections */
	    atom->ll = kvmstat.debug[26];
	    break;
        case 27:	/* kvm.nmi_window */
	    atom->ll = kvmstat.debug[27];
	    break;
        case 28:	/* kvm.pf_fixed */
	    atom->ll = kvmstat.debug[28];
	    break;
        case 29:	/* kvm.pf_guest */
	    atom->ll = kvmstat.debug[29];
	    break;
        case 30:	/* kvm.remote_tlb_flush */
	    atom->ll = kvmstat.debug[30];
	    break;
        case 31:	/* kvm.request_irq */
	    atom->ll = kvmstat.debug[31];
	    break;
        case 32:	/* kvm.signal_exits */
	    atom->ll = kvmstat.debug[32];
	    break;
        case 33:	/* kvm.tlb_flush */
	    atom->ll = kvmstat.debug[33];
	    break;
        default:
	    return PM_ERR_PMID;
        }
        break;
    default: /* unknown cluster */
	return PM_ERR_PMID;
    }
    return 1;
}

static int
trace_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    return pmdaTreePMID(pmns, name, pmid);
}

static int
trace_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    return pmdaTreeName(pmns, pmid, nameset);
}

static int
trace_children(const char *name, int traverse, char ***kids, int **sts,
		pmdaExt *pmda)
{
    return pmdaTreeChildren(pmns, name, traverse, kids, sts);
}

/*
 * used to call refresh function and return a valid(or invalid) metric data.
 * numpmid : metrics' number transferred to pmda.
 * pmidlist: list of metrics.
 * resp    : returned data buffer.
 * pmda    : pmdaExt.
 * return  : always return pmdaFetch(*).
 */
static int
kvm_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int		need_refresh[NUM_CLUSTERS];
    int i;
    memset(need_refresh, 0, sizeof(need_refresh));
    for (i=0; i < numpmid; i++) {
        pmID *idp = (pmID *)&(pmidlist[i]);
        if (pmID_cluster(*idp) < NUM_CLUSTERS) {
	      need_refresh[pmID_cluster(*idp)]++;
         }
    }
    kvm_refresh(pmda, need_refresh);
    refresh_kvm_trace();
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

void extract_trace_metrices()
{
    FILE *fp;
    char buf[BUFSIZ];
    sep = pmPathSeparator();

    pmsprintf(path, sizeof(path), "%s%c" "kvmstat" "%c" "trace_kvm.conf",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    path[sizeof(path)-1] = '\0';
    if ((fp = fopen(path, "rt")) == NULL)
	return -oserror();
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        if (strncmp(buf, "#", 1) == 0)
            continue;
        if (strncmp(buf, "[", 1) == 0)
            continue;
        if (strncmp(buf, "\n", 1) == 0
            || strncmp(buf, " ", 1) == 0
            || strncmp(buf, "\t", 1) == 0)    continue;
        
        if (sscanf(buf, "PATH=%s", trace_path) != 0) {
            continue; 
        }
        trace_nametab[ntrace] = (char *)malloc(sizeof(char));
        buf[strlen(buf) - 1] = '\0';
        strcpy(trace_nametab[ntrace], buf);
        ntrace++; 
    }
    fclose(fp);
}

/*
 * used to Initialize kvmstat pmda.
 * set interface function (fetch, store, instance etc.)
 * set instance domain.
 * run pmdaInit function.
 * dp     : pmdaInterface.
 * return : void
 */
void
__PMDA_INIT_CALL
kvmstat_init(pmdaInterface *dp)
{
    size_t nmetrics, nindoms;
    size_t tmetrics;
    int m = 0;
    int sts;
    char name[MAXPATHLEN];
    pmdaMetric *pmetric;
    char       *envpath;

    if (dp->status != 0)
	return;

    /* get the specified kvm trace metrics */ 
    extract_trace_metrices();

    nindoms = sizeof(indomtab)/sizeof(indomtab[0]);
    nmetrics = sizeof(metrictab)/sizeof(metrictab[0]);
    tmetrics = nmetrics + ntrace;

    /* allocate memory for metrictab_t */
    metrictab_t = malloc(tmetrics * sizeof(pmdaMetric));
    memcpy(metrictab_t, metrictab, sizeof(metrictab));
    pmetric = &metrictab_t[nmetrics];

    if ((trace_metrics = malloc(ntrace * sizeof(pmdaMetric))) != NULL) {
        for (m = 0; m < ntrace; m++) {
            trace_metrics[m].m_user = NULL; 
            trace_metrics[m].m_desc.pmid = PMDA_PMID(CLUSTER_TRACE, m);
            trace_metrics[m].m_desc.type = PM_TYPE_64;
            trace_metrics[m].m_desc.indom = TRACE_INDOM;
            trace_metrics[m].m_desc.sem = PM_SEM_INSTANT;
            memset(&trace_metrics[m].m_desc.units, 0, sizeof(pmUnits));
            memcpy(pmetric, &trace_metrics[m], sizeof(trace_metrics[m]));
            pmetric += 1;
        }
    } else {
	    pmNotifyErr(LOG_ERR, "%s: pmdaInit - out of memory\n",
				pmGetProgname());
            exit(0);
    }
    
    if ((envpath = getenv("LINUX_NCPUS")))
        cpus = atoi(envpath);
    else
        cpus = sysconf(_SC_NPROCESSORS_CONF);

    group_fd = malloc(cpus * sizeof(int));
    sts = perf_event(cpus, group_fd);

    dp->version.any.fetch = kvm_fetch;
    dp->version.six.pmid = trace_pmid;
    dp->version.six.name = trace_name;
    dp->version.six.children = trace_children;
    pmdaSetFetchCallBack(dp, kvm_fetchCallBack);

    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, indomtab, nindoms, metrictab_t, tmetrics);

    /* Create the dynamic PMNS tree and populate it. */
    if ((sts = __pmNewPMNS(&pmns)) < 0) {
	pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n",
			pmGetProgname(), pmErrStr(sts));
	pmns = NULL;
	return;
    }
    pmetric = &metrictab_t[nmetrics];
    for (m = 0; m < ntrace ; m++) {
	pmsprintf(name, sizeof(name),
			"kvm.trace.%s",  trace_nametab[m]);
	__pmAddPMNSNode(pmns, pmetric[m].m_desc.pmid, name);
    }

    /* for reverse (pmid->name) lookups */
    pmdaTreeRebuildHash(pmns, ntrace);
}

/*
 * main function.
 */
int
main(int argc, char **argv)
{
    pmdaInterface	dispatch;
    char		helppath[MAXPATHLEN];

    __pmSetProgname(argv[0]);

    snprintf(helppath, sizeof(helppath), "%s%c" "kvm" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_6, pmGetProgname(), KVM, "kvm.log", helppath);

    pmdaGetOptions(argc, argv,  &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    pmdaOpenLog(&dispatch);
    kvmstat_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
