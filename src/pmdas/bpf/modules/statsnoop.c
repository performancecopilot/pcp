/*
 *
 * Copyright (c) 2022 Sohaib Mohamed <sohaib.amhmd@gmail.com>
 *
 * Based on the statsnoop(8):
 * https://github.com/iovisor/bcc/blob/master/libbpf-tools/statsnoop.c
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

#include "statsnoop.h"
#include "statsnoop.skel.h"
#include "btf_helpers.h"

#define PERF_BUFFER_PAGES 16
#define PERF_POLL_TIMEOUT_MS 0
#define INDOM_COUNT 1

static struct env {
    int process_count;
} env = {
    .process_count = 20,
};

static pid_t target_pid = 0;
static bool trace_failed_only = false;
static pmdaInstid *statsnoop_instances;
static struct statsnoop_bpf *obj;
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

#define METRIC_COUNT 6
enum metric_name { PID, COMM, RET, ERR, PATH, LOST };
enum metric_indom { STATSNOOP_INDOM };

char* metric_names[METRIC_COUNT] = {
    [PID]   =  "statsnoop.pid",
    [COMM]  =  "statsnoop.comm",
    [RET]   =  "statsnoop.ret",
    [ERR]   =  "statsnoop.err",
    [PATH]  =  "statsnoop.path",
    [LOST]  =  "statsnoop.lost",
};

char* metric_text_oneline[METRIC_COUNT] = {
    [PID]   =  "Process ID",
    [COMM]  =  "Command name",
    [RET]   =  "Return value",
    [ERR]   =  "errno value",
    [PATH]  =  "Stat path",
    [LOST]  =  "Number of the lost events",
};

char* metric_text_long[METRIC_COUNT] = {
    [PID]   =  "Process identifier",
    [COMM]  =  "Command name for the process",
    [RET]   =  "Return value",
    [ERR]   =  "errno value (see /usr/include/sys/errno.h)",
    [PATH]  =  "Stat path",
    [LOST]  =  "Number of the lost events",
};

static unsigned int statsnoop_metric_count(void)
{
    return METRIC_COUNT;
}

static char* statsnoop_metric_name(unsigned int metric)
{
    return metric_names[metric];
}

static unsigned int statsnoop_indom_count(void)
{
    return INDOM_COUNT;
}

static void statsnoop_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

static int statsnoop_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}

static void statsnoop_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    /* bpf.statsnoop.pid */
    metrics[PID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 0),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[STATSNOOP_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.statsnoop.comm */
    metrics[COMM] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 1),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[STATSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.statsnoop.ret */
    metrics[RET] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 2),
            .type  = PM_TYPE_32,
            .indom = indom_id_mapping[STATSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.statsnoop.err */
    metrics[ERR] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 3),
            .type  = PM_TYPE_32,
            .indom = indom_id_mapping[STATSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.statsnoop.path */
    metrics[PATH] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 4),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[STATSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.statsnoop.lost */
    metrics[LOST] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 5),
            .type  = PM_TYPE_U32,
            .indom = PM_INDOM_NULL,
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
        }
    };

    /* STATSNOOP_INDOM */
    indoms[STATSNOOP_INDOM] = (struct pmdaIndom)
    {
        indom_id_mapping[STATSNOOP_INDOM],
        env.process_count,
        statsnoop_instances,
    };
}

static void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
    struct event *event = data;
    struct tailq_entry *elm = allocElm();

    elm->event.pid = event->pid;
    elm->event.ret = event->ret;
    pmstrncpy(elm->event.comm, sizeof(elm->event.comm), event->comm);
    pmstrncpy(elm->event.pathname, sizeof(elm->event.pathname), event->pathname);

    push(elm);
}

static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    lost_events += lost_cnt;
}

static int statsnoop_init(dict *cfg, char *module_name)
{
    LIBBPF_OPTS(bpf_object_open_opts, open_opts);
    int err;
    char *val;

    if ((val = pmIniFileLookup(cfg, module_name, "process_count")))
        env.process_count = atoi(val);
    if ((val = pmIniFileLookup(cfg, module_name, "pid")))
        target_pid = atoi(val);

    err = ensure_core_btf(&open_opts);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to fetch necessary BTF for CO-RE: %s", strerror(-err));
        return 1;
    }

    obj = statsnoop_bpf__open_opts(&open_opts);
    if (!obj) {
        pmNotifyErr(LOG_ERR, "failed to open BPF object");
        return 1;
    }

    obj->rodata->target_pid = target_pid;
    obj->rodata->trace_failed_only = trace_failed_only;

    err = statsnoop_bpf__load(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to load BPF object: %d", err);
        return err != 0;
    }

    err = statsnoop_bpf__attach(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to attach BPF programs: %d", err);
        return err != 0;
    }

    /* internal/external instance ids */
    fill_instids(env.process_count, &statsnoop_instances);

    /* Initialize the tail queue. */
    TAILQ_INIT(&head);

    pb = perf_buffer__new(bpf_map__fd(obj->maps.events), PERF_BUFFER_PAGES,
            handle_event, handle_lost_events, NULL, NULL);
    if (!pb) {
        err = -errno;
        pmNotifyErr(LOG_ERR, "failed to open perf buffer: %d", err);
        return err != 0;
    }

    return err != 0;
}

static void statsnoop_shutdown()
{
    struct tailq_entry *itemp;

    free(statsnoop_instances);
    perf_buffer__free(pb);
    statsnoop_bpf__destroy(obj);
    /* Free the entire cache queue. */
    while ((itemp = TAILQ_FIRST(&head))) {
        TAILQ_REMOVE(&head, itemp, entries);
        free(itemp);
    }
}

static void statsnoop_refresh(unsigned int item)
{
    perf_buffer__poll(pb, PERF_POLL_TIMEOUT_MS);
}

static int statsnoop_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    struct tailq_entry *value;

    /* bpf.statsnoop.lost */
    if (item == LOST) {
        atom->ul = lost_events;
        return PMDA_FETCH_STATIC;
    }

    if (inst == PM_IN_NULL) {
        return PM_ERR_INST;
    }

    if(!get_item(inst, &value))
        return PMDA_FETCH_NOVALUES;

    /* bpf.statsnoop.pid */
    if (item == PID) {
        atom->ul = value->event.pid;
    }
    /* bpf.statsnoop.comm */
    if (item == COMM) {
        atom->cp = value->event.comm;
    }
    /* bpf.statsnoop.ret */
    if (item == RET) {
        if (value->event.ret >= 0)
            atom->l = value->event.ret;
        else
            atom->l = -1;
    }
    /* bpf.statsnoop.err */
    if (item == ERR) {
        if (value->event.ret >= 0)
            atom->l = 0;
        else
            atom->l = - value->event.ret;
    }
    /* bpf.statsnoop.path */
    if (item == PATH) {
        atom->cp = value->event.pathname;
    }

    return PMDA_FETCH_STATIC;
}

struct module bpf_module = {
    .init               = statsnoop_init,
    .register_metrics   = statsnoop_register,
    .metric_count       = statsnoop_metric_count,
    .indom_count        = statsnoop_indom_count,
    .set_indom_serial   = statsnoop_set_indom_serial,
    .shutdown           = statsnoop_shutdown,
    .refresh            = statsnoop_refresh,
    .fetch_to_atom      = statsnoop_fetch_to_atom,
    .metric_name        = statsnoop_metric_name,
    .metric_text        = statsnoop_metric_text,
};
