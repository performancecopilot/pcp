/*
 * Based on netatop-bpf:
 * https://github.com/bytedance/netatop-bpf
 */

#include "module.h"

#include <errno.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <pcp/pmwebapi.h>

#include "netatop.skel.h"
#include "netatop.h"
#include "btf_helpers.h"

#define INDOM_COUNT 1
#define MAX_PORTS 64

static struct env {
    int process_count;
} env = {
    .process_count = 20,
};

int tgid_map_fd;
int nr_cpus;

static pmdaInstid *netatop_instances;
// /usr/src/kernels/.M.m-b-r.kn.x86_64/tools/lib/bpf/libbpf.c
static struct netatop_bpf *obj;
static char **instid_strings;
static pmdaIndom *netatop_indoms;
static unsigned int indom_id_mapping[INDOM_COUNT];


#define METRIC_COUNT 8
enum metric_name { TCPSNDPACKS, TCPSNDBYTES, TCPRCVPACKS, TCPRCVBYTES, 
		   UDPSNDPACKS, UDPSNDBYTES, UDPRCVPACKS, UDPRCVBYTES };
enum metric_indom { NETATOP_INDOM };


char* metric_names[METRIC_COUNT] = {
    [TCPSNDPACKS] = "proc.net.tcp.send.packets",
    [TCPSNDBYTES] = "proc.net.tcp.send.bytes",
    [TCPRCVPACKS] = "proc.net.tcp.recv.packets",
    [TCPRCVBYTES] = "proc.net.tcp.recv.bytes",
    [UDPSNDPACKS] = "proc.net.udp.send.packets",
    [UDPSNDBYTES] = "proc.net.udp.send.bytes",
    [UDPRCVPACKS] = "proc.net.udp.recv.packets",
    [UDPRCVBYTES] = "proc.net.udp.recv.bytes",
};

char* metric_text_oneline[METRIC_COUNT] = {
    [TCPSNDPACKS] = "tcp packets sent",
    [TCPSNDBYTES] = "tcp bytes sent",
    [TCPRCVPACKS] = "tcp packets received",
    [TCPRCVBYTES] = "tcp bytes received",
    [UDPSNDPACKS] = "udp packets sent",
    [UDPSNDBYTES] = "udp bytes sent",
    [UDPRCVPACKS] = "udp packets received",
    [UDPRCVBYTES] = "udp bytes received"
};

char* metric_text_long[METRIC_COUNT] = {
    [TCPSNDPACKS] = "tcp packets sent (tracepoint/sock/sock_send_length)",
    [TCPSNDBYTES] = "tcp bytes sent (tracepoint/sock/sock_send_length)",
    [TCPRCVPACKS] = "tcp packets received (tracepoint/sock/sock_recv_length)",
    [TCPRCVBYTES] = "tcp bytes received (tracepoint/sock/sock_recv_length)",
    [UDPSNDPACKS] = "udp packets sent (tracepoint/sock/sock_send_length)",
    [UDPSNDBYTES] = "udp bytes sent (tracepoint/sock/sock_send_length)",
    [UDPRCVPACKS] = "udp packets received (tracepoint/sock/sock_recv_length)",
    [UDPRCVBYTES] = "udp bytes received (tracepoint/sock/sock_recv_length)"
};

static unsigned int netatop_metric_count(void)
{
    return METRIC_COUNT;
}

static char* netatop_metric_name(unsigned int metric)
{
    return metric_names[metric];
}

static unsigned int netatop_indom_count(void)
{
    return INDOM_COUNT;
}

static void netatop_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

static int netatop_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}


