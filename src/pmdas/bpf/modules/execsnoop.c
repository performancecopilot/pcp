/*
 *
 * Copyright (c) 2022 Sohaib Mohamed <sohaib.amhmd@gmail.com>
 *
 * Based on the execsnoop(8):
 * https://github.com/iovisor/bcc/blob/master/libbpf-tools/execsnoop.c
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

#include "execsnoop.h"
#include "execsnoop.skel.h"

#define PERF_BUFFER_PAGES   64
#define PERF_POLL_TIMEOUT_MS	50
#define MAX_ARGS_KEY 259

#define INDOM_COUNT 2

static struct env {
    bool fails;
    uid_t uid;
    const char *name;
    const char *line;
    int max_args;
    int process_count;
} env = {
    .max_args = DEFAULT_MAXARGS,
    .uid = INVALID_UID,
    .process_count = 20,
};

static pmdaInstid *execsnoop_instances, *execsnoop_lost;
static struct execsnoop_bpf *obj;
static struct perf_buffer *pb = NULL;
static struct event *event;
static int lost_events;

/* cache array */
struct tailq_entry {
    struct event event;
    TAILQ_ENTRY(tailq_entry) entries;
};

TAILQ_HEAD(tailhead, tailq_entry) head;
static int queuelength;

static struct tailq_entry* allocElm(void)
{
    return malloc(sizeof(struct tailq_entry));
}

static void push(struct tailq_entry *elm)
{
    if (queuelength > env.process_count)
    {
        struct tailq_entry *l;
        l = head.tqh_first;
        TAILQ_REMOVE(&head, l, entries);
        free(l);
        queuelength--;
    }
    TAILQ_INSERT_TAIL(&head, elm, entries);
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

static char arg_val[FULL_MAX_ARGS_ARR];
static unsigned int indom_id_mapping[INDOM_COUNT];

#define METRIC_COUNT 7
enum metric_name { COMM, PID, PPID, RET, ARGS, UID, LOST };
enum metric_indom { EXECSNOOP_INDOM, LOST_EVENTS };

char* metric_names[METRIC_COUNT] = {
    [COMM] = "execsnoop.comm",
    [PID]  = "execsnoop.pid",
    [PPID] = "execsnoop.ppid",
    [RET]  = "execsnoop.ret",
    [ARGS] = "execsnoop.args",
    [UID]  = "execsnoop.uid",
    [LOST] = "execsnoop.lost",
};

char* metric_text_oneline[METRIC_COUNT] = {
    [COMM] = "Command name",
    [PID]  = "Process identifier",
    [PPID] = "Process ID of the parent process",
    [RET]  = "Return value of exec()",
    [ARGS] = "Details of the arguments",
    [UID]  = "User identifier",
    [LOST] = "Number of the lost events",
};

char* metric_text_long[METRIC_COUNT] = {
    [COMM] = "Command name",
    [PID]  = "Process identifier",
    [PPID] = "Process ID of the parent process",
    [RET]  = "Return value of exec()",
    [ARGS] = "Details of the arguments",
    [UID]  = "User identifier",
    [LOST] = "Number of the lost events",
};

static unsigned int execsnoop_metric_count()
{
    return METRIC_COUNT;
}

static char* execsnoop_metric_name(unsigned int metric)
{
    return metric_names[metric];
}

static unsigned int execsnoop_indom_count(void)
{
    return INDOM_COUNT;
}

static void execsnoop_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

static int execsnoop_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}

static void execsnoop_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    /* bpf.execsnoop.comm */
    metrics[COMM] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 0),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[EXECSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.execsnoop.pid */
    metrics[PID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 1),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[EXECSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.execsnoop.ppid */
    metrics[PPID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 2),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[EXECSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.execsnoop.ret */
    metrics[RET] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 3),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[EXECSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.execsnoop.args */
    metrics[ARGS] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 4),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[EXECSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.execsnoop.uid */
    metrics[UID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 5),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[EXECSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.execsnoop.uid */
    metrics[LOST] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 6),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[EXECSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };

    /* EXECSNOOP_INDOM */
    indoms[0] = (struct pmdaIndom)
    {
        indom_id_mapping[EXECSNOOP_INDOM],
        env.process_count,
        execsnoop_instances,
    };

    /* LOST_EVENTS InDom */
    indoms[1] = (struct pmdaIndom)
    {
        indom_id_mapping[LOST_EVENTS],
        1,
        execsnoop_lost,
    };
}

static void handle_args(const struct event *e)
{
    int i, args_counter = 0;

    for (i = 0; i < e->args_size && args_counter < e->args_count; i++) {
        char c = e->args[i];
        if (c == '\0') {
            args_counter++;
            arg_val[i] = ' ';
        } else {
            arg_val[i] = c;
        }
    }
    if (e->args_count == env.max_args + 1) {
        arg_val[e->args_size-1] = '\0';
    }
    if (e->args_size <= ARGSIZE ) {
        arg_val[e->args_size-1] = '\0';
    }
}

static void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
    event = data;
    handle_args(event);

    struct tailq_entry *elm = allocElm();

    elm->event.pid = event->pid;
    elm->event.ppid = event->ppid;
    elm->event.uid = event->uid;
    elm->event.retval = event->retval;
    elm->event.args_count = event->args_count;
    elm->event.args_size = event->args_size;
    strncpy(elm->event.comm, event->comm, sizeof(event->comm));
    strncpy(elm->event.args, arg_val, sizeof(arg_val));

    /* TODO: use pcre lib */
    if (env.name && strstr(elm->event.comm, env.name) == NULL) {
        free(elm);
        return;
    }

    /* TODO: use pcre lib */
    if (env.line && strstr(elm->event.comm, env.line) == NULL) {
        free(elm);
        return;
    }

    push(elm);
}

static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    lost_events = lost_cnt;
}

