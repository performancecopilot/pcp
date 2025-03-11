/*
 * Copyright (c) 2022 Sohaib Mohamed <sohaib.amhmd@gmail.com>
 *
 * Based on the biosnoop(8):
 * https://github.com/iovisor/bcc/blob/master/libbpf-tools/biosnoop.c
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

#include "blk_types.h"
#include "biosnoop.h"
#include "biosnoop.skel.h"
#include "trace_helpers.h"

#define PERF_BUFFER_PAGES 16
#define PERF_POLL_TIMEOUT_MS 0

#define INDOM_COUNT 1

static struct env {
    char *disk;
    int duration;
    bool timestamp;
    bool queued;
    bool verbose;
    char *cgroupspath;
    bool cg;
    int process_count;
} env = {
    .process_count = 20,
    .queued = false,
};

static pmdaInstid *biosnoop_instances;
static struct biosnoop_bpf *obj;
static struct perf_buffer *pb = NULL;
static struct ksyms *ksyms = NULL;
static struct partitions *partitions;
static int cgfd = -1;
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
enum metric_name { COMM, PID, DISK, RWBS, SECTOR, BYTES, LAT, LOST };
enum metric_indom { BIOSNOOP_INDOM };

char* metric_names[METRIC_COUNT] = {
    [COMM]    =  "biosnoop.comm",
    [PID]     =  "biosnoop.pid",
    [DISK]    =  "biosnoop.disk",
    [RWBS]    =  "biosnoop.rwbs",
    [SECTOR]  =  "biosnoop.sector",
    [BYTES]   =  "biosnoop.bytes",
    [LAT]     =  "biosnoop.lat",
    [LOST]    =  "biosnoop.lost",
};

char* metric_text_oneline[METRIC_COUNT] = {
    [COMM]    =  "Command name",
    [PID]     =  "Process identifier",
    [DISK]    =  "Disk name",
    [RWBS]    =  "Operations",
    [SECTOR]  =  "Sector",
    [BYTES]   =  "Bytes",
    [LAT]     =  "Latency",
    [LOST]    =  "Number of the lost events",
};

char* metric_text_long[METRIC_COUNT] = {
    [COMM]    =  "Command name",
    [PID]     =  "Process identifier",
    [DISK]    =  "Disk name",
    [RWBS]    =  "Operations",
    [SECTOR]  =  "Sector",
    [BYTES]   =  "Bytes",
    [LAT]     =  "Latency",
    [LOST]    =  "Number of the lost events",
};

static unsigned int biosnoop_metric_count(void)
{
    return METRIC_COUNT;
}

static char* biosnoop_metric_name(unsigned int metric)
{
    return metric_names[metric];
}

static unsigned int biosnoop_indom_count(void)
{
    return INDOM_COUNT;
}

static void biosnoop_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

static int biosnoop_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}

static void biosnoop_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    /* bpf.biosnoop.comm */
    metrics[COMM] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 0),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[BIOSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.biosnoop.pid */
    metrics[PID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 1),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[BIOSNOOP_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.biosnoop.disk */
    metrics[DISK] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 2),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[BIOSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.biosnoop.rwbs */
    metrics[RWBS] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 3),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[BIOSNOOP_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.biosnoop.sector */
    metrics[SECTOR] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 4),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[BIOSNOOP_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.biosnoop.bytes */
    metrics[BYTES] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 5),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[BIOSNOOP_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.biosnoop.lat */
    metrics[LAT] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 6),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[BIOSNOOP_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.biosnoop.lost */
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

    /* BIOSNOOP_INDOM */
    indoms[BIOSNOOP_INDOM] = (struct pmdaIndom)
    {
        indom_id_mapping[BIOSNOOP_INDOM],
        env.process_count,
        biosnoop_instances,
    };
}

static void blk_fill_rwbs(char *rwbs, unsigned int op)
{
    int i = 0;

    if (op & REQ_PREFLUSH)
        rwbs[i++] = 'F';

    switch (op & REQ_OP_MASK) {
        case REQ_OP_WRITE:
        case REQ_OP_WRITE_SAME:
            rwbs[i++] = 'W';
            break;
        case REQ_OP_DISCARD:
            rwbs[i++] = 'D';
            break;
        case REQ_OP_SECURE_ERASE:
            rwbs[i++] = 'D';
            rwbs[i++] = 'E';
            break;
        case REQ_OP_FLUSH:
            rwbs[i++] = 'F';
            break;
        case REQ_OP_READ:
            rwbs[i++] = 'R';
            break;
        default:
            rwbs[i++] = 'N';
    }

    if (op & REQ_FUA)
        rwbs[i++] = 'F';
    if (op & REQ_RAHEAD)
        rwbs[i++] = 'A';
    if (op & REQ_SYNC)
        rwbs[i++] = 'S';
    if (op & REQ_META)
        rwbs[i++] = 'M';

    rwbs[i] = '\0';
}

static void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
    struct event *event = data;

    struct tailq_entry *elm = allocElm();

    elm->event.delta = event->delta;
    elm->event.ts = event->ts;
    elm->event.sector = event->sector;
    elm->event.len = event->len;
    elm->event.pid = event->pid;
    elm->event.cmd_flags = event->cmd_flags;
    elm->event.dev = event->dev;
    pmstrncpy(elm->event.comm, sizeof(elm->event.comm), event->comm);

    push(elm);
}

