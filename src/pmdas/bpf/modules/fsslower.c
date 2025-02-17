/*
 *
 * Copyright (c) 2022 Sohaib Mohamed <sohaib.amhmd@gmail.com>
 *
 * Based on the fsslower(8):
 * https://github.com/iovisor/bcc/blob/master/libbpf-tools/fsslower.c
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

#include <bpf/libbpf.h>
#include <bpf/bpf.h>

#include "fsslower.h"
#include "fsslower.skel.h"
#include "btf_helpers.h"
#include "trace_helpers.h"

#define PERF_BUFFER_PAGES 64
#define PERF_POLL_TIMEOUT_MS 0
#define INDOM_COUNT 1

static struct env {
    int process_count;
} env = {
    .process_count = 20,
};

enum fs_type {
    NONE,
    BTRFS,
    EXT4,
    NFS,
    XFS,
};

static struct fs_config {
    const char *fs;
    const char *op_funcs[F_MAX_OP];
} fs_configs[] = {
    [BTRFS] = { "btrfs", {
        [F_READ] = "btrfs_file_read_iter",
        [F_WRITE] = "btrfs_file_write_iter",
        [F_OPEN] = "btrfs_file_open",
        [F_FSYNC] = "btrfs_sync_file",
    }},
    [EXT4] = { "ext4", {
        [F_READ] = "ext4_file_read_iter",
        [F_WRITE] = "ext4_file_write_iter",
        [F_OPEN] = "ext4_file_open",
        [F_FSYNC] = "ext4_sync_file",
    }},
    [NFS] = { "nfs", {
        [F_READ] = "nfs_file_read",
        [F_WRITE] = "nfs_file_write",
        [F_OPEN] = "nfs_file_open",
        [F_FSYNC] = "nfs_file_fsync",
    }},
    [XFS] = { "xfs", {
        [F_READ] = "xfs_file_read_iter",
        [F_WRITE] = "xfs_file_write_iter",
        [F_OPEN] = "xfs_file_open",
        [F_FSYNC] = "xfs_file_fsync",
    }},
};

static char* file_op[] = {
    [F_READ] = "R",
    [F_WRITE] = "W",
    [F_OPEN] = "O",
    [F_FSYNC] = "F",
};

static enum fs_type fs_type;
static pid_t target_pid = 0;
static __u64 min_lat_ms = 10;

static pmdaInstid *fsslower_instances;
static struct fsslower_bpf *skel;
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
enum metric_name { COMM, PID, FILE_OP, BYTES, OFFSET, LAT, FILENAME, LOST };
enum metric_indom { FSSLOWER_INDOM };

char* metric_names[METRIC_COUNT] = {
    [COMM]      =  "fsslower.comm",
    [PID]       =  "fsslower.pid",
    [FILE_OP]   =  "fsslower.file_op",
    [BYTES]     =  "fsslower.bytes",
    [OFFSET]    =  "fsslower.offset",
    [LAT]       =  "fsslower.lat",
    [FILENAME]  =  "fsslower.filename",
    [LOST]      =  "fsslower.lost",
};

char* metric_text_oneline[METRIC_COUNT] = {
    [COMM]      =  "Command name",
    [PID]       =  "Process identifier",
    [FILE_OP]   =  "File operation",
    [BYTES]     =  "Bytes",
    [OFFSET]    =  "Offset",
    [LAT]       =  "Latency",
    [FILENAME]  =  "File Name",
    [LOST]      =  "Number of the lost events",
};

char* metric_text_long[METRIC_COUNT] = {
    [COMM]      =  "Command name",
    [PID]       =  "Process identifier",
    [FILE_OP]   =  "File operation",
    [BYTES]     =  "Bytes",
    [OFFSET]    =  "Offset",
    [LAT]       =  "Latency",
    [FILENAME]  =  "File Name",
    [LOST]      =  "Number of the lost events",
};

static unsigned int fsslower_metric_count(void)
{
    return METRIC_COUNT;
}

static char* fsslower_metric_name(unsigned int metric)
{
    return metric_names[metric];
}

static unsigned int fsslower_indom_count(void)
{
    return INDOM_COUNT;
}

static void fsslower_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

static int fsslower_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}

static void fsslower_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    /* bpf.fsslower.comm */
    metrics[COMM] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 0),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[FSSLOWER_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.fsslower.pid */
    metrics[PID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 1),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[FSSLOWER_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.fsslower.file_op */
    metrics[FILE_OP] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 2),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[FSSLOWER_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.fsslower.bytes */
    metrics[BYTES] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 3),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[FSSLOWER_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
        }
    };
    /* bpf.fsslower.offset */
    metrics[OFFSET] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 4),
            .type  = PM_TYPE_64,
            .indom = indom_id_mapping[FSSLOWER_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
        }
    };
    /* bpf.fsslower.lat */
    metrics[LAT] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 5),
            .type  = PM_TYPE_DOUBLE,
            .indom = indom_id_mapping[FSSLOWER_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0),
        }
    };
    /* bpf.fsslower.filename */
    metrics[FILENAME] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 6),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[FSSLOWER_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.fsslower.lost */
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

    /* FSSLOWER_INDOM */
    indoms[FSSLOWER_INDOM] = (struct pmdaIndom)
    {
        indom_id_mapping[FSSLOWER_INDOM],
        env.process_count,
        fsslower_instances,
    };
}

