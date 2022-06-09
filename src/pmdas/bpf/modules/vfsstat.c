/*
 *
 * Copyright (c) 2022 Sohaib Mohamed <sohaib.amhmd@gmail.com>
 *
 * Based on the vfsstat(8):
 * https://github.com/iovisor/bcc/blob/master/libbpf-tools/vfsstat.c
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

#include "vfsstat.h"
#include "vfsstat.skel.h"
#include "btf_helpers.h"
#include "trace_helpers.h"

static struct env {
    int count;
    int interval_sec;
    int process_count;
} env = {
    .interval_sec = 1,	/* once a second */
    .process_count = 20,
};

static pmdaInstid *vfsstat_instances;
static struct vfsstat_bpf *skel;
static int queuelength;

struct event {
    __u64 read;
    __u64 write;
    __u64 fsync;
    __u64 open;
    __u64 create;
};

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

#define METRIC_COUNT 5
enum metric_name { READ, WRITE, FSYNC, OPEN, CREATE };
enum metric_indom { VFSSTAT_INDOM };

char* metric_names[METRIC_COUNT] = {
    [READ]    =  "vfsstat.read",
    [WRITE]   =  "vfsstat.write",
    [FSYNC]   =  "vfsstat.fsync",
    [OPEN]    =  "vfsstat.open",
    [CREATE]  =  "vfsstat.create",
};

char* metric_text_oneline[METRIC_COUNT] = {
    [READ]    =  "Count of read() per second",
    [WRITE]   =  "Count of write() per second",
    [FSYNC]   =  "Count of fsync() per second",
    [OPEN]    =  "Count of open() per second",
    [CREATE]  =  "Count of create() per second",
};

char* metric_text_long[METRIC_COUNT] = {
    [READ]    =  "Count of read() per second",
    [WRITE]   =  "Count of write() per second",
    [FSYNC]   =  "Count of fsync() per second",
    [OPEN]    =  "Count of open() per second",
    [CREATE]  =  "Count of create() per second",
};

static unsigned int vfsstat_metric_count(void)
{
    return METRIC_COUNT;
}

static char* vfsstat_metric_name(unsigned int metric)
{
    return metric_names[metric];
}

static unsigned int vfsstat_indom_count(void)
{
    return INDOM_COUNT;
}

static void vfsstat_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

static int vfsstat_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}

static void vfsstat_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    /* bpf.vfsstat.read */
    metrics[READ] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 0),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[VFSSTAT_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.vfsstat.write */
    metrics[WRITE] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 1),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[VFSSTAT_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.vfsstat.fsync */
    metrics[FSYNC] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 2),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[VFSSTAT_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.vfsstat.open */
    metrics[OPEN] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 3),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[VFSSTAT_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.vfsstat.create */
    metrics[CREATE] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 4),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[VFSSTAT_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };

    /* VFSSTAT_INDOM */
    indoms[0] = (struct pmdaIndom)
    {
        indom_id_mapping[VFSSTAT_INDOM],
        env.process_count,
        vfsstat_instances,
    };

}

static void fill_and_reset_stats(__u64 stats[S_MAXSTAT])
{
    struct tailq_entry *elm = allocElm();
    __u64 val;

    for (int i = 0; i < S_MAXSTAT; i++) {
        val = __atomic_exchange_n(&stats[i], 0, __ATOMIC_RELAXED);

        val = val / env.interval_sec;
        if (i == READ)
            elm->event.read = val;
        if (i == WRITE)
            elm->event.write = val;
        if (i == FSYNC)
            elm->event.fsync = val;
        if (i == OPEN)
            elm->event.open = val;
        if (i == CREATE)
            elm->event.create = val;
    }

    push(elm);
}

