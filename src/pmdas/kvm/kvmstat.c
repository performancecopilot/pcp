/*
 * Configurable Kernel Virtual Machine (KVM) PMDA
 *
 * Copyright (c) 2018 Fujitsu.
 * Copyright (c) 2018 Red Hat.
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
#include <ctype.h>
#include "pmapi.h"
#include "pmda.h"
#include "domain.h"
#include "kvmstat.h"
#include <dirent.h>
#include <sys/ioctl.h>
#ifdef HAVE_LINUX_PERF_EVENT_H
#include <asm/unistd.h>
#include <linux/perf_event.h>
typedef struct perf_event_attr perf_event_attr_t;
#endif

static int _isDSO;
static pmdaNameSpace *pmns;
static char *username;
static char helppath[MAXPATHLEN];
static char sep;

static int ntrace;
static char **trace_nametab;
static int ncpus;
static int *group_fd;
static char tracefs[MAXPATHLEN];
static char debugfs[MAXPATHLEN];
static pmdaMetric *tmetrictab;

static pmdaIndom indomtab[] = {
    { TRACE_INDOM, 0, NULL },
};

static pmInDom *trace_indom = &indomtab[TRACE_INDOM].it_indom;

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

static pmdaMetric metrictab[] = {
    { "efer_reload",
	{ PMDA_PMID(CLUSTER_DEBUG, 0), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "exits",
	{ PMDA_PMID(CLUSTER_DEBUG, 1), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "fpu_reload",
	{ PMDA_PMID(CLUSTER_DEBUG, 2), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "halt_attempted_poll",
	{ PMDA_PMID(CLUSTER_DEBUG, 3), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "halt_exits",
	{ PMDA_PMID(CLUSTER_DEBUG, 4), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "halt_successful_poll",
	{ PMDA_PMID(CLUSTER_DEBUG, 5), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "halt_wakeup",
	{ PMDA_PMID(CLUSTER_DEBUG, 6), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "host_state_reload",
	{ PMDA_PMID(CLUSTER_DEBUG, 7), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "hypercalls",
	{ PMDA_PMID(CLUSTER_DEBUG, 8), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "insn_emulation",
	{ PMDA_PMID(CLUSTER_DEBUG, 9), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "insn_emulation_fail",
	{ PMDA_PMID(CLUSTER_DEBUG, 10), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "invlpg",
	{ PMDA_PMID(CLUSTER_DEBUG, 11), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "io_exits",
	{ PMDA_PMID(CLUSTER_DEBUG, 12), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "irq_exits",
	{ PMDA_PMID(CLUSTER_DEBUG, 13), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "irq_injections",
	{ PMDA_PMID(CLUSTER_DEBUG, 14), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "irq_window",
	{ PMDA_PMID(CLUSTER_DEBUG, 15), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "largepages",
	{ PMDA_PMID(CLUSTER_DEBUG, 16), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "mmio_exits",
	{ PMDA_PMID(CLUSTER_DEBUG, 17), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "mmu_cache_miss",
	{ PMDA_PMID(CLUSTER_DEBUG, 18), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "mmu_flooded",
	{ PMDA_PMID(CLUSTER_DEBUG, 19), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "mmu_pde_zapped",
	{ PMDA_PMID(CLUSTER_DEBUG, 20), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "mmu_pte_updated",
	{ PMDA_PMID(CLUSTER_DEBUG, 21), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "mmu_pte_write",
	{ PMDA_PMID(CLUSTER_DEBUG, 22), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "mmu_recycled",
	{ PMDA_PMID(CLUSTER_DEBUG, 23), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "mmu_shadow_zapped",
	{ PMDA_PMID(CLUSTER_DEBUG, 24), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "mmu_unsync",
	{ PMDA_PMID(CLUSTER_DEBUG, 25), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "nmi_injections",
	{ PMDA_PMID(CLUSTER_DEBUG, 26), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "nmi_window",
	{ PMDA_PMID(CLUSTER_DEBUG, 27), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "pf_fixed",
	{ PMDA_PMID(CLUSTER_DEBUG, 28), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "pf_guest",
	{ PMDA_PMID(CLUSTER_DEBUG, 29), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "remote_tlb_flush",
	{ PMDA_PMID(CLUSTER_DEBUG, 30), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "request_irq",
	{ PMDA_PMID(CLUSTER_DEBUG, 31), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "signal_exits",
	{ PMDA_PMID(CLUSTER_DEBUG, 32), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { "tlb_flush",
	{ PMDA_PMID(CLUSTER_DEBUG, 33), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
#define KVM_DEBUG_COUNT	34

    { "trace.count",	/* final entry - insert new entries above */
	{ PMDA_PMID(CLUSTER_TRACE, 0), PM_TYPE_U64, PM_INDOM_NULL,
	    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
};