static bool check_fentry()
{
    int i;
    const char *fn_name, *mod;
    bool support_fentry = true;

    for (i = 0; i < F_MAX_OP; i++) {
        fn_name = fs_configs[fs_type].op_funcs[i];
        mod = fs_configs[fs_type].fs;
        if (fn_name && !fentry_can_attach(fn_name, mod)) {
            support_fentry = false;
            break;
        }
    }
    return support_fentry;
}

static int fentry_set_attach_target(struct fsslower_bpf *obj)
{
    struct fs_config *cfg = &fs_configs[fs_type];
    int err = 0;

    err = err ?: bpf_program__set_attach_target(obj->progs.file_read_fentry, 0, cfg->op_funcs[F_READ]);
    err = err ?: bpf_program__set_attach_target(obj->progs.file_read_fexit, 0, cfg->op_funcs[F_READ]);
    err = err ?: bpf_program__set_attach_target(obj->progs.file_write_fentry, 0, cfg->op_funcs[F_WRITE]);
    err = err ?: bpf_program__set_attach_target(obj->progs.file_write_fexit, 0, cfg->op_funcs[F_WRITE]);
    err = err ?: bpf_program__set_attach_target(obj->progs.file_open_fentry, 0, cfg->op_funcs[F_OPEN]);
    err = err ?: bpf_program__set_attach_target(obj->progs.file_open_fexit, 0, cfg->op_funcs[F_OPEN]);
    err = err ?: bpf_program__set_attach_target(obj->progs.file_sync_fentry, 0, cfg->op_funcs[F_FSYNC]);
    err = err ?: bpf_program__set_attach_target(obj->progs.file_sync_fexit, 0, cfg->op_funcs[F_FSYNC]);
    return err;
}

static void disable_fentry(struct fsslower_bpf *obj)
{
    bpf_program__set_autoload(obj->progs.file_read_fentry, false);
    bpf_program__set_autoload(obj->progs.file_read_fexit, false);
    bpf_program__set_autoload(obj->progs.file_write_fentry, false);
    bpf_program__set_autoload(obj->progs.file_write_fexit, false);
    bpf_program__set_autoload(obj->progs.file_open_fentry, false);
    bpf_program__set_autoload(obj->progs.file_open_fexit, false);
    bpf_program__set_autoload(obj->progs.file_sync_fentry, false);
    bpf_program__set_autoload(obj->progs.file_sync_fexit, false);
}

static void disable_kprobes(struct fsslower_bpf *obj)
{
    bpf_program__set_autoload(obj->progs.file_read_entry, false);
    bpf_program__set_autoload(obj->progs.file_read_exit, false);
    bpf_program__set_autoload(obj->progs.file_write_entry, false);
    bpf_program__set_autoload(obj->progs.file_write_exit, false);
    bpf_program__set_autoload(obj->progs.file_open_entry, false);
    bpf_program__set_autoload(obj->progs.file_open_exit, false);
    bpf_program__set_autoload(obj->progs.file_sync_entry, false);
    bpf_program__set_autoload(obj->progs.file_sync_exit, false);
}