static int vfsstat_init(dict *cfg, char *module_name)
{
    LIBBPF_OPTS(bpf_object_open_opts, open_opts);
    int err;
    char *val;

    if ((val = pmIniFileLookup(cfg, module_name, "process_count")))
        env.process_count = atoi(val);


    err = ensure_core_btf(&open_opts);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to fetch necessary BTF for CO-RE: %s", strerror(-err));
        return 1;
    }

    skel = vfsstat_bpf__open();
    if (!skel) {
        pmNotifyErr(LOG_ERR, "failed to open BPF skelect");
        return 1;
    }

    /* It fallbacks to kprobes when kernel does not support fentry. */
    if (vmlinux_btf_exists() && fentry_can_attach("vfs_read", NULL)) {
        bpf_program__set_autoload(skel->progs.kprobe_vfs_read, false);
        bpf_program__set_autoload(skel->progs.kprobe_vfs_write, false);
        bpf_program__set_autoload(skel->progs.kprobe_vfs_fsync, false);
        bpf_program__set_autoload(skel->progs.kprobe_vfs_open, false);
        bpf_program__set_autoload(skel->progs.kprobe_vfs_create, false);
    } else {
        bpf_program__set_autoload(skel->progs.fentry_vfs_read, false);
        bpf_program__set_autoload(skel->progs.fentry_vfs_write, false);
        bpf_program__set_autoload(skel->progs.fentry_vfs_fsync, false);
        bpf_program__set_autoload(skel->progs.fentry_vfs_open, false);
        bpf_program__set_autoload(skel->progs.fentry_vfs_create, false);
    }

    err = vfsstat_bpf__load(skel);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to load BPF skelect: %d", err);
        return err != 0;
    }

    if (!skel->bss) {
        pmNotifyErr(LOG_ERR, "Memory-mapping BPF maps is supported starting from Linux 5.7, please upgrade.");
        return err != 0;
    }

    err = vfsstat_bpf__attach(skel);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to attach BPF programs: %s",
                strerror(-err));
        return err != 0;
    }

    /* internal/external instance ids */
    fill_instids(env.process_count, &vfsstat_instances);

    /* Initialize the tail queue. */
    TAILQ_INIT(&head);

    return err != 0;
}

static void vfsstat_shutdown()
{
    struct tailq_entry *itemp;

    free(vfsstat_instances);
    vfsstat_bpf__destroy(skel);
    /* Free the entire cache queue. */
    while ((itemp = TAILQ_FIRST(&head))) {
        TAILQ_REMOVE(&head, itemp, entries);
        free(itemp);
    }
}

static void vfsstat_refresh(unsigned int item)
{
    fill_and_reset_stats(skel->bss->stats);
}

static int vfsstat_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    struct tailq_entry *value;

    if (inst == PM_IN_NULL) {
        return PM_ERR_INST;
    }

    if(!get_item(inst, &value))
        return PMDA_FETCH_NOVALUES;

    /* bpf.vfsstat.read */
    if (item == READ) {
        atom->ull = value->event.read;
    }
    /* bpf.vfsstat.write */
    if (item == WRITE) {
        atom->ull = value->event.write;
    }
    /* bpf.vfsstat.fsync */
    if (item == FSYNC) {
        atom->ull = value->event.fsync;
    }
    /* bpf.vfsstat.open */
    if (item == OPEN) {
        atom->ull = value->event.open;
    }
    /* bpf.vfsstat.create */
    if (item == CREATE) {
        atom->ull = value->event.create;
    }

    return PMDA_FETCH_STATIC;
}

struct module bpf_module = {
    .init               = vfsstat_init,
    .register_metrics   = vfsstat_register,
    .metric_count       = vfsstat_metric_count,
    .indom_count        = vfsstat_indom_count,
    .set_indom_serial   = vfsstat_set_indom_serial,
    .shutdown           = vfsstat_shutdown,
    .refresh            = vfsstat_refresh,
    .fetch_to_atom      = vfsstat_fetch_to_atom,
    .metric_name        = vfsstat_metric_name,
    .metric_text        = vfsstat_metric_text,
};