typedef struct kvm_debug {
    unsigned long long	value[KVM_DEBUG_COUNT];
} kvm_debug_value_t;
static kvm_debug_value_t kvmstat;

static int
kvm_debug_refresh(kvm_debug_value_t *kvm)
{
    struct dirent	*de;
    FILE   		*fp;
    DIR			*kvm_dir;
    char		buffer[256];
    char		path[MAXPATHLEN];
    int			i, sts = 0;

    pmsprintf(path, sizeof(path), "%s/kvm", debugfs);
    if ((kvm_dir = opendir(path)) == NULL)
	return -oserror();

    while ((de = readdir(kvm_dir)) != NULL) {   
	if (!strncmp(de->d_name, ".", 1))
	    continue;

	pmsprintf(path, sizeof(path), "%s/kvm/%s", debugfs, de->d_name);
	path[sizeof(path)-1] = '\0';
	if ((fp = fopen(path, "r")) == NULL) {
	    sts = -oserror();
	    break;
	}

        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
	    for (i = 0; i < KVM_DEBUG_COUNT; i++) {
		if (strcmp(de->d_name, metrictab[i].m_user) != 0)
		    continue;
		kvm->value[i] = strtoull(buffer, NULL, 0);
	    }
        }
        fclose(fp);
    } 
    closedir(kvm_dir);
    return sts;
}

typedef struct kvm_trace_value {
    unsigned long long	value;
} kvm_trace_value_t;

static void
kvm_trace_refresh(void)
{
    static kvm_trace_value_t *buffer;
    kvm_trace_value_t	*ktrace = NULL;
    char		cpu[64];
    ssize_t		bytes;
    size_t		ksize = ntrace * sizeof(unsigned long long);
    size_t		bufsize = ksize + sizeof(unsigned long long);
    int			i, sts, changed = 0;

    if (ntrace == 0 || group_fd == NULL)
	return;

    if (buffer == NULL) {
	if ((buffer = malloc(bufsize)) == NULL) {
	    pmNotifyErr(LOG_ERR, "kvm_trace_refresh OOM (%d)", ntrace);
	    return;
	}
    }

    for (i = 0; i < ncpus; i++) {
	pmsprintf(cpu, sizeof(cpu), "cpu%d", i);
	if (pmdaCacheLookupName(*trace_indom, cpu, NULL, (void **)&ktrace) < 0 ||
	    ktrace == NULL) {
	    ktrace = (kvm_trace_value_t *)calloc(1, ksize);
	    if (ktrace == NULL)
		continue;
	    changed = 1;
	}
	memset(buffer, 0, bufsize);
	if ((bytes = read(group_fd[i], buffer, bufsize)) < 0) {
	    pmNotifyErr(LOG_ERR, "kvm_trace_refresh trace read error: %s",
			    strerror(errno));
	    continue;
	}
	if (bytes == bufsize)
	    memcpy(ktrace, buffer+1, ksize);
	else
	    memset(ktrace, 0, ksize);
	sts = pmdaCacheStore(*trace_indom, PMDA_CACHE_ADD, cpu, (void *)ktrace);
	if (sts < 0)
	    pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
	if (changed)
	     pmdaCacheOp(*trace_indom, PMDA_CACHE_SAVE);
    }
} 

#ifdef HAVE_LINUX_PERF_EVENT_H
static long
perf_event_open(perf_event_attr_t *kvm_event, pid_t pid,
		int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, kvm_event, pid, cpu, group_fd, flags);
}

