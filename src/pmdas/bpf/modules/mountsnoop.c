/*
 *
 * Copyright (c) 2022 Sohaib Mohamed <sohaib.amhmd@gmail.com>
 *
 * Based on the mountsnoop(8):
 * https://github.com/iovisor/bcc/blob/master/libbpf-tools/mountsnoop.c
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

#include "mountsnoop.h"
#include "mountsnoop.skel.h"
#include "btf_helpers.h"
#include "compat.h"

#define PERF_BUFFER_PAGES 64
#define PERF_POLL_TIMEOUT_MS 0

/* https://www.gnu.org/software/gnulib/manual/html_node/strerrorname_005fnp.html */
#if !defined(__GLIBC__) || __GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 32)
const char *strerrorname_np(int errnum)
{
    return NULL;
}
#endif

static struct env {
    int process_count;
} env = {
    .process_count = 20,
};

static pid_t target_pid = 0;
static const char *flag_names[] = {
    [0] = "MS_RDONLY",
    [1] = "MS_NOSUID",
    [2] = "MS_NODEV",
    [3] = "MS_NOEXEC",
    [4] = "MS_SYNCHRONOUS",
    [5] = "MS_REMOUNT",
    [6] = "MS_MANDLOCK",
    [7] = "MS_DIRSYNC",
    [8] = "MS_NOSYMFOLLOW",
    [9] = "MS_NOATIME",
    [10] = "MS_NODIRATIME",
    [11] = "MS_BIND",
    [12] = "MS_MOVE",
    [13] = "MS_REC",
    [14] = "MS_VERBOSE",
    [15] = "MS_SILENT",
    [16] = "MS_POSIXACL",
    [17] = "MS_UNBINDABLE",
    [18] = "MS_PRIVATE",
    [19] = "MS_SLAVE",
    [20] = "MS_SHARED",
    [21] = "MS_RELATIME",
    [22] = "MS_KERNMOUNT",
    [23] = "MS_I_VERSION",
    [24] = "MS_STRICTATIME",
    [25] = "MS_LAZYTIME",
    [26] = "MS_SUBMOUNT",
    [27] = "MS_NOREMOTELOCK",
    [28] = "MS_NOSEC",
    [29] = "MS_BORN",
    [30] = "MS_ACTIVE",
    [31] = "MS_NOUSER",
};
static const int flag_count = sizeof(flag_names) / sizeof(flag_names[0]);

static pmdaInstid *mountsnoop_instances;
static struct mountsnoop_bpf *obj;
static struct bpf_buffer *buf;
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

#define METRIC_COUNT 14
enum metric_name { PID, TID, COMM, OP, RET, LAT, MNT_NS, FS, SOURCE, TARGET,
    DATA, FLAGS, CALL, LOST};
enum metric_indom { MOUNTSNOOP_INDOM };

char* metric_names[METRIC_COUNT] = {
    [PID]     =  "mountsnoop.pid",
    [TID]     =  "mountsnoop.tid",
    [COMM]    =  "mountsnoop.comm",
    [OP]      =  "mountsnoop.op",
    [RET]     =  "mountsnoop.ret",
    [LAT]     =  "mountsnoop.lat",
    [MNT_NS]  =  "mountsnoop.mnt_ns",
    [FS]      =  "mountsnoop.fs",
    [SOURCE]  =  "mountsnoop.source",
    [TARGET]  =  "mountsnoop.target",
    [DATA]    =  "mountsnoop.data",
    [FLAGS]   =  "mountsnoop.flags",
    [CALL]    =  "mountsnoop.call",
    [LOST]    =  "mountsnoop.lost",
};

char* metric_text_oneline[METRIC_COUNT] = {
    [PID]     =  "Process identifier",
    [TID]     =  "Thread identifier",
    [COMM]    =  "Command name",
    [OP]      =  "operation",
    [RET]     =  "u/mount() return value",
    [LAT]     =  "Latency",
    [MNT_NS]  =  "MNT_NS",
    [FS]      =  "Filesystem",
    [SOURCE]  =  "Source",
    [TARGET]  =  "Target",
    [DATA]    =  "Data",
    [FLAGS]   =  "Flags",
    [CALL]    =  "u/mount(params)",
    [LOST]    =  "Number of the lost events",
};

