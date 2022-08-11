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

#include "vfsstat.h"
#include "vfsstat.skel.h"
#include "btf_helpers.h"
#include "trace_helpers.h"

static pmdaInstid *vfsstat_instances;
static struct vfsstat_bpf *skel;

#define INDOM_COUNT 0
#define METRIC_COUNT 5
enum metric_name { READ, WRITE, FSYNC, OPEN, CREATE };

char* metric_names[METRIC_COUNT] = {
    [READ]    =  "vfsstat.read",
    [WRITE]   =  "vfsstat.write",
    [FSYNC]   =  "vfsstat.fsync",
    [OPEN]    =  "vfsstat.open",
    [CREATE]  =  "vfsstat.create",
};

char* metric_text_oneline[METRIC_COUNT] = {
    [READ]    =  "Count of read() calls",
    [WRITE]   =  "Count of write() calls",
    [FSYNC]   =  "Count of fsync() calls",
    [OPEN]    =  "Count of open() calls",
    [CREATE]  =  "Count of create() calls",
};

char* metric_text_long[METRIC_COUNT] = {
    [READ]    =  "Count of read() calls",
    [WRITE]   =  "Count of write() calls",
    [FSYNC]   =  "Count of fsync() calls",
    [OPEN]    =  "Count of open() calls",
    [CREATE]  =  "Count of create() calls",
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
            .indom = PM_INDOM_NULL,
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
        }
    };
    /* bpf.vfsstat.write */
    metrics[WRITE] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 1),
            .type  = PM_TYPE_U64,
            .indom = PM_INDOM_NULL,
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
        }
    };
    /* bpf.vfsstat.fsync */
    metrics[FSYNC] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 2),
            .type  = PM_TYPE_U64,
            .indom = PM_INDOM_NULL,
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
        }
    };
    /* bpf.vfsstat.open */
    metrics[OPEN] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 3),
            .type  = PM_TYPE_U64,
            .indom = PM_INDOM_NULL,
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
        }
    };
    /* bpf.vfsstat.create */
    metrics[CREATE] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 4),
            .type  = PM_TYPE_U64,
            .indom = PM_INDOM_NULL,
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
        }
    };
}

static int vfsstat_init(dict *cfg, char *module_name)
{
    LIBBPF_OPTS(bpf_object_open_opts, open_opts);
    int err;

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

    return err != 0;
}

static void vfsstat_shutdown()
{

    free(vfsstat_instances);
    vfsstat_bpf__destroy(skel);
}

static void vfsstat_refresh(unsigned int item)
{
}

static int vfsstat_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    __u64 *stats = skel->bss->stats;

    /* bpf.vfsstat.read */
    if (item == READ) {
        atom->ull = stats[S_READ];
    }
    /* bpf.vfsstat.write */
    if (item == WRITE) {
        atom->ull = stats[S_WRITE];
    }
    /* bpf.vfsstat.fsync */
    if (item == FSYNC) {
        atom->ull = stats[S_FSYNC];
    }
    /* bpf.vfsstat.open */
    if (item == OPEN) {
        atom->ull = stats[S_OPEN];
    }
    /* bpf.vfsstat.create */
    if (item == CREATE) {
        atom->ull = stats[S_CREATE];
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

