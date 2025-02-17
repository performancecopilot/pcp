/*
 *
 * Copyright (c) 2022 Sohaib Mohamed <sohaib.amhmd@gmail.com>
 *
 * Based on the opensnoop(8):
 * https://github.com/iovisor/bcc/blob/master/libbpf-tools/opensnoop.c
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

#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <pcp/pmda.h>
#include <pcp/pmwebapi.h>
#include <string.h>
#include <sys/queue.h>

#include "opensnoop.h"
#include "opensnoop.skel.h"
#include "btf_helpers.h"

#define PERF_BUFFER_PAGES 64
#define PERF_POLL_TIMEOUT_MS 0
#define NSEC_PER_SEC 1000000000ULL
#define INDOM_COUNT 1

static struct env {
    pid_t pid;
    pid_t tid;
    uid_t uid;
    bool failed;
    char *name;
    int process_count;
} env = {
    .uid = INVALID_UID,
    .process_count = 20,
};

static pmdaInstid *opensnoop_instances;
static struct opensnoop_bpf *obj;
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
enum metric_name { PID, UID, FD, ERR, FLAGS, COMM, FNAME, LOST };
enum metric_indom { OPENSNOOP_INDOM };

char* metric_names[METRIC_COUNT] = {
    [PID]    =  "opensnoop.pid",
    [UID]    =  "opensnoop.uid",
    [FD]     =  "opensnoop.fd",
    [ERR]    =  "opensnoop.err",
    [FLAGS]  =  "opensnoop.flags",
    [COMM]   =  "opensnoop.comm",
    [FNAME]  =  "opensnoop.fname",
    [LOST]   =  "opensnoop.lost",
};

char* metric_text_oneline[METRIC_COUNT] = {
    [PID]    =  "Process ID",
    [UID]    =  "User ID",
    [FD]     =  "File descriptor",
    [ERR]    =  "Return value",
    [FLAGS]  =  "opoen() sys call flags",
    [COMM]   =  "Command name",
    [FNAME]  =  "File name",
    [LOST]   =  "Number of the lost events",
};

char* metric_text_long[METRIC_COUNT] = {
    [PID]    =  "Process identifier",
    [UID]    =  "User identifier",
    [FD]     =  "File Descriptor (-1 is error)",
    [ERR]    =  "errno value (see /usr/include/sys/errno.h)",
    [FLAGS]  =  "opoen() sys call flags",
    [COMM]   =  "Command name for the process",
    [FNAME]  =  "File name",
    [LOST]   =  "Number of the lost events",
};

static unsigned int opensnoop_metric_count(void)
{
    return METRIC_COUNT;
}

static char* opensnoop_metric_name(unsigned int metric)
{
    return metric_names[metric];
}

static unsigned int opensnoop_indom_count(void)
{
    return INDOM_COUNT;
}

static void opensnoop_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

static int opensnoop_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}

static void opensnoop_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    /* bpf.opensnoop.pid */
    metrics[PID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 0),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[OPENSNOOP_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.opensnoop.uid */
    metrics[UID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 1),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[OPENSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.opensnoop.fd */
    metrics[FD] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 2),
            .type  = PM_TYPE_32,
            .indom = indom_id_mapping[OPENSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.opensnoop.err */
    metrics[ERR] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 3),
            .type  = PM_TYPE_32,
            .indom = indom_id_mapping[OPENSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.opensnoop.flags */
    metrics[FLAGS] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 4),
            .type  = PM_TYPE_32,
            .indom = indom_id_mapping[OPENSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.opensnoop.comm */
    metrics[COMM] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 5),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[OPENSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.opensnoop.fname */
    metrics[FNAME] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 6),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[OPENSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.opensnoop.lost */
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

    /* OPENSNOOP_INDOM */
    indoms[OPENSNOOP_INDOM] = (struct pmdaIndom)
    {
        indom_id_mapping[OPENSNOOP_INDOM],
        env.process_count,
        opensnoop_instances,
    };
}

static void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
    struct event *event = data;
    struct tailq_entry *elm = allocElm();

    /* name filtering is currently done in user space */
    if (env.name && strstr(event->comm, env.name) == NULL)
        return;

    elm->event.pid = event->pid;
    elm->event.uid = event->uid;
    elm->event.ret = event->ret;
    elm->event.flags = event->flags;
    pmstrncpy(elm->event.comm, sizeof(elm->event.comm), event->comm);
    pmstrncpy(elm->event.fname, sizeof(elm->event.fname), event->fname);

    push(elm);
}