static int
perf_event(int ncpus, int *group_fd)
{
    struct dirent	*de;
    perf_event_attr_t	pe;
    FILE		*pfile;
    DIR			*dir;
    char		temp[256];
    int			i, fd = 0, cpu, flag, offset = 0, sts = 0;
    char		path[MAXPATHLEN];

    memset(&pe, 0, sizeof(perf_event_attr_t));
    pe.type = PERF_TYPE_TRACEPOINT;
    pe.size = sizeof(struct perf_event_attr);
    pe.sample_type = PERF_SAMPLE_RAW | PERF_SAMPLE_TIME | PERF_SAMPLE_CPU;
    pe.sample_period = 1;
    pe.read_format = PERF_FORMAT_GROUP; 

    pmsprintf(path, sizeof(path), "%s/events/kvm", tracefs);
    if ((dir = opendir(path)) == NULL)
	return -errno;

    for (cpu = 0; cpu < ncpus; cpu++) {
	flag = 0;
	group_fd[cpu] = -1;
	for (i = 0; i < ntrace; i++) {
	    while ((de = readdir(dir)) != NULL) {
		if (offset == 0)
		    offset = telldir(dir);
		if (strncmp(de->d_name, ".", 1) == 0 ||
		    strcmp(de->d_name, "enable") == 0 ||
		    strcmp(de->d_name, "filter") == 0)
		    continue;
		if (strcmp(de->d_name, trace_nametab[i]) == 0) {
		    pmsprintf(path, sizeof(path), "%s/events/kvm/%s/id", tracefs, de->d_name);
		    if ((pfile = fopen(path, "r")) == NULL)
			continue;
		    memset(temp, 0, sizeof(temp));
		    pe.config = atoi(fgets(temp, sizeof(temp), pfile));
		    fclose(pfile);
		    if ((fd = perf_event_open(&pe, -1, cpu, group_fd[cpu], 0)) < 0) {
			pmNotifyErr(LOG_ERR, "perf_event_open error [trace=%d]", i);
			sts = -errno;
			break;
		    }
		    if (flag == 0) {
			group_fd[cpu] = fd;
			flag = 1;
		    }
		    if (ioctl(fd, PERF_EVENT_IOC_RESET, 0) == -1 ||
			ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) == -1)
			pmNotifyErr(LOG_ERR, "ioctl failed 'PERF_EVENT_IOC_ENABLE'");
		    break;
		}
	    }
	    seekdir(dir, offset);
	}
    }
    closedir(dir);
    return sts;
}
#else
static int
perf_event(int c, int *fds)
{ 
    (void)c; (void)fds;
    return -EOPNOTSUPP;
}
#endif

/*
 * Refresh metrics data
 * pmda		: pmdaExt
 * need_refresh	: check clusters whether to be refresh or not.
 * return	: void
 */
static void
kvm_refresh(pmdaExt *pmda, int *need_refresh)
{
    if (need_refresh[CLUSTER_DEBUG])
	kvm_debug_refresh(&kvmstat);
    if (need_refresh[CLUSTER_TRACE])
	kvm_trace_refresh();
}

/*
 * Callback provided to help pmdaFetch and kvm_fetch.
 * mdesc  : pmdaMetric transferred from pmcd.
 * inst   : instance transferred from pmcd.
 * atom   : return data buffer.
 * return : check whether the fetchCallBack get a valid data or not.
 *	  0 means invalid data.
 *	  1 means valid data
 *	  other return please check pmapi.h
 */
static int
kvm_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    pmID		*idp = (pmID *)&(mdesc->m_desc.pmid);
    unsigned int	cluster = pmID_cluster(*idp);
    unsigned int	item = pmID_item(*idp);
    kvm_trace_value_t	*va;
    char		*name;
    int			sts;

    switch (cluster) {
    case CLUSTER_TRACE:
	if (item == 0) {
	    atom->ull = ntrace;
	    break;
	}
	sts = pmdaCacheLookup(*trace_indom, inst, &name, (void **)&va);
	if (sts != PMDA_CACHE_ACTIVE && sts < 0) {
	    pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s",
			inst, pmErrStr(sts));
	    return PM_ERR_INST;
	}
	if (item > ntrace)
	    return PM_ERR_PMID;
	atom->ull = va[item-1].value;
	break;

    case CLUSTER_DEBUG:
	if (item >= KVM_DEBUG_COUNT)
	    return PM_ERR_PMID;
	atom->ull = kvmstat.value[item];
	break;

    default:
	return PM_ERR_PMID;
    }
    return 1;
}

static int
kvm_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    return pmdaTreePMID(pmns, name, pmid);
}

static int
kvm_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    return pmdaTreeName(pmns, pmid, nameset);
}

static int
kvm_children(const char *name, int traverse, char ***kids, int **sts,
		pmdaExt *pmda)
{
    return pmdaTreeChildren(pmns, name, traverse, kids, sts);
}