static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    lost_events += lost_cnt;
}

static int biosnoop_init(dict *cfg, char *module_name)
{
    const struct partition *partition;
    int err;
    char *val;
    int idx, cg_map_fd;

    if ((val = pmIniFileLookup(cfg, module_name, "process_count")))
        env.process_count = atoi(val);
    if ((val = pmIniFileLookup(cfg, module_name, "disk")))
        env.disk = val;
    if ((val = pmIniFileLookup(cfg, module_name, "cgroup"))) {
        env.cg = true;
        env.cgroupspath = val;
    }

    obj = biosnoop_bpf__open();
    if (!obj) {
        pmNotifyErr(LOG_ERR, "failed to open BPF object");
        return 1;
    }

    partitions = partitions__load();
    if (!partitions) {
        pmNotifyErr(LOG_ERR, "failed to load partitions info");
        return 1;
    }

    /* initialize global data (filtering options) */
    if (env.disk) {
        partition = partitions__get_by_name(partitions, env.disk);
        if (!partition) {
            pmNotifyErr(LOG_ERR, "invaild partition name: not exist");
            return 1;
        }
        obj->rodata->filter_dev = true;
        obj->rodata->targ_dev = partition->dev;
    }
    obj->rodata->targ_queued = env.queued;
    obj->rodata->filter_cg = env.cg;

    if (fentry_can_attach("blk_account_io_start", NULL))
        bpf_program__set_attach_target(obj->progs.blk_account_io_start, 0,
                                       "blk_account_io_start");
    else
        bpf_program__set_attach_target(obj->progs.blk_account_io_start, 0,
                                       "__blk_account_io_start");

    err = biosnoop_bpf__load(obj);
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
            return 1;
        }
        if (bpf_map_update_elem(cg_map_fd, &idx, &cgfd, BPF_ANY)) {
            pmNotifyErr(LOG_ERR, "Failed adding target cgroup to map");
            return 1;
        }
    }

    obj->links.blk_account_io_start = bpf_program__attach(obj->progs.blk_account_io_start);
    if (!obj->links.blk_account_io_start) {
        err = -errno;
        pmNotifyErr(LOG_ERR, "failed to attach blk_account_io_start: %s",
                strerror(-err));
        return err != 0;
    }
    ksyms = ksyms__load();
    if (!ksyms) {
        err = -ENOMEM;
        pmNotifyErr(LOG_ERR, "failed to load kallsyms");
        return err != 0;
    }
    if (ksyms__get_symbol(ksyms, "blk_account_io_merge_bio")) {
        obj->links.blk_account_io_merge_bio =
            bpf_program__attach(obj->progs.blk_account_io_merge_bio);
        if (!obj->links.blk_account_io_merge_bio) {
            err = -errno;
            pmNotifyErr(LOG_ERR, "failed to attach blk_account_io_merge_bio: %s",
                    strerror(-err));
            return err != 0;
        }
    }
    if (env.queued) {
        obj->links.block_rq_insert =
            bpf_program__attach(obj->progs.block_rq_insert);
        if (!obj->links.block_rq_insert) {
            err = -errno;
            pmNotifyErr(LOG_ERR, "failed to attach block_rq_insert: %s", strerror(-err));
            return err != 0;
        }
    }
    obj->links.block_rq_issue = bpf_program__attach(obj->progs.block_rq_issue);
    if (!obj->links.block_rq_issue) {
        err = -errno;
        pmNotifyErr(LOG_ERR, "failed to attach block_rq_issue: %s", strerror(-err));
        return err != 0;
    }
    obj->links.block_rq_complete = bpf_program__attach(obj->progs.block_rq_complete);
    if (!obj->links.block_rq_complete) {
        err = -errno;
        pmNotifyErr(LOG_ERR, "failed to attach block_rq_complete: %s", strerror(-err));
        return err != 0;
    }

    /* internal/external instance ids */
    fill_instids(env.process_count, &biosnoop_instances);

    /* Initialize the tail queue. */
    TAILQ_INIT(&head);

    pb = perf_buffer__new(bpf_map__fd(obj->maps.events), PERF_BUFFER_PAGES,
            handle_event, handle_lost_events, NULL, NULL);
    if (!pb) {
        err = -errno;
        fprintf(stderr, "failed to open perf buffer: %d\n", err);
        return err != 0;
    }

    return err != 0;
}