static int attach_kprobes(struct fsslower_bpf *obj)
{
    long err = 0;
    struct fs_config *cfg = &fs_configs[fs_type];

    /* READ */
    obj->links.file_read_entry = bpf_program__attach_kprobe(obj->progs.file_read_entry, false, cfg->op_funcs[F_READ]);
    if (!obj->links.file_read_entry)
        goto errout;
    obj->links.file_read_exit = bpf_program__attach_kprobe(obj->progs.file_read_exit, true, cfg->op_funcs[F_READ]);
    if (!obj->links.file_read_exit)
        goto errout;
    /* WRITE */
    obj->links.file_write_entry = bpf_program__attach_kprobe(obj->progs.file_write_entry, false, cfg->op_funcs[F_WRITE]);
    if (!obj->links.file_write_entry)
        goto errout;
    obj->links.file_write_exit = bpf_program__attach_kprobe(obj->progs.file_write_exit, true, cfg->op_funcs[F_WRITE]);
    if (!obj->links.file_write_exit)
        goto errout;
    /* OPEN */
    obj->links.file_open_entry = bpf_program__attach_kprobe(obj->progs.file_open_entry, false, cfg->op_funcs[F_OPEN]);
    if (!obj->links.file_open_entry)
        goto errout;
    obj->links.file_open_exit = bpf_program__attach_kprobe(obj->progs.file_open_exit, true, cfg->op_funcs[F_OPEN]);
    if (!obj->links.file_open_exit)
        goto errout;
    /* FSYNC */
    obj->links.file_sync_entry = bpf_program__attach_kprobe(obj->progs.file_sync_entry, false, cfg->op_funcs[F_FSYNC]);
    if (!obj->links.file_sync_entry)
        goto errout;
    obj->links.file_sync_exit = bpf_program__attach_kprobe(obj->progs.file_sync_exit, true, cfg->op_funcs[F_FSYNC]);
    if (!obj->links.file_sync_exit)
        goto errout;
    return 0;

errout:
    err = -errno;
    pmNotifyErr(LOG_ERR, "failed to attach kprobe: %ld", err);
    return err;
}

static void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
    struct event * event = data;
    struct tailq_entry *elm = allocElm();

    elm->event.delta_us = event->delta_us;
    elm->event.end_ns = event->end_ns;
    elm->event.offset = event->offset;
    elm->event.size = event->size;
    elm->event.pid = event->pid;
    elm->event.op = event->op;
    pmstrncpy(elm->event.file, sizeof(elm->event.file), event->file);
    pmstrncpy(elm->event.task, sizeof(elm->event.task), event->task);

    push(elm);
}

static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    lost_events += lost_cnt;
}

