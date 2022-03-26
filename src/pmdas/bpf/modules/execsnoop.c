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
#include "trace_helpers.h"

#define PERF_BUFFER_PAGES   64
#define PERF_POLL_TIMEOUT_MS	100
#define NSEC_PRECISION (NSEC_PER_SEC / 1000)
#define MAX_ARGS_KEY 259

#define INDOM_COUNT 1
#define EXECSNOOP_INDOM 0

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

pmdaInstid *execsnoop_instances;
struct execsnoop_bpf *obj;
struct perf_buffer *pb = NULL;
struct event *event;
char arg_val[FULL_MAX_ARGS_ARR];

unsigned int indom_id_mapping[INDOM_COUNT];

#define METRIC_COUNT 6
enum metric_name { COMM, PID, PPID, RET, ARGS, UID, };

char* metric_names[METRIC_COUNT] = {
    "execsnoop.comm",
    "execsnoop.pid",
    "execsnoop.ppid",
    "execsnoop.ret",
    "execsnoop.args",
    "execsnoop.uid",
};

char* metric_text_oneline[METRIC_COUNT] = {
    "Command name",
    "Process identifier",
    "Process ID of the parent process",
    "Return value of exec()",
    "Details of the arguments",
    "User identifier",
};

char* metric_text_long[METRIC_COUNT] = {
    "Command name",
    "Process identifier",
    "Process ID of the parent process",
    "Return value of exec()",
    "Details of the arguments",
    "User identifier",
};

unsigned int execsnoop_metric_count()
{
    return METRIC_COUNT;
}

char* execsnoop_metric_name(unsigned int metric)
{
    return metric_names[metric];
}

unsigned int execsnoop_indom_count()
{
    return INDOM_COUNT;
}

void execsnoop_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

int execsnoop_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}

void execsnoop_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    /* bpf.execsnoop.comm */
    metrics[COMM] = (struct pmdaMetric)
    { /* m_user */ NULL,
        { /* m_desc */
            PMDA_PMID(cluster_id, 0), PM_TYPE_STRING, indom_id_mapping[EXECSNOOP_INDOM],
            PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
        }
    };
    /* bpf.execsnoop.pid */
    metrics[PID] = (struct pmdaMetric)
    { /* m_user */ NULL,
        { /* m_desc */
            PMDA_PMID(cluster_id, 1), PM_TYPE_U32, indom_id_mapping[EXECSNOOP_INDOM],
            PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
        }
    };
    /* bpf.execsnoop.ppid */
    metrics[PPID] = (struct pmdaMetric)
    { /* m_user */ NULL,
        { /* m_desc */
            PMDA_PMID(cluster_id, 2), PM_TYPE_U32, indom_id_mapping[EXECSNOOP_INDOM],
            PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
        }
    };
    /* bpf.execsnoop.ret */
    metrics[RET] = (struct pmdaMetric)
    { /* m_user */ NULL,
        { /* m_desc */
            PMDA_PMID(cluster_id, 3), PM_TYPE_U32, indom_id_mapping[EXECSNOOP_INDOM],
            PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
        }
    };
    /* bpf.execsnoop.args */
    metrics[ARGS] = (struct pmdaMetric)
    { /* m_user */ NULL,
        { /* m_desc */
            PMDA_PMID(cluster_id, 4), PM_TYPE_STRING, indom_id_mapping[EXECSNOOP_INDOM],
            PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
        }
    };
    /* bpf.execsnoop.uid */
    metrics[UID] = (struct pmdaMetric)
    { /* m_user */ NULL,
        { /* m_desc */
            PMDA_PMID(cluster_id, 5), PM_TYPE_U32, indom_id_mapping[EXECSNOOP_INDOM],
            PM_SEM_INSTANT, PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
        }
    };

    indoms[0] = (struct pmdaIndom)
    {
        indom_id_mapping[EXECSNOOP_INDOM],
        env.process_count,
        execsnoop_instances,
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
    strncpy(elm->event.comm, event->comm, sizeof(event->comm));
    strncpy(elm->event.args, arg_val, sizeof(arg_val));

    strncpy(elm->event.comm, event->comm, sizeof(event->comm));
    strncpy(elm->event.args, arg_val, sizeof(arg_val));
    /* TODO: use pcre lib */
    if (env.name && strstr(event->comm, env.name) == NULL)
        return;

    /* TODO: use pcre lib */
    if (env.line && strstr(event->comm, env.line) == NULL)
        return;

    handle_args(event);
}

static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    pmNotifyErr(LOG_ERR, "Lost %llu events on CPU #%d!", lost_cnt, cpu);
}

int execsnoop_init(dict *cfg, char *module_name)
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

    return err != 0;
}

void execsnoop_shutdown()
{
    free(execsnoop_instances);
    perf_buffer__free(pb);
    execsnoop_bpf__destroy(obj);
}


void execsnoop_refresh(unsigned int item)
{
    /* do nothing */
}

int execsnoop_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    if (inst == PM_IN_NULL) {
        return PM_ERR_INST;
    }

    perf_buffer__poll(pb, PERF_POLL_TIMEOUT_MS);

    /* bpf.execsnoop.comm */
    if (item == COMM) {
        atom->cp = event->comm;
    }
    /* bpf.execsnoop.pid */
    if (item == PID) {
        atom->ul = event->pid;
    }
    /* bpf.execsnoop.ppid */
    if (item == PPID) {
        atom->ul = event->ppid;
    }
    /* bpf.execsnoop.ret */
    if (item == RET) {
        atom->ul = event->retval;
    }
    /* bpf.execsnoop.args */
    if (item == ARGS) {
        atom->cp = arg_val;
    }
    /* bpf.execsnoop.uid */
    if (item == UID) {
        atom->ul = event->uid;
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