static void netatop_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    /* bpf.netatop.tcpsndpacks */
    metrics[TCPSNDPACKS] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 0),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[NETATOP_INDOM],
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };

    /* bpf.netatop.tcpsndpacks */
    metrics[TCPSNDBYTES] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 1),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[NETATOP_INDOM],
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.netatop.tcpsndpacks */
    metrics[TCPRCVPACKS] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 2),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[NETATOP_INDOM],
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.netatop.tcpsndpacks */
    metrics[TCPRCVBYTES] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 3),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[NETATOP_INDOM],
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.netatop.tcpsndpacks */
    metrics[UDPSNDPACKS] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 4),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[NETATOP_INDOM],
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.netatop.tcpsndpacks */
    metrics[UDPSNDBYTES] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 5),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[NETATOP_INDOM],
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.netatop.tcpsndpacks */
    metrics[UDPRCVPACKS] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 6),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[NETATOP_INDOM],
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.netatop.tcpsndpacks */
    metrics[UDPRCVBYTES] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 7),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[NETATOP_INDOM],
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };

    indoms[NETATOP_INDOM] = (struct pmdaIndom)
    {
        indom_id_mapping[NETATOP_INDOM],
        env.process_count,
        netatop_instances,
    };

    netatop_indoms = indoms;
}


/* Allocate instance id slots and give them an initial value */

static void netatop_fill_instids(unsigned int slot_count, pmdaInstid **slots) {
     if (*slots == NULL 
	   && ((*slots = malloc((slot_count + 1) * sizeof(pmdaInstid))) == NULL
	     || (instid_strings = malloc((env.process_count + 1) * sizeof(void*))) == NULL)) {
        pmNotifyErr(LOG_ERR, "pmdaInstid: realloc err: %d", PM_FATAL_ERR);
        exit(1);
    }

    for (int i = 0; i <= slot_count; i++) {
	 char *string;
	 if (asprintf(&string, "%d", 0-i) > 0)
	      instid_strings[i] = string;
	(*slots)[i].i_inst = 0-i;
	(*slots)[i].i_name = instid_strings[i];
    }
}

/*  Set the first unused instance id to tid */

static 
bool 
netatop_add_instids(unsigned int slot_count, pid_t tid, pmdaInstid **slots)
{
    for (int i = 0; i <= slot_count; i++) {
	 char *string;

	if ((*slots)[i].i_inst == tid)
	    return false;
	else if ((*slots)[i].i_inst <= 0) {
	     if (asprintf(&string, "%d", tid) > 0) {
		  (*slots)[i].i_name = string;
                  (*slots)[i].i_inst = tid;
		  return true;
	     }
	}
    }
    return false;
}


/*  Fill initial instances, do bpf bookkeeping, get bpf map */

int netatop_init(dict *cfg, char *module_name)
{
    LIBBPF_OPTS(bpf_object_open_opts, open_opts);
    int err = 0;
    char *val;

    if ((val = pmIniFileLookup(cfg, module_name, "process_count")))
        env.process_count = atoi(val);

    err = ensure_core_btf(&open_opts);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to fetch necessary BTF for CO-RE: %s", strerror(-err));
        return 1;
    }

    obj = netatop_bpf__open_opts(&open_opts);
    if (!obj) {
        pmNotifyErr(LOG_ERR, "failed to open BPF object");
        return 1;
    }

    obj = netatop_bpf__open_and_load();
    if (!obj) {
	pmNotifyErr(LOG_ERR, "Failed to open BPF object\n");
	return 1;
    }

    err = netatop_bpf__attach(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to attach BPF programs: %s", strerror(-err));
        return true;
    }
    
    /* internal/external instance ids */
    netatop_fill_instids(env.process_count, &netatop_instances);

    tgid_map_fd = bpf_object__find_map_fd_by_name(obj->obj, "tgid_net_stat");
    if (tgid_map_fd < 0) {
        pmNotifyErr(LOG_ERR, "failed to get map fd: %s", strerror(errno));
        return true;
    }
    return err;
}


static void netatop_shutdown()
{
    free(netatop_instances);

    netatop_bpf__destroy(obj);
}

/* Set instance ids to tids present in bpf map */

