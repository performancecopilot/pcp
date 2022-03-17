#include "module.h"
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <pcp/pmda.h>
#include "runqlat.skel.h"

#define NUM_LATENCY_SLOTS 63
pmdaInstid runqlat_instances[NUM_LATENCY_SLOTS];

struct runqlat_bpf *bpf_obj;
int runqlat_fd = -1;
#define INDOM_COUNT 1
#define RUNQLAT_INDOM 0
unsigned int indom_id_mapping[INDOM_COUNT];

#define METRIC_COUNT 1
char* metric_names[METRIC_COUNT] = {
    "runq.latency"
};

char* metric_text_oneline[METRIC_COUNT] = {
    "Run queue latency (ns)"
};
char* metric_text_long[METRIC_COUNT] = {
    "Run queue latency from task switches,\nie: how long each task sat in queue from entry to queue until executing.\n"
};

unsigned int runqlat_metric_count()
{
    return 1;
}

char* runqlat_metric_name(unsigned int metric)
{
    return metric_names[metric];
}

unsigned int runqlat_indom_count()
{
    return INDOM_COUNT;
}

void runqlat_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

int runqlat_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}

void runqlat_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    // must match PMNS

    /* bpf.runq.latency */
    metrics[0] = (struct pmdaMetric)
        { /* m_user */ NULL,
            { /* m_desc */
                PMDA_PMID(cluster_id, 0),
                PM_TYPE_U64,
                indom_id_mapping[RUNQLAT_INDOM],
                PM_SEM_COUNTER,
                PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0)
            }
        };

    indoms[0] = (struct pmdaIndom)
        {
            indom_id_mapping[RUNQLAT_INDOM],
            sizeof(runqlat_instances)/sizeof(pmdaIndom),
            runqlat_instances
        };
}

int runqlat_init(dict *cfg, char *module_name)
{
    char errorstring[1024];
    int ret;

    bpf_obj = runqlat_bpf__open();
    pmNotifyErr(LOG_INFO, "booting: %s", bpf_obj->skeleton->name);

    ret = runqlat_bpf__load(bpf_obj);
    if (ret == 0) {
        pmNotifyErr(LOG_INFO, "bpf loaded");
    } else {
        libbpf_strerror(ret, errorstring, 1023);
        pmNotifyErr(LOG_ERR, "bpf load failed: %d, %s", ret, errorstring);
        return ret;
    }

    pmNotifyErr(LOG_INFO, "attaching bpf programs");
    runqlat_bpf__attach(bpf_obj);
    pmNotifyErr(LOG_INFO, "attached!");

    runqlat_fd = bpf_map__fd(bpf_obj->maps.hist);
    if (runqlat_fd >= 0) {
        pmNotifyErr(LOG_INFO, "opened hist map, fd: %d", runqlat_fd);
    } else {
        libbpf_strerror(runqlat_fd, errorstring, 1023);
        pmNotifyErr(LOG_ERR, "bpf map open failed: %d, %s", runqlat_fd, errorstring);
        return runqlat_fd;
    }

    fill_instids_log2(NUM_LATENCY_SLOTS, runqlat_instances);

    return 0;
}

void runqlat_shutdown()
{
    if (runqlat_fd != 0) {
        close(runqlat_fd);
        runqlat_fd = -1;
    }
    if (bpf_obj) {
        runqlat_bpf__destroy(bpf_obj);
    }
}

void runqlat_refresh(unsigned int item)
{
    /* do nothing */
}

int runqlat_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    if (inst == PM_IN_NULL) {
        return PM_ERR_INST;
    }

    if (runqlat_fd == -1) {
        // not initialised
        return PMDA_FETCH_NOVALUES;
    }

    unsigned long key = inst;
    unsigned long value = 0;
    int ret = bpf_map_lookup_elem(runqlat_fd, &key, &value);
    if (ret == -1) {
        return PMDA_FETCH_NOVALUES;
    }

    atom->ull = value;
    return PMDA_FETCH_STATIC;
}

struct module bpf_module = {
    .init               = runqlat_init,
    .register_metrics   = runqlat_register,
    .metric_count       = runqlat_metric_count,
    .indom_count        = runqlat_indom_count,
    .set_indom_serial   = runqlat_set_indom_serial,
    .shutdown           = runqlat_shutdown,
    .refresh            = runqlat_refresh,
    .fetch_to_atom      = runqlat_fetch_to_atom,
    .metric_name        = runqlat_metric_name,
    .metric_text        = runqlat_metric_text,
};