static int execsnoop_init(dict *cfg, char *module_name)
{
    int err;
    char *val;

    if ((val = pmIniFileLookup(cfg, module_name, "uid")))
        env.uid = strtol(val, NULL, 10);
    if ((val = pmIniFileLookup(cfg, module_name, "max_args")))
        env.max_args = atoi(val);
    if ((val = pmIniFileLookup(cfg, module_name, "process_count")))
        env.process_count = atoi(val);
    if ((val = pmIniFileLookup(cfg, module_name, "include_failed")))
        env.fails = atoi(val);
    if ((val = pmIniFileLookup(cfg, module_name, "command")))
        env.name = val;
    if ((val = pmIniFileLookup(cfg, module_name, "line")))
        env.line = val;

    obj = execsnoop_bpf__open();
    if (!obj) {
        pmNotifyErr(LOG_ERR, "failed to open BPF object");
        return 1;
    }
    pmNotifyErr(LOG_INFO, "booting: %s", obj->skeleton->name);

    /* initialize global data (filtering options) */
    obj->rodata->ignore_failed = !env.fails;
    obj->rodata->targ_uid = env.uid;
    obj->rodata->max_args = env.max_args;

    /* initialize global data (filtering options) */
    obj->rodata->ignore_failed = !env.fails;
    obj->rodata->targ_uid = env.uid;
    obj->rodata->max_args = env.max_args;

    err = execsnoop_bpf__load(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to load BPF object: %d", err);
        return err != 0;
    }

    err = execsnoop_bpf__attach(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to attach BPF programs");
        return err != 0;
    }

    /* setup event callbacks */
    pb = perf_buffer__new(bpf_map__fd(obj->maps.events), PERF_BUFFER_PAGES,
            handle_event, handle_lost_events, NULL, NULL);
    if (!pb) {
        err = -errno;
        pmNotifyErr(LOG_ERR, "failed to open perf buffer: %d", err);
        return err != 0;
    }

    /* internal/external instance ids */
    fill_instids(env.process_count, &execsnoop_instances);
    fill_instids(env.process_count, &execsnoop_lost);

    /* Initialize the tail queue. */
    TAILQ_INIT(&head);

    return err != 0;
}

static void execsnoop_shutdown()
{
    struct tailq_entry *itemp;

    free(execsnoop_instances);
    free(execsnoop_lost);
    perf_buffer__free(pb);
    execsnoop_bpf__destroy(obj);
    /* Free the entire cache queue. */
    while ((itemp = TAILQ_FIRST(&head))) {
        TAILQ_REMOVE(&head, itemp, entries);
        free(itemp);
    }
}

static void execsnoop_refresh(unsigned int item)
{
    perf_buffer__poll(pb, PERF_POLL_TIMEOUT_MS);
}

static int execsnoop_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    struct tailq_entry *value;
    bool exist;

    if (inst == PM_IN_NULL) {
        return PM_ERR_INST;
    }

    exist = get_item(inst, &value);
    if (!exist)
       return PMDA_FETCH_NOVALUES;

    /* bpf.execsnoop.comm */
    if (item == COMM) {
        atom->cp = value->event.comm;
    }
    /* bpf.execsnoop.pid */
    if (item == PID) {
        atom->ul = value->event.pid;
    }
    /* bpf.execsnoop.ppid */
    if (item == PPID) {
        atom->ul = value->event.ppid;
    }
    /* bpf.execsnoop.ret */
    if (item == RET) {
        atom->l = value->event.retval;
    }
    /* bpf.execsnoop.args */
    if (item == ARGS) {
        atom->cp = value->event.args;
    }
    /* bpf.execsnoop.uid */
    if (item == UID) {
        atom->ul = value->event.uid;
    }
    /* bpf.execsnoop.lost */
    if (item == LOST) {
        atom->ul = lost_events;
    }

    return PMDA_FETCH_STATIC;
}

struct module bpf_module = {
    .init               = execsnoop_init,
    .register_metrics   = execsnoop_register,
    .metric_count       = execsnoop_metric_count,
    .indom_count        = execsnoop_indom_count,
    .set_indom_serial   = execsnoop_set_indom_serial,
    .shutdown           = execsnoop_shutdown,
    .refresh            = execsnoop_refresh,
    .fetch_to_atom      = execsnoop_fetch_to_atom,
    .metric_name        = execsnoop_metric_name,
    .metric_text        = execsnoop_metric_text,
};