static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    lost_events += lost_cnt;
}

static int opensnoop_init(dict *cfg, char *module_name)
{
    LIBBPF_OPTS(bpf_object_open_opts, open_opts);
    int err;
    char *val;

    if ((val = pmIniFileLookup(cfg, module_name, "process_count")))
        env.process_count = atoi(val);

    err = ensure_core_btf(&open_opts);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to fetch necessary BTF for CO-RE: %s", strerror(-err));
        return err != 0;
    }

    obj = opensnoop_bpf__open_opts(&open_opts);
    if (!obj) {
        pmNotifyErr(LOG_ERR, "failed to open BPF object");
        return err != 0;
    }

    /* initialize global data (filtering options) */
    obj->rodata->targ_tgid = env.pid;
    obj->rodata->targ_pid = env.tid;
    obj->rodata->targ_uid = env.uid;
    obj->rodata->targ_failed = env.failed;

#ifdef __aarch64__
    /* aarch64 has no open syscall, only openat variants.
     * Disable associated tracepoints that do not exist. See #3344.
     */
    bpf_program__set_autoload(
            obj->progs.tracepoint__syscalls__sys_enter_open, false);
    bpf_program__set_autoload(
            obj->progs.tracepoint__syscalls__sys_exit_open, false);
#endif

    err = opensnoop_bpf__load(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to load BPF object: %d", err);
        return err != 0;
    }

    err = opensnoop_bpf__attach(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to attach BPF programs");
        return err != 0;
    }

    /* internal/external instance ids */
    fill_instids(env.process_count, &opensnoop_instances);

    /* Initialize the tail queue. */
    TAILQ_INIT(&head);

    /* setup event callbacks */
    pb = perf_buffer__new(bpf_map__fd(obj->maps.events), PERF_BUFFER_PAGES,
            handle_event, handle_lost_events, NULL, NULL);
    if (!pb) {
        err = -errno;
        pmNotifyErr(LOG_ERR, "failed to open perf buffer: %d", err);
        return err != 0;
    }

    return err != 0;
}

static void opensnoop_shutdown()
{
    struct tailq_entry *itemp;

    free(opensnoop_instances);
    perf_buffer__free(pb);
    opensnoop_bpf__destroy(obj);
    /* Free the entire cache queue. */
    while ((itemp = TAILQ_FIRST(&head))) {
        TAILQ_REMOVE(&head, itemp, entries);
        free(itemp);
    }
}

static void opensnoop_refresh(unsigned int item)
{
    perf_buffer__poll(pb, PERF_POLL_TIMEOUT_MS);
}

static int opensnoop_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    struct tailq_entry *value;

    /* bpf.opensnoop.lost */
    if (item == LOST) {
        atom->ul = lost_events;
        return PMDA_FETCH_STATIC;
    }

    if (inst == PM_IN_NULL) {
        return PM_ERR_INST;
    }

    if(!get_item(inst, &value))
        return PMDA_FETCH_NOVALUES;

    /* bpf.opensnoop.pid */
    if (item == PID) {
        atom->ul = value->event.pid;
    }
    /* bpf.opensnoop.uid */
    if (item == UID) {
        atom->ul = value->event.uid;
    }
    /* bpf.opensnoop.fd */
    if (item == FD) {
        if (value->event.ret >= 0)
            atom->l = value->event.ret;
        else
            atom->l = -1;
    }
    /* bpf.opensnoop.err */
    if (item == ERR) {
        if (value->event.ret >= 0)
            atom->l = 0;
        else
            atom->l = - value->event.ret;
    }
    /* bpf.opensnoop.flags */
    if (item == FLAGS) {
        atom->l = value->event.flags;
    }
    /* bpf.opensnoop.comm */
    if (item == COMM) {
        atom->cp = value->event.comm;
    }
    /* bpf.opensnoop.fname */
    if (item == FNAME) {
        atom->cp = value->event.fname;
    }

    return PMDA_FETCH_STATIC;
}

struct module bpf_module = {
    .init               = opensnoop_init,
    .register_metrics   = opensnoop_register,
    .metric_count       = opensnoop_metric_count,
    .indom_count        = opensnoop_indom_count,
    .set_indom_serial   = opensnoop_set_indom_serial,
    .shutdown           = opensnoop_shutdown,
    .refresh            = opensnoop_refresh,
    .fetch_to_atom      = opensnoop_fetch_to_atom,
    .metric_name        = opensnoop_metric_name,
    .metric_text        = opensnoop_metric_text,
};