static void biosnoop_shutdown()
{
    struct tailq_entry *itemp;

    free(biosnoop_instances);
    perf_buffer__free(pb);
    biosnoop_bpf__destroy(obj);
    ksyms__free(ksyms);
    partitions__free(partitions);
    if (cgfd > 0)
        close(cgfd);
    /* Free the entire cache queue. */
    while ((itemp = TAILQ_FIRST(&head))) {
        TAILQ_REMOVE(&head, itemp, entries);
        free(itemp);
    }
}

static void biosnoop_refresh(unsigned int item)
{
    perf_buffer__poll(pb, PERF_POLL_TIMEOUT_MS);
}

static int biosnoop_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    const struct partition *partition;
    char rwbs[RWBS_LEN];
    struct tailq_entry *value;

    /* bpf.biosnoop.lost */
    if (item == LOST) {
        atom->ul = lost_events;
        return PMDA_FETCH_STATIC;
    }

    if (inst == PM_IN_NULL) {
        return PM_ERR_INST;
    }

    if(!get_item(inst, &value))
        return PMDA_FETCH_NOVALUES;

    /* bpf.biosnoop.comm */
    if (item == COMM) {
        atom->cp = value->event.comm;
    }
    /* bpf.biosnoop.pid */
    if (item == PID) {
        atom->ul = value->event.pid;
    }
    /* bpf.biosnoop.disk */
    if (item == DISK) {
        partition = partitions__get_by_dev(partitions, value->event.dev);
        if (partition)
            atom->cp = partition->name;
        else
            strcpy(atom->cp, "Unknown");
    }
    /* bpf.biosnoop.rwbs */
    if (item == RWBS) {
        blk_fill_rwbs(rwbs, value->event.cmd_flags);
        atom->cp = rwbs;
    }
    /* bpf.biosnoop.sector */
    if (item == SECTOR) {
        atom->ull = value->event.sector;
    }
    /* bpf.biosnoop.bytes */
    if (item == BYTES) {
        atom->ul = value->event.len;
    }
    /* bpf.biosnoop.lat */
    if (item == LAT) {
        atom->ull =  value->event.delta;
    }

    return PMDA_FETCH_STATIC;
}

struct module bpf_module = {
    .init               = biosnoop_init,
    .register_metrics   = biosnoop_register,
    .metric_count       = biosnoop_metric_count,
    .indom_count        = biosnoop_indom_count,
    .set_indom_serial   = biosnoop_set_indom_serial,
    .shutdown           = biosnoop_shutdown,
    .refresh            = biosnoop_refresh,
    .fetch_to_atom      = biosnoop_fetch_to_atom,
    .metric_name        = biosnoop_metric_name,
    .metric_text        = biosnoop_metric_text,
};