static int fsslower_init(dict *cfg, char *module_name)
{
    LIBBPF_OPTS(bpf_object_open_opts, open_opts);
    int err;
    char *val;
    bool support_fentry = true;

    if ((val = pmIniFileLookup(cfg, module_name, "process_count")))
        env.process_count = atoi(val);
    if ((val = pmIniFileLookup(cfg, module_name, "pid")))
        target_pid = atoi(val);
    if ((val = pmIniFileLookup(cfg, module_name, "min")))
        min_lat_ms = atoi(val);
    if ((val = pmIniFileLookup(cfg, module_name, "filesystem"))) {
        if(!strcmp(val, "btrfs") || !strcmp(val, "BTRFS"))
                fs_type = BTRFS;
        else if(!strcmp(val, "ext4") || !strcmp(val, "EXT4"))
                fs_type = EXT4;
        else if(!strcmp(val, "nfs") || !strcmp(val, "NFS"))
                fs_type = NFS;
        else if(!strcmp(val, "xfs") || !strcmp(val, "XFS"))
                fs_type = XFS;
    }

    err = ensure_core_btf(&open_opts);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to fetch necessary BTF for CO-RE: %s", strerror(-err));
        return err != 0;
    }

    skel = fsslower_bpf__open_opts(&open_opts);
    if (!skel) {
        pmNotifyErr(LOG_ERR, "failed to open BPF object");
        return err != 0;
    }

    skel->rodata->target_pid = target_pid;
    skel->rodata->min_lat_ns = min_lat_ms * 1000 * 1000;

    /*
     * before load
     * if fentry is supported, we set attach target and disable kprobes
     * otherwise, we disable fentry and attach kprobes after loading
     */
    support_fentry = check_fentry();
    if (support_fentry) {
        err = fentry_set_attach_target(skel);
        if (err) {
            pmNotifyErr(LOG_ERR, "failed to set attach target: %d", err);
            return err != 0;
        }
        disable_kprobes(skel);
    } else {
        disable_fentry(skel);
    }

    err = fsslower_bpf__load(skel);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to load BPF object: %d", err);
        return err != 0;
    }

    /*
     * after load
     * if fentry is supported, let libbpf do auto load
     * otherwise, we attach to kprobes manually
     */
    err = support_fentry ? fsslower_bpf__attach(skel) : attach_kprobes(skel);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to attach BPF programs: %d", err);
        return err != 0;
    }

    /* internal/external instance ids */
    fill_instids(env.process_count, &fsslower_instances);

    /* Initialize the tail queue. */
    TAILQ_INIT(&head);

    pb = perf_buffer__new(bpf_map__fd(skel->maps.events), PERF_BUFFER_PAGES,
            handle_event, handle_lost_events, NULL, NULL);
    if (!pb) {
        err = -errno;
        pmNotifyErr(LOG_ERR, "failed to open perf buffer: %d", err);
        return err != 0;
    }

    return err != 0;
}

static void fsslower_shutdown()
{
    struct tailq_entry *itemp;

    free(fsslower_instances);
    perf_buffer__free(pb);
    fsslower_bpf__destroy(skel);
    /* Free the entire cache queue. */
    while ((itemp = TAILQ_FIRST(&head))) {
        TAILQ_REMOVE(&head, itemp, entries);
        free(itemp);
    }
}

static void fsslower_refresh(unsigned int item)
{
    perf_buffer__poll(pb, PERF_POLL_TIMEOUT_MS);
}

static int fsslower_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    struct tailq_entry *value;

    /* bpf.fsslower.lost */
    if (item == LOST) {
        atom->ul = lost_events;
        return PMDA_FETCH_STATIC;
    }

    if (inst == PM_IN_NULL) {
        return PM_ERR_INST;
    }

    if(!get_item(inst, &value))
        return PMDA_FETCH_NOVALUES;

    /* bpf.fsslower.comm */
    if (item == COMM) {
        atom->cp = value->event.task;
    }
    /* bpf.fsslower.pid */
    if (item == PID) {
        atom->ul = value->event.pid;
    }
    /* bpf.fsslower.file_op */
    if (item == FILE_OP) {
        atom->cp = file_op[value->event.op];
    }
    /* bpf.fsslower.bytes */
    if (item == BYTES) {
        atom->ul = value->event.size;
    }
    /* bpf.fsslower.offset */
    if (item == OFFSET) {
        atom->ll = value->event.offset;
    }
    /* bpf.fsslower.lat */
    if (item == LAT) {
        atom->d = (double)value->event.delta_us;
    }
    /* bpf.fsslower.comm */
    if (item == FILENAME) {
        atom->cp = value->event.file;
    }

    return PMDA_FETCH_STATIC;
}

struct module bpf_module = {
    .init               = fsslower_init,
    .register_metrics   = fsslower_register,
    .metric_count       = fsslower_metric_count,
    .indom_count        = fsslower_indom_count,
    .set_indom_serial   = fsslower_set_indom_serial,
    .shutdown           = fsslower_shutdown,
    .refresh            = fsslower_refresh,
    .fetch_to_atom      = fsslower_fetch_to_atom,
    .metric_name        = fsslower_metric_name,
    .metric_text        = fsslower_metric_text,
};

