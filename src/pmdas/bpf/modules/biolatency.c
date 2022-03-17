#include "module.h"
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <pcp/pmda.h>
#include "biolatency.skel.h"

#define NUM_LATENCY_SLOTS 63
pmdaInstid biolatency_instances[NUM_LATENCY_SLOTS];

struct biolatency_bpf *bpf_obj;
int biolatency_fd = -1;
#define INDOM_COUNT 1
#define BIOLATENCY_INDOM 0
unsigned int indom_id_mapping[INDOM_COUNT];

#define METRIC_COUNT 1
char* metric_names[METRIC_COUNT] = {
	"disk.all.latency"
};

char* metric_text_oneline[METRIC_COUNT] = {
    "Disk latency"
};
char* metric_text_long[METRIC_COUNT] = {
    "Disk latency histogram across all disks, for both reads and writes.\n"
};

unsigned int biolatency_metric_count()
{
    return METRIC_COUNT;
}

char* biolatency_metric_name(unsigned int metric)
{
	return metric_names[metric];
}

unsigned int biolatency_indom_count()
{
    return INDOM_COUNT;
}

void biolatency_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

int biolatency_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}

void biolatency_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    // must match PMNS

    /* bpf.disk.all.latency */
    metrics[0] = (struct pmdaMetric)
        { /* m_user */ NULL,
            { /* m_desc */
                PMDA_PMID(cluster_id, 0),
                PM_TYPE_U64,
                indom_id_mapping[BIOLATENCY_INDOM],
                PM_SEM_COUNTER,
                PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
            }
        };

    indoms[0] = (struct pmdaIndom)
        {
            indom_id_mapping[BIOLATENCY_INDOM],
            sizeof(biolatency_instances)/sizeof(pmdaIndom),
            biolatency_instances
        };
}

int biolatency_init(dict *cfg, char *module_name)
{
    char errorstring[1024];
    int ret;

    bpf_obj = biolatency_bpf__open();
    pmNotifyErr(LOG_INFO, "booting: %s", bpf_obj->skeleton->name);

    ret = biolatency_bpf__load(bpf_obj);
    if (ret == 0) {
        pmNotifyErr(LOG_INFO, "bpf loaded");
    } else {
        libbpf_strerror(ret, errorstring, 1023);
        pmNotifyErr(LOG_ERR, "bpf load failed: %d, %s", ret, errorstring);
        return ret;
    }

    pmNotifyErr(LOG_INFO, "attaching bpf programs");
    biolatency_bpf__attach(bpf_obj);
    pmNotifyErr(LOG_INFO, "attached!");

    biolatency_fd = bpf_map__fd(bpf_obj->maps.hist);
    if (biolatency_fd >= 0) {
        pmNotifyErr(LOG_INFO, "opened latencies map, fd: %d", biolatency_fd);
    } else {
        libbpf_strerror(biolatency_fd, errorstring, 1023);
        pmNotifyErr(LOG_ERR, "bpf map open failed: %d, %s", biolatency_fd, errorstring);
        return biolatency_fd;
    }

    fill_instids_log2(NUM_LATENCY_SLOTS, biolatency_instances);

    return 0;
}

void biolatency_shutdown()
{
    if (biolatency_fd != 0) {
        close(biolatency_fd);
        biolatency_fd = -1;
    }
    if (bpf_obj) {
        biolatency_bpf__destroy(bpf_obj);
    }
}

void biolatency_refresh(unsigned int item)
{
    /* do nothing */
}

int biolatency_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    if (inst == PM_IN_NULL) {
        return PM_ERR_INST;
    }

    if (biolatency_fd == -1) {
        // not initialised
        return PMDA_FETCH_NOVALUES;
    }

    unsigned long key = inst;
    unsigned long value = 0;
    int ret = bpf_map_lookup_elem(biolatency_fd, &key, &value);
    if (ret == -1) {
        return PMDA_FETCH_NOVALUES;
    }

    atom->ull = value;
    return PMDA_FETCH_STATIC;
}

struct module bpf_module = {
    .init               = biolatency_init,
    .register_metrics   = biolatency_register,
    .metric_count       = biolatency_metric_count,
    .indom_count        = biolatency_indom_count,
    .set_indom_serial   = biolatency_set_indom_serial,
    .shutdown           = biolatency_shutdown,
    .refresh            = biolatency_refresh,
    .fetch_to_atom      = biolatency_fetch_to_atom,
    .metric_name        = biolatency_metric_name,
    .metric_text        = biolatency_metric_text,
};