static int
kvm_labelCallBack(pmInDom indom, unsigned int inst, pmLabelSet **lp)
{
    if (pmInDom_serial(indom) == TRACE_INDOM)
	return pmdaAddLabels(lp, "{\"cpu\":%u}", inst);
    return 0;
}

static int
kvm_label(int ident, int type, pmLabelSet **lpp, pmdaExt *pmda)
{
    if (type == PM_LABEL_INDOM && pmInDom_serial(ident) == TRACE_INDOM) {
	pmdaAddLabels(lpp, "{\"device_type\":\"cpu\"}");
	pmdaAddLabels(lpp, "{\"indom_name\":\"per cpu\"}");
    }
    return pmdaLabel(ident, type, lpp, pmda);
}

/*
 * Used to call refresh functions and return metric values (or lack thereof)
 * numpmid : metrics' number transferred to pmda.
 * pmidlist: list of metrics.
 * resp    : returned data buffer.
 * pmda    : pmdaExt.
 * return  : always return pmdaFetch(*).
 */
static int
kvm_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int		need_refresh[NUM_CLUSTERS], i;

    memset(need_refresh, 0, sizeof(need_refresh));
    for (i = 0; i < numpmid; i++) {
	pmID *idp = (pmID *)&(pmidlist[i]);
	if (pmID_cluster(*idp) < NUM_CLUSTERS)
	    need_refresh[pmID_cluster(*idp)]++;
    }
    kvm_refresh(pmda, need_refresh);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
kvm_config(void)
{
    char		*p;
    char		buf[BUFSIZ];
    void		*table;
    FILE		*fp;
    enum { STATE_UNKNOWN, STATE_PATHS, STATE_TRACE } state = 0;

    pmsprintf(buf, sizeof(buf), "%s%ckvm%ckvm.conf",
		pmGetOptionalConfig("PCP_PMDAS_DIR"), sep, sep);
    if ((fp = fopen(buf, "rt")) == NULL)
	return -oserror();
    while (fgets(buf, sizeof(buf), fp) != NULL) {
	buf[sizeof(buf)-1] = '\0';
	/* strip whitespace from the end then the start */
	p = buf + strlen(buf) - 1;
	while (p > buf && isspace(*p)) { *p = '\0'; p--; }
	for (p = buf; isspace(*p) && *p != '\0'; p++);
	/* skip empty lines and comments */
	if (*p == '\0' || *p == '#')
	    continue;

	if (strcmp(p, "[paths]") == 0) {
	    state = STATE_PATHS;
	    continue;
	} else if (strcmp(p, "[trace]") == 0 || strcmp(p, "[dynamic]") == 0) {
	    state = STATE_TRACE;
	    continue;
	} else if (*p == '[') {
	    state = STATE_UNKNOWN;	/* ignore unrecognized file sections */
	    continue;
	}

	if (state == STATE_PATHS) {
	    if (sscanf(p, "tracefs=%s", tracefs) != 0)
	        continue;
	    if (sscanf(p, "debugfs=%s", debugfs) != 0)
		continue;
	}
	if (state == STATE_TRACE) {
	    if (!(table = realloc(trace_nametab, (ntrace+1) * sizeof(char*)))) {
		pmNotifyErr(LOG_ERR, "kvm_config OOM (%d)", ntrace);
		continue;
	    }
	    trace_nametab = (char **)table;
	    if ((trace_nametab[ntrace] = strdup(p)) == NULL) {
		pmNotifyErr(LOG_ERR, "kvm_config tracepoint OOM");
		continue;
	    }
	    ntrace++;
	}
    }
    fclose(fp);
    return 0;
}

/*
 * Used to initialize the KVM pmda.
 * Set interface function (fetch, store, instance etc.), sets instance domain,
 * and runs the pmdaInit function.
 */
