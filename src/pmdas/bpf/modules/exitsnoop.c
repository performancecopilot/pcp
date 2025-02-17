/*
 *
 * Copyright (c) 2022 Sohaib Mohamed <sohaib.amhmd@gmail.com>
 *
 * Based on the exitsnoop(8):
 * https://github.com/iovisor/bcc/blob/master/libbpf-tools/exitsnoop.c
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

#include "exitsnoop.h"
#include "exitsnoop.skel.h"
#include "btf_helpers.h"

#define PERF_BUFFER_PAGES 16
#define PERF_POLL_TIMEOUT_MS 0

static pid_t target_pid = 0;
static bool trace_failed_only = false;
static bool trace_by_process = true;

static struct env {
    char *cgroupspath;
    bool cg;
    int process_count;
} env = {
    .process_count = 20,
};

static pmdaInstid *exitsnoop_instances;
static struct exitsnoop_bpf *obj;
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

#define INDOM_COUNT 1
static unsigned int indom_id_mapping[INDOM_COUNT];

#define METRIC_COUNT 9
enum metric_name { AGE, PID, TID, PPID, SIG, EXIT_CODE, COMM, COREDUMP, LOST };
enum metric_indom { EXITSNOOP_INDOM };

char* metric_names[METRIC_COUNT] = {
    [AGE]        =  "exitsnoop.age",
    [PID]        =  "exitsnoop.pid",
    [TID]        =  "exitsnoop.tid",
    [PPID]       =  "exitsnoop.ppid",
    [SIG]        =  "exitsnoop.sig",
    [EXIT_CODE]  =  "exitsnoop.exit_code",
    [COMM]       =  "exitsnoop.comm",
    [COREDUMP]   =  "exitsnoop.coredump",
    [LOST]       =  "exitsnoop.lost",
};

char* metric_text_oneline[METRIC_COUNT] = {
    [AGE]        =  "the AGE of the process with hundredth of a second resolution",
    [PID]        =  "Process identifier",
    [TID]        =  "Thread ID",
    [PPID]       =  "The process that will be notified",
    [SIG]        =  "The signal",
    [EXIT_CODE]  =  "The exit code for exit() or the signal number for a fatal signal.",
    [COMM]       =  "The process command name",
    [COREDUMP]   =  "bool value repressed if it's coredump or not",
    [LOST]       =  "The Number of the lost events",
};

char* metric_text_long[METRIC_COUNT] = {
    [AGE]        =  "the AGE of the process with hundredth of a second resolution",
    [PID]        =  "Process identifier",
    [TID]        =  "Thread ID",
    [PPID]       =  "The process that will be notified",
    [SIG]        =  "The signal",
    [EXIT_CODE]  =  "The exit code for exit() or the signal number for a fatal signal.",
    [COMM]       =  "The process command name",
    [COREDUMP]   =  "bool value repressed if it's coredump or not",
    [LOST]       =  "The Number of the lost events",
};

static unsigned int exitsnoop_metric_count(void)
{
    return METRIC_COUNT;
}

static char* exitsnoop_metric_name(unsigned int metric)
{
    return metric_names[metric];
}

static unsigned int exitsnoop_indom_count(void)
{
    return INDOM_COUNT;
}

static void exitsnoop_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

static int exitsnoop_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}

static void exitsnoop_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    /* bpf.exitsnoop.age */
    metrics[AGE] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 0),
            .type  = PM_TYPE_DOUBLE,
            .indom = indom_id_mapping[EXITSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0),
        }
    };
    /* bpf.exitsnoop.pid */
    metrics[PID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 1),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[EXITSNOOP_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.exitsnoop.tid */
    metrics[TID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 2),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[EXITSNOOP_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.exitsnoop.ppid */
    metrics[PPID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 3),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[EXITSNOOP_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.exitsnoop.sig */
    metrics[SIG] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 4),
            .type  = PM_TYPE_32,
            .indom = indom_id_mapping[EXITSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.exitsnoop.exit_code */
    metrics[EXIT_CODE] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 5),
            .type  = PM_TYPE_32,
            .indom = indom_id_mapping[EXITSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.exitsnoop.exit_code */
    metrics[COMM] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 6),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[EXITSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.exitsnoop.coredump */
    metrics[COREDUMP] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 7),
            .type  = PM_TYPE_32,
            .indom = indom_id_mapping[EXITSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.exitsnoop.lost */
    metrics[LOST] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 8),
            .type  = PM_TYPE_U32,
            .indom = PM_INDOM_NULL,
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
        }
    };

    /* EXITSNOOP_INDOM */
    indoms[EXITSNOOP_INDOM] = (struct pmdaIndom)
    {
        indom_id_mapping[EXITSNOOP_INDOM],
        env.process_count,
        exitsnoop_instances,
    };
}