static void netatop_refresh(unsigned int item)
{
     unsigned long cur_key = 1;

    if (item == 0) {
	 int map_entry_n = 0;
	 netatop_fill_instids(env.process_count, &netatop_instances);
         for (int ret = bpf_map_get_next_key(tgid_map_fd, NULL, &cur_key);
              ret == 0;
              ret = bpf_map_get_next_key (tgid_map_fd, &cur_key, &cur_key)) {
//	     if (kill(cur_key, 0) && errno == ESRCH) 
//		  bpf_map_delete_elem(tgid_map_fd, &cur_key);
	     netatop_add_instids (env.process_count, cur_key, &netatop_instances);
	     map_entry_n += 1;
	 }
	 netatop_indoms[NETATOP_INDOM].it_numinst = map_entry_n;
    }
}

/* Get network metrics from bpf for a particular metric for a particular tid */

static int netatop_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    nr_cpus = libbpf_num_possible_cpus();
    struct taskcount *value = (struct taskcount*)calloc(nr_cpus, sizeof(struct taskcount));
    unsigned long next_key = inst;

    if (bpf_map_lookup_elem(tgid_map_fd, &next_key, value) == 0) {
	struct taskcount data = {
	    .tcpsndpacks = 0,
	    .tcpsndbytes = 0,
	    .tcprcvpacks = 0,
	    .tcprcvbytes = 0,
	    .udpsndpacks = 0,
	    .udpsndbytes = 0,
	    .udprcvpacks = 0,
	    .udprcvbytes = 0,
	};

        for (int i = 0; i < nr_cpus; i++) {
            data.tcpsndpacks += value[i].tcpsndpacks;
            data.tcpsndbytes += value[i].tcpsndbytes;
            data.tcprcvpacks += value[i].tcprcvpacks;
            data.tcprcvbytes += value[i].tcprcvbytes;
            data.udpsndpacks += value[i].udpsndpacks;
            data.udpsndbytes += value[i].udpsndbytes;
            data.udprcvpacks += value[i].udprcvpacks;
            data.udprcvbytes += value[i].udprcvbytes;
        } 
	netatop_add_instids(env.process_count, next_key, &netatop_instances);
    }

    long netvalue = 0;
    switch (item) {
    case TCPSNDPACKS:
        for (int i = 0; i < nr_cpus; i++) {
            netvalue += value[i].tcpsndpacks;
        } 
        atom->ull = netvalue;
	break;
    case TCPSNDBYTES:
	for (int i = 0; i < nr_cpus; i++) {
	    netvalue += value[i].tcpsndbytes;
	}
	atom->ull = netvalue;
	break;
    case TCPRCVPACKS:
	for (int i = 0; i < nr_cpus; i++) {
	    netvalue += value[i].tcprcvpacks;
	}
	atom->ull = netvalue;
	break;
    case TCPRCVBYTES:
	for (int i = 0; i < nr_cpus; i++) {
	    netvalue += value[i].tcprcvbytes;
	}
	atom->ull = netvalue;
	break;
    case UDPSNDPACKS:
	for (int i = 0; i < nr_cpus; i++) {
	    netvalue += value[i].udpsndpacks;
	}
	atom->ull = netvalue;
	break;
    case UDPSNDBYTES:
	for (int i = 0; i < nr_cpus; i++) {
	    netvalue += value[i].udpsndbytes;
	}
	atom->ull = netvalue;
	break;
    case UDPRCVPACKS:
	for (int i = 0; i < nr_cpus; i++) {
	    netvalue += value[i].udprcvpacks;
	}
	atom->ull = netvalue;
	break;
    case UDPRCVBYTES:
	for (int i = 0; i < nr_cpus; i++) {
	    netvalue += value[i].udprcvbytes;
	}
	atom->ull = netvalue;
	break;
    }

    return PMDA_FETCH_STATIC;
}

struct module bpf_module = {
    .init               = netatop_init,
    .register_metrics   = netatop_register,
    .metric_count       = netatop_metric_count,
    .indom_count        = netatop_indom_count,
    .set_indom_serial   = netatop_set_indom_serial,
    .shutdown           = netatop_shutdown,
    .refresh            = netatop_refresh,
    .fetch_to_atom      = netatop_fetch_to_atom,
    .metric_name        = netatop_metric_name,
    .metric_text        = netatop_metric_text,
};
