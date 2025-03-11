/*
 *
 * Copyright (c) 2022 Sohaib Mohamed <sohaib.amhmd@gmail.com>
 *
 * Based on the oomkill(8):
 * https://github.com/iovisor/bcc/blob/master/libbpf-tools/oomkill.c
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
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

#include "oomkill.h"
#include "oomkill.skel.h"
#include "btf_helpers.h"
#include "compat.h"

#define PERF_POLL_TIMEOUT_MS 0

static struct env {
    int process_count;
} env = {
    .process_count = 20,
};

static pmdaInstid *oomkill_instances;
static struct oomkill_bpf *obj;
static struct bpf_buffer *buf;
static int lost_events;
static int queuelength;

/* cache array */
struct tailq_entry {
    struct data_t data_t;
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

#define METRIC_COUNT 6
enum metric_name { FPID, FCOMM, TPID, TCOMM, PAGES, LOST };
enum metric_indom { OOMKILL_INDOM };

char* metric_names[METRIC_COUNT] = {
    [FPID]   =  "oomkill.fpid",
    [FCOMM]  =  "oomkill.fcomm",
    [TPID]   =  "oomkill.tpid",
    [TCOMM]  =  "oomkill.tcomm",
    [PAGES]  =  "oomkill.pages",
    [LOST]   =  "oomkill.lost",
};

char* metric_text_oneline[METRIC_COUNT] = {
    [FPID]   =  "Triggered by PID",
    [FCOMM]  =  "Triggered by COMM",
    [TPID]   =  "OOM kill of PID",
    [TCOMM]  =  "OOM kill of COMM",
    [PAGES]  =  "Pages",
    [LOST]   =  "Number of the lost events",
};

char* metric_text_long[METRIC_COUNT] = {
    [FPID]   =  "The process ID of the task that was running when another task was OOM killed.",
    [FCOMM]  =  "The process name of the task that was running when another task was OOM killed.",
    [TPID]   =  "The process ID of the target process that was OOM killed.",
    [TCOMM]  =  "The process name of the target process that was OOM killed.",
    [PAGES]  =  "pages requested",
    [LOST]   =  "Number of the lost events",
};

static unsigned int oomkill_metric_count(void)
{
    return METRIC_COUNT;
}

static char* oomkill_metric_name(unsigned int metric)
{
    return metric_names[metric];
}

static unsigned int oomkill_indom_count(void)
{
    return INDOM_COUNT;
}

static void oomkill_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

static int oomkill_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}

static void oomkill_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    /* bpf.oomkill.fpid */
    metrics[FPID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 0),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[OOMKILL_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.oomkill.fcomm */
    metrics[FCOMM] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 1),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[OOMKILL_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.oomkill.tpid */
    metrics[TPID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 2),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[OOMKILL_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.oomkill.tcomm */
    metrics[TCOMM] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 3),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[OOMKILL_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.oomkill.pages */
    metrics[PAGES] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 4),
            .type  = PM_TYPE_U64,
            .indom = indom_id_mapping[OOMKILL_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.oomkill.lost */
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

    /* OOMKILL_INDOM */
    indoms[OOMKILL_INDOM] = (struct pmdaIndom)
    {
        indom_id_mapping[OOMKILL_INDOM],
        env.process_count,
        oomkill_instances,
    };
}

static int handle_event(void *ctx, void *data, size_t len)
{
    struct data_t *data_t = data;
    struct tailq_entry *elm = allocElm();

    elm->data_t.fpid = data_t->fpid;
    elm->data_t.tpid = data_t->tpid;
    elm->data_t.pages = data_t->pages;
    pmstrncpy(elm->data_t.fcomm, sizeof(elm->data_t.fcomm), data_t->fcomm);
    pmstrncpy(elm->data_t.tcomm, sizeof(elm->data_t.tcomm), data_t->tcomm);

    push(elm);
    return 0;
}

static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    lost_events += lost_cnt;
}

static int oomkill_init(dict *cfg, char *module_name)
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

    obj = oomkill_bpf__open_opts(&open_opts);
    if (!obj) {
        pmNotifyErr(LOG_ERR, "failed to load and open BPF object");
        return 1;
    }

    buf = bpf_buffer__new(obj->maps.events, obj->maps.heap);
    if (!buf) {
        err = -errno;
        pmNotifyErr(LOG_ERR, "failed to create ring/perf buffer: %d", err);
        return err;
    }

    err = oomkill_bpf__load(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to load BPF object: %d", err);
        return err;
    }

    err = oomkill_bpf__attach(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to attach BPF programs");
        return err;
    }

    /* internal/external instance ids */
    fill_instids(env.process_count, &oomkill_instances);

    /* Initialize the tail queue. */
    TAILQ_INIT(&head);

    err = bpf_buffer__open(buf, handle_event, handle_lost_events, NULL);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to open ring/perf buffer: %d", err);
        return err;
    }

    return err != 0;
}

static void oomkill_shutdown()
{
    struct tailq_entry *itemp;

    free(oomkill_instances);
    bpf_buffer__free(buf);
    oomkill_bpf__destroy(obj);
    /* Free the entire cache queue. */
    while ((itemp = TAILQ_FIRST(&head))) {
        TAILQ_REMOVE(&head, itemp, entries);
        free(itemp);
    }
}

static void oomkill_refresh(unsigned int item)
{
    bpf_buffer__poll(buf, PERF_POLL_TIMEOUT_MS);
}

static int oomkill_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    struct tailq_entry *value;

    /* bpf.oomkill.lost */
    if (item == LOST) {
        atom->ul = lost_events;
        return PMDA_FETCH_STATIC;
    }

    if (inst == PM_IN_NULL) {
        return PM_ERR_INST;
    }

    if(!get_item(inst, &value))
        return PMDA_FETCH_NOVALUES;

    /* bpf.oomkill.fpid */
    if (item == FPID) {
        atom->ul = value->data_t.fpid;
    }
    /* bpf.oomkill.fcomm */
    if (item == FCOMM) {
        atom->cp = value->data_t.fcomm;
    }
    /* bpf.oomkill.tpid */
    if (item == TPID) {
        atom->ul = value->data_t.tpid;
    }
    /* bpf.oomkill.tcomm */
    if (item == TCOMM) {
        atom->cp = value->data_t.tcomm;
    }
    /* bpf.oomkill.pages */
    if (item == PAGES) {
        atom->ull = value->data_t.pages;
    }

    return PMDA_FETCH_STATIC;
}

struct module bpf_module = {
    .init               = oomkill_init,
    .register_metrics   = oomkill_register,
    .metric_count       = oomkill_metric_count,
    .indom_count        = oomkill_indom_count,
    .set_indom_serial   = oomkill_set_indom_serial,
    .shutdown           = oomkill_shutdown,
    .refresh            = oomkill_refresh,
    .fetch_to_atom      = oomkill_fetch_to_atom,
    .metric_name        = oomkill_metric_name,
    .metric_text        = oomkill_metric_text,
};
