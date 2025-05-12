/*
 *
 * Copyright (c) 2022 Sohaib Mohamed <sohaib.amhmd@gmail.com>
 *
 * Based on the tcpconnect(8):
 * https://github.com/iovisor/bcc/blob/master/libbpf-tools/tcpconnect.c
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

#include "module.h"

#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <pcp/pmda.h>
#include <sys/queue.h>
#include <pcp/pmwebapi.h>
#include <string.h>

#include "tcpconnect.h"
#include "tcpconnect.skel.h"
#include "btf_helpers.h"

#define PERF_BUFFER_PAGES 128
#define PERF_POLL_TIMEOUT_MS 0
#define INDOM_COUNT 1

static struct env {
    bool print_uid;
    pid_t pid;
    uid_t uid;
    int nports;
    int ports[MAX_PORTS];
    int process_count;
} env = {
    .uid = (uid_t) -1,
    .process_count = 20,
};

static pmdaInstid *tcpconnect_instances;
static struct tcpconnect_bpf *obj;
static struct perf_buffer *pb = NULL;
static int lost_events;
static int queuelength;

/* cache array */
struct tailq_entry {
    struct event event;
    TAILQ_ENTRY(tailq_entry) entries;
};

TAILQ_HEAD(tailhead, tailq_entry) head;

static struct tailq_entry* allocElm(void)
{
    return malloc(sizeof(struct tailq_entry));
}

static void push(struct tailq_entry *elm)
{
    TAILQ_INSERT_TAIL(&head, elm, entries);
    if (queuelength > env.process_count)
    {
        struct tailq_entry *l;
        l = head.tqh_first;
        TAILQ_REMOVE(&head, l, entries);
        free(l);
        queuelength--;
    }
    queuelength++;
}

static bool get_item(unsigned int offset, struct tailq_entry** val)
{
    struct tailq_entry *i;
    unsigned int iter = 0;

    TAILQ_FOREACH_REVERSE(i, &head, tailhead, entries) {
        if (offset == iter) {
            *val = i;
            return true;
        }
        iter++;
    }
    return false;
}

static unsigned int indom_id_mapping[INDOM_COUNT];

#define METRIC_COUNT 8
enum metric_name { PID, COMM, IPVERSION, SADDR, DADDR, DPORT, UID, LOST };
enum metric_indom { TCPCONNECT_INDOM };

char* metric_names[METRIC_COUNT] = {
    [PID]        =  "tcpconnect.pid",
    [COMM]       =  "tcpconnect.comm",
    [IPVERSION]  =  "tcpconnect.ipversion",
    [SADDR]      =  "tcpconnect.saddr",
    [DADDR]      =  "tcpconnect.daddr",
    [DPORT]      =  "tcpconnect.dport",
    [UID]        =  "tcpconnect.uid",
    [LOST]       =  "tcpconnect.lost",
};

char* metric_text_oneline[METRIC_COUNT] = {
    [PID]        =  "Process ID",
    [COMM]       =  "Command name",
    [IPVERSION]  =  "IP version 4 or 6",
    [SADDR]      =  "Source IP address.",
    [DADDR]      =  "Destination IP address.",
    [DPORT]      =  "Destination port.",
    [UID]        =  "User ID",
    [LOST]       =  "Number of the lost events",
};

char* metric_text_long[METRIC_COUNT] = {
    [PID]        =  "Process ID",
    [COMM]       =  "Command name",
    [IPVERSION]  =  "IP version 4 or 6",
    [SADDR]      =  "Source IP address.",
    [DADDR]      =  "Destination IP address.",
    [DPORT]      =  "Source port.",
    [UID]        =  "User ID",
    [LOST]       =  "Number of the lost events",
};

static unsigned int tcpconnect_metric_count(void)
{
    return METRIC_COUNT;
}

static char* tcpconnect_metric_name(unsigned int metric)
{
    return metric_names[metric];
}

static unsigned int tcpconnect_indom_count(void)
{
    return INDOM_COUNT;
}

static void tcpconnect_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

static int tcpconnect_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}

static void tcpconnect_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    /* bpf.tcpconnlat.pid */
    metrics[PID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 0),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[TCPCONNECT_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.tcpconnlat.comm */
    metrics[COMM] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 1),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[TCPCONNECT_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.tcpconnlat.ipversion */
    metrics[IPVERSION] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 2),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[TCPCONNECT_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.tcpconnlat.saddr */
    metrics[SADDR] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 3),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[TCPCONNECT_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.tcpconnlat.daddr */
    metrics[DADDR] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 4),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[TCPCONNECT_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.tcpconnlat.dport */
    metrics[DPORT] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 5),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[TCPCONNECT_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.tcpconnect.uid */
    metrics[UID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 6),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[TCPCONNECT_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.tcpconnect.lost */
    metrics[LOST] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 7),
            .type  = PM_TYPE_U32,
            .indom = PM_INDOM_NULL,
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
        }
    };

    /* TCPCONNECT_INDOM */
    indoms[TCPCONNECT_INDOM] = (struct pmdaIndom)
    {
        indom_id_mapping[TCPCONNECT_INDOM],
        env.process_count,
        tcpconnect_instances,
    };
}

static void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
    struct event *event = data;
    struct tailq_entry *elm = allocElm();

    elm->event.pid = event->pid;
    elm->event.af = event->af;
    elm->event.uid = event->uid;
    elm->event.dport = event->dport;
    elm->event.saddr_v4 = event->saddr_v4;
    elm->event.daddr_v4 = event->daddr_v4;
    pmstrncpy(elm->event.task, sizeof(elm->event.task), event->task);

    push(elm);
}

