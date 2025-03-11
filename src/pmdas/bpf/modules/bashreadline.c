/*
 *
 * Copyright (c) 2022 Sohaib Mohamed <sohaib.amhmd@gmail.com>
 *
 * Based on the bashreadline(8):
 * https://github.com/iovisor/bcc/blob/master/libbpf-tools/bashreadline.c
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
#include <sys/queue.h>

#include "bashreadline.h"
#include "bashreadline.skel.h"
#include "btf_helpers.h"
#include "uprobe_helpers.h"

#define PERF_BUFFER_PAGES 16
#define PERF_POLL_TIMEOUT_MS 0
#define INDOM_COUNT 1

static struct env {
    int process_count;
} env = {
    .process_count = 20,
};

static char *libreadline_path = NULL;

static pmdaInstid *bashreadline_instances;
static struct bashreadline_bpf *obj;
static struct perf_buffer *pb = NULL;
static char *readline_so_path;
static int lost_events;
static int queuelength;

/* cache array */
struct tailq_entry {
    struct str_t str_t;
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

#define METRIC_COUNT 3
enum metric_name { COMM, PID, LOST };
enum metric_indom { BASHREADLINE_INDOM };

char* metric_names[METRIC_COUNT] = {
    [COMM] = "bashreadline.comm",
    [PID]  = "bashreadline.pid",
    [LOST] = "bashreadline.lost",
};

char* metric_text_oneline[METRIC_COUNT] = {
    [COMM] = "Command name",
    [PID]  = "Process identifier",
    [LOST] = "Number of the lost events",
};

char* metric_text_long[METRIC_COUNT] = {
    [COMM] = "Command name",
    [PID]  = "Process identifier",
    [LOST] = "Number of the lost events",
};

static unsigned int bashreadline_metric_count(void)
{
    return METRIC_COUNT;
}

static char* bashreadline_metric_name(unsigned int metric)
{
    return metric_names[metric];
}

static unsigned int bashreadline_indom_count(void)
{
    return INDOM_COUNT;
}

static void bashreadline_set_indom_serial(unsigned int local_indom_id, unsigned int global_id)
{
    indom_id_mapping[local_indom_id] = global_id;
}

static int bashreadline_metric_text(int item, int type, char **buffer)
{
    if (type & PM_TEXT_ONELINE) {
        *buffer = metric_text_oneline[item];
    } else {
        *buffer = metric_text_long[item];
    }

    return 0;
}

static void bashreadline_register(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms)
{
    /* bpf.bashreadline.comm */
    metrics[COMM] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 0),
            .type  = PM_TYPE_STRING,
            .indom = indom_id_mapping[BASHREADLINE_INDOM],
            .sem   = PM_SEM_INSTANT,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.bashreadline.pid */
    metrics[PID] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 1),
            .type  = PM_TYPE_U32,
            .indom = indom_id_mapping[BASHREADLINE_INDOM],
            .sem   = PM_SEM_DISCRETE,
            .units = PMDA_PMUNITS(0, 0, 0, 0, 0, 0),
        }
    };
    /* bpf.bashreadline.lost */
    metrics[LOST] = (struct pmdaMetric)
    {
        .m_desc = {
            .pmid  = PMDA_PMID(cluster_id, 2),
            .type  = PM_TYPE_U32,
            .indom = PM_INDOM_NULL,
            .sem   = PM_SEM_COUNTER,
            .units = PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
        }
    };

    /* BASHREADLINE_INDOM */
    indoms[BASHREADLINE_INDOM] = (struct pmdaIndom)
    {
        indom_id_mapping[BASHREADLINE_INDOM],
        env.process_count,
        bashreadline_instances,
    };
}

static void handle_event(void *ctx, int cpu, void *data, __u32 data_size)
{
    struct str_t *str_t = data;
    struct tailq_entry *elm = allocElm();

    elm->str_t.pid = str_t->pid;
    pmstrncpy(elm->str_t.str, sizeof(elm->str_t.str), str_t->str);

    push(elm);
}

static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    lost_events += lost_cnt;
}