static void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
    struct event *event = data;
    struct tailq_entry *elm = allocElm();

    elm->event.start_time  =  event->start_time;
    elm->event.exit_time   =  event->exit_time;
    elm->event.pid         =  event->pid;
    elm->event.tid         =  event->tid;
    elm->event.ppid        =  event->ppid;
    elm->event.sig         =  event->sig;
    elm->event.exit_code   =  event->exit_code;
    pmstrncpy(elm->event.comm, sizeof(elm->event.comm), event->comm);

    push(elm);
}

static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    lost_events += lost_cnt;
}

static int exitsnoop_init(dict *cfg, char *module_name)
{
    LIBBPF_OPTS(bpf_object_open_opts, open_opts);
    int err;
    char *val;
    int idx, cg_map_fd;
    int cgfd = -1;

    if ((val = pmIniFileLookup(cfg, module_name, "process_count")))
        env.process_count = atoi(val);

    err = ensure_core_btf(&open_opts);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to fetch necessary BTF for CO-RE: %s", strerror(-err));
        return 1;
    }

    obj = exitsnoop_bpf__open_opts(&open_opts);
    if (!obj) {
        pmNotifyErr(LOG_ERR, "failed to open BPF object");
        return 1;
    }

    obj->rodata->target_pid = target_pid;
    obj->rodata->trace_failed_only = trace_failed_only;
    obj->rodata->trace_by_process = trace_by_process;
    obj->rodata->filter_cg = env.cg;

    err = exitsnoop_bpf__load(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to load BPF object: %d", err);
        return err != 0;
    }

    /* update cgroup path fd to map */
    if (env.cg) {
        idx = 0;
        cg_map_fd = bpf_map__fd(obj->maps.cgroup_map);
        cgfd = open(env.cgroupspath, O_RDONLY);
        if (cgfd < 0) {
            pmNotifyErr(LOG_ERR, "Failed opening Cgroup path: %s", env.cgroupspath);
            return err != 0;
        }
        if (bpf_map_update_elem(cg_map_fd, &idx, &cgfd, BPF_ANY)) {
            pmNotifyErr(LOG_ERR, "Failed adding target cgroup to map");
            return err != 0;
        }
    }

    err = exitsnoop_bpf__attach(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to attach BPF programs: %d", err);
        return err != 0;
    }

    /* internal/external instance ids */
    fill_instids(env.process_count, &exitsnoop_instances);

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

static void exitsnoop_shutdown()
{
    struct tailq_entry *itemp;

    free(exitsnoop_instances);
    perf_buffer__free(pb);
    exitsnoop_bpf__destroy(obj);

    /* Free the entire cache queue. */
    while ((itemp = TAILQ_FIRST(&head))) {
        TAILQ_REMOVE(&head, itemp, entries);
        free(itemp);
    }
}

static void exitsnoop_refresh(unsigned int item)
{
    perf_buffer__poll(pb, PERF_POLL_TIMEOUT_MS);
}

static int exitsnoop_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    struct tailq_entry *value;

    /* bpf.exitsnoop.lost */
    if (item == LOST) {
        atom->ul = lost_events;
        return PMDA_FETCH_STATIC;
    }

    if (inst == PM_IN_NULL) {
        return PM_ERR_INST;
    }

    if(!get_item(inst, &value))
        return PMDA_FETCH_NOVALUES;

    /* bpf.exitsnoop.age */
    if (item == AGE) {
        atom->d = (value->event.exit_time - value->event.start_time);
    }
    /* bpf.exitsnoop.PID */
    if (item == PID) {
        atom->ul = value->event.pid;
    }
    /* bpf.exitsnoop.tid */
    if (item == TID) {
        atom->ul = value->event.tid;
    }
    /* bpf.exitsnoop.ppid */
    if (item == PPID) {
        atom->ul = value->event.ppid;
    }
    /* bpf.exitsnoop.sig */
    if (item == SIG) {
        atom->l = value->event.sig & 0x7f;
    }
    /* bpf.exitsnoop.exit_code */
    if (item == EXIT_CODE) {
        atom->l = value->event.exit_code;
    }
    /* bpf.exitsnoop.comm */
    if (item == COMM) {
        atom->cp = value->event.comm;
    }
    /* bpf.exitsnoop.coredump */
    if (item == COREDUMP) {
        atom->l = value->event.sig & 0x80;
    }

    return PMDA_FETCH_STATIC;
}

struct module bpf_module = {
    .init               = exitsnoop_init,
    .register_metrics   = exitsnoop_register,
    .metric_count       = exitsnoop_metric_count,
    .indom_count        = exitsnoop_indom_count,
    .set_indom_serial   = exitsnoop_set_indom_serial,
    .shutdown           = exitsnoop_shutdown,
    .refresh            = exitsnoop_refresh,
    .fetch_to_atom      = exitsnoop_fetch_to_atom,
    .metric_name        = exitsnoop_metric_name,
    .metric_text        = exitsnoop_metric_text,
};