static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    lost_events += lost_cnt;
}

static int tcpconnect_init(dict *cfg, char *module_name)
{
    LIBBPF_OPTS(bpf_object_open_opts, open_opts);
    int i, err;
    char *val;

    if ((val = pmIniFileLookup(cfg, module_name, "process_count")))
        env.process_count = atoi(val);
    if ((val = pmIniFileLookup(cfg, module_name, "pid")))
        env.pid = atoi(val);
    if ((val = pmIniFileLookup(cfg, module_name, "uid")))
        env.uid = atoi(val);
    if ((val = pmIniFileLookup(cfg, module_name, "port")))
        env.nports = atoi(val);

    err = ensure_core_btf(&open_opts);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to fetch necessary BTF for CO-RE: %s", strerror(-err));
        return 1;
    }

    obj = tcpconnect_bpf__open_opts(&open_opts);
    if (!obj) {
        pmNotifyErr(LOG_ERR, "failed to open BPF object");
        return 1;
    }

    if (env.pid)
        obj->rodata->filter_pid = env.pid;
    if (env.uid != (uid_t) -1)
        obj->rodata->filter_uid = env.uid;
    if (env.nports > 0) {
        obj->rodata->filter_ports_len = env.nports;
        for (i = 0; i < env.nports; i++) {
            obj->rodata->filter_ports[i] = htons(env.ports[i]);
        }
    }

    err = tcpconnect_bpf__load(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to load BPF object: %d", err);
        return err != 0;
    }

    err = tcpconnect_bpf__attach(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to attach BPF programs: %s", strerror(-err));
        return err != 0;
    }

    /* internal/external instance ids */
    fill_instids(env.process_count, &tcpconnect_instances);

    /* Initialize the tail queue. */
    TAILQ_INIT(&head);

    pb = perf_buffer__new(bpf_map__fd(obj->maps.events), PERF_BUFFER_PAGES,
            handle_event, handle_lost_events, NULL, NULL);
    if (!pb) {
        err = -errno;
        pmNotifyErr(LOG_ERR, "failed to open perf buffer: %d\n", err);
        return err != 0;
    }

    return err != 0;
}

static void tcpconnect_shutdown()
{
    struct tailq_entry *itemp;

    free(tcpconnect_instances);
    perf_buffer__free(pb);
    tcpconnect_bpf__destroy(obj);
    /* Free the entire cache queue. */
    while ((itemp = TAILQ_FIRST(&head))) {
        TAILQ_REMOVE(&head, itemp, entries);
        free(itemp);
    }
}

static void tcpconnect_refresh(unsigned int item)
{
    perf_buffer__poll(pb, PERF_POLL_TIMEOUT_MS);
}

static int tcpconnect_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    struct tailq_entry *value;
    char src[INET6_ADDRSTRLEN];
    char dst[INET6_ADDRSTRLEN];
    union {
        struct in_addr  x4;
        struct in6_addr x6;
    } s, d;

    /* bpf.tcpconnect.lost */
    if (item == LOST) {
        atom->ul = lost_events;
        return PMDA_FETCH_STATIC;
    }

    if (inst == PM_IN_NULL) {
        return PM_ERR_INST;
    }

    if(!get_item(inst, &value))
        return PMDA_FETCH_NOVALUES;

    if (value->event.af == AF_INET) {
        s.x4.s_addr = value->event.saddr_v4;
        d.x4.s_addr = value->event.daddr_v4;
    } else if (value->event.af == AF_INET6) {
        memcpy(&s.x6.s6_addr, value->event.saddr_v6, sizeof(s.x6.s6_addr));
        memcpy(&d.x6.s6_addr, value->event.daddr_v6, sizeof(d.x6.s6_addr));
    } else {
        pmNotifyErr(LOG_ERR, "broken event: event->af=%d", value->event.af);
        return PMDA_FETCH_NOVALUES;
    }

    /* bpf.tcpconnect.pid */
    if (item == PID) {
        atom->ul = value->event.pid;
    }
    /* bpf.tcpconnect.comm */
    if (item == COMM) {
        atom->cp = value->event.task;
    }
    /* bpf.tcpconnect.ipversion */
    if (item == IPVERSION) {
        atom->ul = value->event.af == AF_INET ? 4 : 6;
    }
    /* bpf.tcpconnect.saddr */
    if (item == SADDR) {
        atom->cp = strdup((char*)inet_ntop(value->event.af, &s, src, sizeof(src)));
        return PMDA_FETCH_DYNAMIC;
    }
    /* bpf.tcpconnect.daddr */
    if (item == DADDR) {
        atom->cp = strdup((char*)inet_ntop(value->event.af, &d, dst, sizeof(dst)));
        return PMDA_FETCH_DYNAMIC;
    }
    /* bpf.tcpconnect.dport */
    if (item == DPORT) {
        atom->ul = ntohs(value->event.dport);
    }
    /* bpf.tcpconnect.uid */
    if (item == UID) {
        atom->ul = value->event.uid;
    }

    return PMDA_FETCH_STATIC;
}

struct module bpf_module = {
    .init               = tcpconnect_init,
    .register_metrics   = tcpconnect_register,
    .metric_count       = tcpconnect_metric_count,
    .indom_count        = tcpconnect_indom_count,
    .set_indom_serial   = tcpconnect_set_indom_serial,
    .shutdown           = tcpconnect_shutdown,
    .refresh            = tcpconnect_refresh,
    .fetch_to_atom      = tcpconnect_fetch_to_atom,
    .metric_name        = tcpconnect_metric_name,
    .metric_text        = tcpconnect_metric_text,
};