void
__PMDA_INIT_CALL
kvm_init(pmdaInterface *dp)
{
    pmdaMetric		*pmetric;
    size_t		nmetrics, nindoms, tmetrics;
    char		name[MAXPATHLEN];
    char		*envpath;
    int			m = 0, sts;

    if (_isDSO) {
	sep = pmPathSeparator();
	pmsprintf(helppath, sizeof(helppath), "%s%c" "kvm" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_7, "KVM DSO", helppath);
    } else {
        if (username)
            pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    /* get paths and any specified KVM trace metrics */ 
    kvm_config();

    nindoms = sizeof(indomtab)/sizeof(indomtab[0]);
    nmetrics = sizeof(metrictab)/sizeof(metrictab[0]);
    tmetrics = nmetrics + ntrace;

    /* allocate memory for tmetrictab if needed */
    if (ntrace) {
	if ((tmetrictab = calloc(tmetrics, sizeof(pmdaMetric))) != NULL) {
	    memcpy(tmetrictab, metrictab, sizeof(metrictab));
	    pmetric = &tmetrictab[nmetrics];
	    for (m = 0; m < ntrace; m++, pmetric++) {
		pmetric->m_user = NULL; 
		pmetric->m_desc.pmid = PMDA_PMID(CLUSTER_TRACE, m + 1);
		pmetric->m_desc.type = PM_TYPE_64;
		pmetric->m_desc.indom = TRACE_INDOM;
		pmetric->m_desc.sem = PM_SEM_INSTANT;
		memset(&pmetric->m_desc.units, 0, sizeof(pmUnits));
	    }
	} else {
	    pmNotifyErr(LOG_ERR, "%s: kvm_init OOM, using only static metrics",
		    pmGetProgname());
	}
    }
    if (tmetrictab == NULL) {
	tmetrictab = metrictab;
	tmetrics = nmetrics;
    }

    if ((envpath = getenv("KVM_NCPUS")))
	ncpus = atoi(envpath);
    else
	ncpus = sysconf(_SC_NPROCESSORS_CONF);
    if (ncpus <= 0)
	ncpus = 1;
    if ((envpath = getenv("KVM_DEBUGFS_PATH")))
	pmsprintf(debugfs, sizeof(debugfs), "%s", envpath);
    else
	pmsprintf(debugfs, sizeof(debugfs), "/sys/kernel/debug");
    if ((envpath = getenv("KVM_TRACEFS_PATH")))
	pmsprintf(tracefs, sizeof(tracefs), "%s", envpath);
    else
	pmsprintf(tracefs, sizeof(tracefs), "/sys/kernel/debug/tracing");

    if (tmetrictab != metrictab) {
	group_fd = malloc(ncpus * sizeof(int));
	if ((sts = perf_event(ncpus, group_fd)) < 0) {
	    pmNotifyErr(LOG_INFO, "disabling perf_event support: %s",
			pmErrStr(sts));
	    free(group_fd);
	    group_fd = NULL;
	}
    }

    dp->version.seven.fetch = kvm_fetch;
    dp->version.seven.label = kvm_label;
    dp->version.seven.pmid = kvm_pmid;
    dp->version.seven.name = kvm_name;
    dp->version.seven.children = kvm_children;
    pmdaSetFetchCallBack(dp, kvm_fetchCallBack);
    pmdaSetLabelCallBack(dp, kvm_labelCallBack);

    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, indomtab, nindoms, tmetrictab, tmetrics);

    /* Create the dynamic PMNS tree and populate it. */
    if ((sts = pmdaTreeCreate(&pmns)) < 0) {
	pmNotifyErr(LOG_ERR, "failed to create new PMNS: %s\n",
			pmErrStr(sts));
	dp->status = sts;
	pmns = NULL;
    } else {
	pmetric = &tmetrictab[0];
	for (m = 0; m < nmetrics; m++) {
	    pmsprintf(name, sizeof(name), "kvm.%s", (char *)pmetric[m].m_user);
	    pmdaTreeInsert(pmns, pmetric[m].m_desc.pmid, name);
	}
	pmetric = &tmetrictab[nmetrics];
	for (m = 0; m < ntrace; m++) {
	    pmsprintf(name, sizeof(name), "kvm.trace.%s", trace_nametab[m]);
	    pmdaTreeInsert(pmns, pmetric[m].m_desc.pmid, name);
	}
	/* for reverse (pmid->name) lookups */
	pmdaTreeRebuildHash(pmns, ntrace);
    }
}

int
main(int argc, char **argv)
{
    pmdaInterface	dispatch;

    _isDSO = 0;
    pmSetProgname(argv[0]);

    sep = pmPathSeparator();
    pmsprintf(helppath, sizeof(helppath), "%s%c" "kvm" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), KVM, "kvm.log", helppath);

    pmdaGetOptions(argc, argv,  &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&dispatch);
    kvm_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