static char *find_readline_so()
{
    const char *bash_path = "/bin/bash";
    FILE *fp;
    off_t func_off;
    char *line = NULL;
    size_t line_sz = 0;
    char path[128];
    char *result = NULL;

    func_off = get_elf_func_offset(bash_path, "readline");

    if (func_off >= 0)
        return strdup(bash_path);

    /*
     * Try to find libreadline.so if readline is not defined in
     * bash itself.
     *
     * ldd will print a list of names of shared objects,
     * dependencies, and their paths.  The line for libreadline
     * would looks like
     *
     *      libreadline.so.8 => /usr/lib/libreadline.so.8 (0x00007b....)
     *
     * Here, it finds a line with libreadline.so and extracts the
     * path after the arrow, '=>', symbol.
     */
    fp = popen("ldd /bin/bash", "r");
    if (fp == NULL)
        goto cleanup;
    while (getline(&line, &line_sz, fp) >= 0) {
        if (sscanf(line, "%*s => %127s", path) < 1)
            continue;
        if (strstr(line, "/libreadline.so")) {
            result = strdup(path);
            break;
        }
    }

cleanup:
    if (line)
        free(line);
    if (fp)
        pclose(fp);
    return result;
}

static int bashreadline_init(dict *cfg, char *module_name)
{
    LIBBPF_OPTS(bpf_object_open_opts, open_opts);
    off_t func_off;
    int err;
    char *val;

    if ((val = pmIniFileLookup(cfg, module_name, "process_count")))
        env.process_count = atoi(val);

    if (libreadline_path) {
        readline_so_path = libreadline_path;
    } else if ((readline_so_path = find_readline_so()) == NULL) {
        pmNotifyErr(LOG_ERR, "failed to find readline");
        return 1;
    }

    err = ensure_core_btf(&open_opts);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to fetch necessary BTF for CO-RE: %s", strerror(-err));
        return err;
    }

    obj = bashreadline_bpf__open_opts(&open_opts);
    if (!obj) {
        pmNotifyErr(LOG_ERR, "failed to open BPF object");
        return 1;
    }

    err = bashreadline_bpf__load(obj);
    if (err) {
        pmNotifyErr(LOG_ERR, "failed to load BPF object: %d", err);
        return err;
    }

    func_off = get_elf_func_offset(readline_so_path, "readline");
    if (func_off < 0) {
        pmNotifyErr(LOG_ERR, "cound not find readline in %s", readline_so_path);
        return func_off;
    }

    obj->links.printret = bpf_program__attach_uprobe(obj->progs.printret, true,
            -1, readline_so_path, func_off);
    if (!obj->links.printret) {
        err = -errno;
        pmNotifyErr(LOG_ERR, "failed to attach readline: %d", err);
        return err;
    }

    /* internal/external instance ids */
    fill_instids(env.process_count, &bashreadline_instances);

    /* Initialize the tail queue. */
    TAILQ_INIT(&head);

    pb = perf_buffer__new(bpf_map__fd(obj->maps.events), PERF_BUFFER_PAGES,
            handle_event, handle_lost_events, NULL, NULL);
    if (!pb) {
        err = -errno;
        pmNotifyErr(LOG_ERR, "failed to open perf buffer: %d", err);
        return err;
    }

    return err != 0;
}

static void bashreadline_shutdown()
{
    struct tailq_entry *itemp;

    free(bashreadline_instances);

    if (readline_so_path)
        free(readline_so_path);
    perf_buffer__free(pb);
    bashreadline_bpf__destroy(obj);
    /* Free the entire cache queue. */
    while ((itemp = TAILQ_FIRST(&head))) {
        TAILQ_REMOVE(&head, itemp, entries);
        free(itemp);
    }
}

static void bashreadline_refresh(unsigned int item)
{
    perf_buffer__poll(pb, PERF_POLL_TIMEOUT_MS);
}

static int bashreadline_fetch_to_atom(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    struct tailq_entry *value;

    /* bpf.bashreadline.lost */
    if (item == LOST) {
        atom->ul = lost_events;
        return PMDA_FETCH_STATIC;
    }

    if (inst == PM_IN_NULL) {
        return PM_ERR_INST;
    }

    if(!get_item(inst, &value))
        return PMDA_FETCH_NOVALUES;

    /* bpf.bashreadline.comm */
    if (item == COMM) {
        atom->cp = value->str_t.str;
    }
    /* bpf.bashreadline.pid */
    if (item == PID) {
        atom->ul = value->str_t.pid;
    }

    return PMDA_FETCH_STATIC;
}

struct module bpf_module = {
    .init               = bashreadline_init,
    .register_metrics   = bashreadline_register,
    .metric_count       = bashreadline_metric_count,
    .indom_count        = bashreadline_indom_count,
    .set_indom_serial   = bashreadline_set_indom_serial,
    .shutdown           = bashreadline_shutdown,
    .refresh            = bashreadline_refresh,
    .fetch_to_atom      = bashreadline_fetch_to_atom,
    .metric_name        = bashreadline_metric_name,
    .metric_text        = bashreadline_metric_text,
};