char* metric_text_long[METRIC_COUNT] = {
    [PID]     =  "Process identifier",
    [TID]     =  "Thread identifier",
    [COMM]    =  "Command name",
    [OP]      =  "operation",
    [RET]     =  "u/mount() return value",
    [LAT]     =  "Latency",
    [MNT_NS]  =  "MNT_NS",
    [FS]      =  "Filesystem",
    [SOURCE]  =  "Source",
    [TARGET]  =  "Target",
    [DATA]    =  "Data",
    [FLAGS]   =  "Flags",
    [CALL]    =  "u/mount(params)",
    [LOST]    =  "Number of the lost events",
};

static unsigned int mountsnoop_metric_count(void)
{
    return METRIC_COUNT;
}

static char* mountsnoop_metric_name(unsigned int metric)
{
    return metric_names[metric];
}

static unsigned int mountsnoop_indom_count(void)
{
    return INDOM_COUNT;
}

static void mountsnoop_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

static int mountsnoop_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}

static void mountsnoop_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    /* bpf.mountsnoop.pid */
    metrics[PID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 0),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[MOUNTSNOOP_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.mountsnoop.tid */
    metrics[TID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 1),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[MOUNTSNOOP_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.mountsnoop.comm */
    metrics[COMM] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 2),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[MOUNTSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.mountsnoop.op */
    metrics[OP] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 3),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[MOUNTSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.mountsnoop.ret */
    metrics[RET] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 4),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[MOUNTSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.mountsnoop.lat */
    metrics[LAT] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 5),
            .type  = PM_TYPE_32,
            .indom = indom_id_mapping[MOUNTSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0),
        }
    };
    /* bpf.mountsnoop.mnt_ns */
    metrics[MNT_NS] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 6),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[MOUNTSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.mountsnoop.fs */
    metrics[FS] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 7),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[MOUNTSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.mountsnoop.source */
    metrics[SOURCE] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 8),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[MOUNTSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.mountsnoop.target */
    metrics[TARGET] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 9),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[MOUNTSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.mountsnoop.data */
    metrics[DATA] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 10),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[MOUNTSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.mountsnoop.flags */
    metrics[FLAGS] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 11),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[MOUNTSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.mountsnoop.call */
    metrics[CALL] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 12),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[MOUNTSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.mountsnoop.lost */
    metrics[LOST] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 13),
            .type  = PM_TYPE_U32,
            .indom = PM_INDOM_NULL,
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
        }
    };

    /* MOUNTSNOOP_INDOM */
    indoms[MOUNTSNOOP_INDOM] = (struct pmdaIndom)
    {
        indom_id_mapping[MOUNTSNOOP_INDOM],
        env.process_count,
        mountsnoop_instances,
    };
}

static const char *strflags(__u64 flags)
{
    static char str[512];
    int i;

    if (!flags)
        return "0x0";

    str[0] = '\0';
    for (i = 0; i < flag_count; i++) {
        if (!((1 << i) & flags))
            continue;
        if (str[0])
            strcat(str, " | ");
        strcat(str, flag_names[i]);
    }
    return str;
}

static const char *strerrno(int errnum)
{
    const char *errstr;
    static char ret[32] = {};

    if (!errnum)
        return "0";

    ret[0] = '\0';
    errstr = strerrorname_np(-errnum);
    if (!errstr) {
        snprintf(ret, sizeof(ret), "%d", errnum);
        return ret;
    }

    snprintf(ret, sizeof(ret), "-%s", errstr);
    return ret;
}

static const char *gen_call(const struct event *e)
{
    static char call[10240];

    memset(call, 0, sizeof(call));
    if (e->op == UMOUNT) {
        snprintf(call, sizeof(call), "umount(\"%s\", %s) = %s",
                e->dest, strflags(e->flags), strerrno(e->ret));
    } else {
        snprintf(call, sizeof(call), "mount(\"%s\", \"%s\", \"%s\", %s, \"%s\") = %s",
                e->src, e->dest, e->fs, strflags(e->flags), e->data, strerrno(e->ret));
    }
    return call;
}

static int handle_event(void *ctx, void *data, size_t len)
{
    struct event *event = data;
    struct tailq_entry *elm = allocElm();

    elm->event.delta = event->delta;
    elm->event.flags = event->flags;
    elm->event.pid = event->pid;
    elm->event.tid = event->tid;
    elm->event.mnt_ns = event->mnt_ns;
    elm->event.ret = event->ret;
    elm->event.op = event->op;
    strncpy(elm->event.comm, event->comm, sizeof(event->comm));
    strncpy(elm->event.fs, event->fs, sizeof(event->fs));
    strncpy(elm->event.src, event->src, sizeof(event->src));
    strncpy(elm->event.dest, event->dest, sizeof(event->dest));
    strncpy(elm->event.data, event->data, sizeof(event->data));

    push(elm);
    return 0;
}

