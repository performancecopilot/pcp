#include "module.h"
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <pcp/pmda.h>

#define NUM_LATENCY_SLOTS 63
pmdaInstid biolatency_instances[NUM_LATENCY_SLOTS];

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
    struct bpf_object *bpf_obj;
    char errorstring[1024];
    struct bpf_program *bpfprg;
    const char *name;
    int ret;
    char *bpf_path;

    ret = asprintf(&bpf_path, "%s/bpf/modules/biolatency.bpf.o", pmGetConfig("PCP_PMDAS_DIR"));
    if (ret <= 0) {
        pmNotifyErr(LOG_ERR, "could not construct bpf module path");
        return ret;
    }

    bpf_obj = bpf_object__open(bpf_path);
    free(bpf_path);
    name = bpf_object__name(bpf_obj);
    pmNotifyErr(LOG_INFO, "booting: %s", name);

    ret = bpf_object__load(bpf_obj);
    if (ret == 0) {
        pmNotifyErr(LOG_INFO, "bpf loaded");
    } else {
        libbpf_strerror(ret, errorstring, 1023);
        pmNotifyErr(LOG_ERR, "bpf load failed: %d, %s", ret, errorstring);
        return ret;
    }

    pmNotifyErr(LOG_INFO, "attaching bpf programs");
    bpfprg = bpf_program__next(NULL, bpf_obj);
    while (bpfprg != NULL)
    {
        bpf_program__attach(bpfprg);
        bpfprg = bpf_program__next(bpfprg, bpf_obj);
    }
    pmNotifyErr(LOG_INFO, "attached!");

    biolatency_fd = bpf_object__find_map_fd_by_name(bpf_obj, "hist");
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