static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    lost_events += lost_cnt;
}

static int mountsnoop_init(dict *cfg, char *module_name)
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

    obj = mountsnoop_bpf__open_opts(&open_opts);
    if (!obj) {
        pmNotifyErr(LOG_ERR, "failed to open BPF object");
        return 1;
    }

    obj->rodata->target_pid = target_pid;

    buf = bpf_buffer__new(obj->maps.events, obj->maps.heap);
    if (!buf) {
        err = -errno;
        pmNotifyErr(LOG_ERR, "failed to create ring/perf buffer: %d", err);
        return err != 0;
    }

    err = mountsnoop_bpf__load(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to load BPF object: %d", err);
        return err != 0;
    }

    err = mountsnoop_bpf__attach(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to attach BPF programs: %d", err);
        return err != 0;
    }

    /* internal/external instance ids */
    fill_instids(env.process_count, &mountsnoop_instances);

    /* Initialize the tail queue. */
    TAILQ_INIT(&head);

    err = bpf_buffer__open(buf, handle_event, handle_lost_events, NULL);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to open ring/perf buffer: %d", err);
        return err != 0;
    }

    return err != 0;
}

static void mountsnoop_shutdown()
{
    struct tailq_entry *itemp;

    free(mountsnoop_instances);

    bpf_buffer__free(buf);
    mountsnoop_bpf__destroy(obj);
    /* Free the entire cache queue. */
    while ((itemp = TAILQ_FIRST(&head))) {
        TAILQ_REMOVE(&head, itemp, entries);
        free(itemp);
    }
}

static void mountsnoop_refresh(unsigned int item)
{
    bpf_buffer__poll(buf, PERF_POLL_TIMEOUT_MS);
}

static int mountsnoop_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    struct tailq_entry *value;
    static char *op_name[] = {
        [MOUNT] = "MOUNT",
        [UMOUNT] = "UMOUNT",
    };

    /* bpf.mountsnoop.lost */
    if (item == LOST) {
        atom->ul = lost_events;
        return PMDA_FETCH_STATIC;
    }

    if (inst == PM_IN_NULL) {
        return PM_ERR_INST;
    }

    if(!get_item(inst, &value))
        return PMDA_FETCH_NOVALUES;

    /* bpf.mountsnoop.pid */
    if (item == PID) {
        atom->ul = value->event.pid;
    }
    /* bpf.mountsnoop.tid */
    if (item == TID) {
        atom->ul = value->event.pid;
    }
    /* bpf.mountsnoop.comm */
    if (item == COMM) {
        atom->cp = value->event.comm;
    }
    /* bpf.mountsnoop.op */
    if (item == OP) {
        atom->cp = op_name[value->event.op];
    }
    /* bpf.mountsnoop.ret */
    if (item == RET) {
        atom->cp = (char*)strerrno(value->event.ret);
    }
    /* bpf.mountsnoop.lat */
    if (item == LAT) {
        atom->ull = value->event.delta;
    }
    /* bpf.mountsnoop.mnt_ns */
    if (item == MNT_NS) {
        atom->ul = value->event.mnt_ns;
    }
    /* bpf.mountsnoop.fs */
    if (item == FS) {
        atom->cp = value->event.fs;
    }
    /* bpf.mountsnoop.source */
    if (item == SOURCE) {
        atom->cp = value->event.src;
    }
    /* bpf.mountsnoop.target */
    if (item == TARGET) {
        atom->cp = value->event.dest;
    }
    /* bpf.mountsnoop.data */
    if (item == DATA) {
        atom->cp = value->event.data;
    }
    /* bpf.mountsnoop.flags */
    if (item == FLAGS) {
        atom->cp = (char*)strflags(value->event.flags);
    }
    /* bpf.mountsnoop.call */
    if (item == CALL) {
        atom->cp = (char*)gen_call(&value->event);
    }

    return PMDA_FETCH_STATIC;
}

struct module bpf_module = {
    .init               = mountsnoop_init,
    .register_metrics   = mountsnoop_register,
    .metric_count       = mountsnoop_metric_count,
    .indom_count        = mountsnoop_indom_count,
    .set_indom_serial   = mountsnoop_set_indom_serial,
    .shutdown           = mountsnoop_shutdown,
    .refresh            = mountsnoop_refresh,
    .fetch_to_atom      = mountsnoop_fetch_to_atom,
    .metric_name        = mountsnoop_metric_name,
    .metric_text        = mountsnoop_metric_text,
};
