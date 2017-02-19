/*
 * Copyright (C) 2017 IBM Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <malloc.h>
#include <stdlib.h>

#include "parse_events.h"
#include "perfinterface.h"

static void cleanup_property(struct property *prop)
{
    if (!prop)
        return;
    if (prop->name)
        free(prop->name);
    free(prop);
}

static void cleanup_property_list(struct property *prop)
{
    struct property *prop_del, *curr_prop;

    if (!prop)
        return;
    for (curr_prop = prop; curr_prop; curr_prop = prop_del) {
        prop_del = curr_prop->next;
        cleanup_property(curr_prop);
    }
}

static void cleanup_event(struct pmu_event *event)
{
    if (!event)
        return;
    if (event->name)
        free(event->name);
    free(event);
}

static void cleanup_event_list(struct pmu_event *event)
{
    struct pmu_event *event_del, *curr_ev;

    if (!event)
        return;
    for (curr_ev = event; curr_ev; curr_ev = event_del) {
        event_del = curr_ev->next;
        cleanup_event(curr_ev);
    }
}

static void cleanup_pmu(struct pmu *pmu)
{
    if (!pmu)
        return;

    cleanup_event_list(pmu->ev);
    cleanup_property_list(pmu->prop);

    /* Now, clean up the pmu */
    if (pmu->name)
        free(pmu->name);
    free(pmu);
}

static void cleanup_pmu_list(struct pmu *pmu)
{
    struct pmu *pmu_del, *curr_pmu;

    if (!pmu)
        return;

    for (curr_pmu = pmu; curr_pmu; curr_pmu = pmu_del) {
        pmu_del = curr_pmu->next;
        cleanup_pmu(pmu_del);
    }
}

/*
 * We need to statically set these up since, these software
 * events are not exposed via the kernel /sys/devices/
 * interface. However, these events are defined in
 * linux/perf_event.h.
 */
static struct software_event sw_events[] =
{
    {
        .name = "cpu-clock",
        .config = PERF_COUNT_SW_CPU_CLOCK,
    },
    {
        .name = "task-clock",
        .config = PERF_COUNT_SW_TASK_CLOCK,
    },
    {
        .name = "page-faults",
        .config = PERF_COUNT_SW_PAGE_FAULTS,
    },
    {
        .name = "context-switches",
        .config = PERF_COUNT_SW_CONTEXT_SWITCHES,
    },
    {
        .name = "cpu-migrations",
        .config = PERF_COUNT_SW_CPU_MIGRATIONS,
    },
    {
        .name = "minor-faults",
        .config = PERF_COUNT_SW_PAGE_FAULTS_MIN,
    },
    {
        .name = "major-faults",
        .config = PERF_COUNT_SW_PAGE_FAULTS_MAJ,
    },
    {
        .name = "alignment-faults",
        .config = PERF_COUNT_SW_ALIGNMENT_FAULTS,
    },
    {
        .name = "emulation-faults",
        .config = PERF_COUNT_SW_EMULATION_FAULTS,
    },
};

/*
 * Sets up the software events.
 */
static int setup_sw_events(struct pmu_event **events, struct pmu *pmu)
{
    struct pmu_event *head = NULL, *tmp, *ptr;
    int i, sw_elems;

    sw_elems = sizeof(sw_events) / sizeof(struct software_event);

    for (i = 0; i < sw_elems; i++) {
        tmp = calloc(1, sizeof(*tmp));
        if (!tmp)
            return -1;
        tmp->next = NULL;
        tmp->name = strdup(sw_events[i].name);
        if (!tmp->name) {
            cleanup_event_list(head);
            cleanup_event(tmp);
            return -E_PERFEVENT_REALLOC;
        }
        tmp->config = sw_events[i].config;
        tmp->pmu = pmu;
        if (!head) {
            head = tmp;
            ptr = head;
        } else {
            ptr->next = tmp;
            ptr = ptr->next;
        }
    }
    *events = head;
    return 0;
}

/*
 * Sets up the software PMU.
 */
static int setup_sw_pmu(struct pmu *pmu)
{
    struct pmu *tmp, *ptr;
    int ret;

    tmp = calloc(1, sizeof(*tmp));
    if (!tmp)
        return -1;
    tmp->next = NULL;

    tmp->name = strdup("software");
    if (!tmp->name) {
        ret = -1;
        goto err_ret;
    }

    tmp->type = PERF_TYPE_SOFTWARE;
    ret = setup_sw_events(&tmp->ev, pmu);
    if (ret) {
        ret = -1;
        goto err_ret;
    }

    /* add the sw pmu to the end of the pmu list */
    for (ptr = pmu; ptr->next; ptr = ptr->next);

    ptr->next = tmp;
    return 0;
 err_ret:
    cleanup_pmu(tmp);
    return ret;
}

/*
 * Utility function to fetch the contents of a
 * file(in "path") to "buf"
 */
int get_file_string(char *path, char *buf)
{
    FILE *fp;
    int ret;
    size_t size = BUF_SIZE;
    char *ptr;

    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Can't open %s\n", path);
        return -1;
    }

    ret = getline(&buf, &size, fp);
    if (ret < 0) {
        fclose(fp);
        return ret;
    }

    /* Strip off the new line character (if found) */
    ptr = strchr(buf, '\n');
    if (ptr)
        *ptr = '\0';
    fclose(fp);
    return 0;
}

/*
 * Fetches the format properties for a pmu. The format directory is
 * basically a provider of the syntax definitions for a PMU. This syntax
 * defines how an event string should be parsed.
 */
static int fetch_format_properties(char *format_path, struct property **prop)
{
    DIR *format_dir;
    struct dirent *dir;
    char *buf, property_path[PATH_MAX], *ptr, *start;
    int ret;
    struct property *pp = NULL, *tmp, *head = NULL;

    format_dir = opendir(format_path);
    if (!format_dir) {
        fprintf(stderr, "Error opening %s\n", format_path);
        return -1;
    }

    while ((dir = readdir(format_dir)) != NULL) {
        if (!strncmp(dir->d_name, ".", 1) ||
            !strncmp(dir->d_name, "..", 2)) {
            continue;
        }
        buf = calloc(BUF_SIZE, sizeof(*buf));
        if (!buf) {
            ret = -E_PERFEVENT_REALLOC;
            goto free_prop_list;
        }
        snprintf(property_path, PATH_MAX, "%s/%s", format_path,
                 dir->d_name);
        ret = get_file_string(property_path, buf);
        if (ret)
            goto free_buf;

        ptr = strchr(buf, ':');
        if (!ptr) {
            free(buf);
            continue;
        }
        *ptr = '\0';
        ptr++;
        tmp = calloc(1, sizeof(*tmp));
        if (!tmp) {
            ret = -E_PERFEVENT_REALLOC;
            goto free_buf;
        }
        tmp->next = NULL;

        if (!strncmp(buf, "config1", strlen("config1"))) {
            tmp->belongs_to = CONFIG1;
        } else if (!strncmp(buf, "config2", strlen("config2"))) {
            tmp->belongs_to = CONFIG2;
        } else
            tmp->belongs_to = CONFIG;

        tmp->name = strdup(dir->d_name);
        if (!tmp->name) {
            ret = -E_PERFEVENT_REALLOC;
            goto free_property;
        }

        start = ptr;
        ptr = strchr(ptr, '-');
        if (!ptr) {
            /* Not a range */
            tmp->lo_bit = tmp->hi_bit = atoi(start);
        } else {
            /* ptr points to '-' */
            *ptr = '\0';
            ptr++;
            tmp->lo_bit = atoi(start);
            tmp->hi_bit = atoi(ptr);
        }

        if (!pp) {
            pp = tmp;
            head = pp;
        } else {
            pp->next = tmp;
            pp = pp->next;
        }
    }

    *prop = head;
    return 0;
 free_property:
    cleanup_property(tmp);
 free_buf:
    free(buf);
 free_prop_list:
    cleanup_property_list(head);
    return ret;
}

/*
 * Based on the property_info, each event's config(s) are calculated.
 * It also checks if config1 or config2 needs to be set for each of
 * these events and accordingly sets them.
 */
static int fetch_event_config(struct property_info *head,
                              struct pmu_event *event,
                              struct pmu *pmu)
{
    struct property *pp;
    struct property_info *pi;

    for (pi = head; pi; pi = pi->next) {
        pp = pmu->prop;
        while (pp) {
            if (!strncmp(pp->name, pi->name,
                         strlen(pp->name))) {
                switch (pp->belongs_to) {
                case CONFIG:
                    event->config = event->config |
                        (pi->value << pp->lo_bit);
                    break;
                case CONFIG1:
                    event->config1 = event->config1 |
                        (pi->value << pp->lo_bit);
                    break;
                case CONFIG2:
                    event->config2 = event->config2 |
                        (pi->value << pp->lo_bit);
                    break;
                }
            }
            pp = pp->next;
        }
    }

    return 0;
}

static void cleanup_property_info(struct property_info *pi)
{
    struct property_info *pi_curr, *pi_del;

    if (!pi)
        return;
    for (pi_curr = pi; pi_curr; pi_curr = pi_del) {
        pi_del = pi_curr->next;
        if (pi_curr->name)
            free(pi_curr->name);
        free(pi_curr);
    }
}

/*
 * Parses each event string and finds out the properties each event string
 * contains.
 */
static int parse_event_string(char *buf, struct pmu_event *event,
                              struct pmu *pmu)
{
    struct property_info *pi, *head = NULL, *tmp;
    char *start, *ptr, *nptr, **endptr, *str;
    int ret = 0;

    start = buf;

    while (1) {
        ptr = strchr(start, '=');
        if (!ptr)
            break;

        /* Found a property */
        *ptr = '\0';
        ptr++;   /* ptr now points to the value */
        pi = calloc(1, sizeof(*pi));
        if (!pi) {
            cleanup_property_info(head);
            return -E_PERFEVENT_REALLOC;
        }
        pi->next = NULL;

        pi->name = strdup(start);
        if (!pi->name) {
            free(pi);
            cleanup_property_info(head);
            return -E_PERFEVENT_REALLOC;
        }

        /* Find next property */
        start = strchr(ptr, ',');
        if (!start) {
            str = buf + strlen(buf) - 1;
            endptr = &str;
            nptr = ptr;
            pi->value = strtoull(nptr, endptr, 16);
            if (!head) {
                head = pi;
                pi = pi->next;
            } else {
                tmp->next = pi;
                tmp = tmp->next;
            }
            break;
        } else {
            /* We found the next property */
            *start = '\0';
            str = buf + strlen(buf) - 1;
            endptr = &str;
            start++;
            nptr = ptr;
            pi->value = strtoul(nptr, endptr, 16);
        }

        if (!head) {
            head = pi;
            tmp = head;
        } else {
            tmp->next = pi;
            tmp = tmp->next;
        }
    }

    if (ret)
        return ret;

    ret = fetch_event_config(head, event, pmu);
    return ret;
}

/*
 * Fetches all the events(exposed by the kernel) for a PMU. It also
 * initializes the config(s) for each event based on the PMU's property
 * list.
 */
static int fetch_events(DIR *events_dir, struct pmu_event **events,
                        struct pmu *pmu, char *events_path)
{
    struct dirent *dir;
    struct pmu_event *ev = NULL, *tmp, *head = NULL;
    char event_path[PATH_MAX], *buf;
    int ret = 0;

    if (!events_dir)
        return -1;

    while ((dir = readdir(events_dir)) != NULL) {
        if (!strncmp(dir->d_name, ".", 1) ||
            !strncmp(dir->d_name, "..", 2))
            continue;

        /* Ignoring .scale and .unit for now */
        if (strstr(dir->d_name, ".scale") || strstr(dir->d_name, ".unit"))
            continue;

        tmp = calloc(1, sizeof(*tmp));
        if (!tmp) {
            cleanup_event_list(head);
            ret = -E_PERFEVENT_REALLOC;
            goto free_event_list;
        }
        tmp->next = NULL;
        tmp->name = strdup(dir->d_name);
        if (!tmp->name) {
            ret = -E_PERFEVENT_REALLOC;
            goto free_event;
        }

        /* Now, we have to find the config value for this event */
        buf = calloc(BUF_SIZE, sizeof(*buf));
        if (!buf) {
            ret = -E_PERFEVENT_REALLOC;
            goto free_event;
        }

        memset(event_path, '\0', PATH_MAX);
        snprintf(event_path, PATH_MAX, "%s/%s", events_path, dir->d_name);
        ret = get_file_string(event_path, buf);
        if (ret) {
            ret = -E_PERFEVENT_RUNTIME;
            goto free_buf;
        }

        ret = parse_event_string(buf, tmp, pmu);
        if (ret) {
            ret = -E_PERFEVENT_RUNTIME;
            goto free_buf;
        }

        tmp->pmu = pmu;

        if (!ev) {
            ev = tmp;
            head = ev;
        } else {
            ev->next = tmp;
            ev = ev->next;
        }
    }

    *events = head;
    return 0;
 free_buf:
    free(buf);
 free_event:
    cleanup_event(tmp);
 free_event_list:
    cleanup_event_list(head);
    return ret;
}

/*
 * Fetches the PMU type which is in "type_path".
 */
static int fetch_pmu_type(char *type_path, struct pmu *pmu)
{
    char *buf;
    int ret;

    buf = calloc(1, sizeof(BUF_SIZE));
    if (!buf)
        return -1;

    ret = get_file_string(type_path, buf);
    if (ret < 0) {
        free(buf);
        return ret;
    }

    pmu->type = (int)strtoul(buf, NULL, 10);
    free(buf);
    return 0;
}

/*
 * For a PMU "pmu", its "events" directory is looked up, if not
 * present, then we will not count that in the list of pmus. If the
 * "events" directory is present, we proceed to read the "format"
 * directory which helps in parsing the event string. We also fetch
 * the type of the pmu.
 */
static int fetch_format_and_events(char *pmu_path, struct pmu *pmu)
{
    DIR *events_dir;
    char format_path[PATH_MAX], events_path[PATH_MAX];
    char type_path[PATH_MAX];
    struct property *tmp = NULL;
    struct pmu_event *ev = NULL;
    int ret;

    snprintf(type_path, PATH_MAX, "%s/%s", pmu_path, PMU_TYPE);
    snprintf(format_path, PATH_MAX, "%s/%s", pmu_path, FORMAT);
    snprintf(events_path, PATH_MAX, "%s/%s", pmu_path, EVENTS);
    events_dir = opendir(events_path);
    if (!events_dir)
        return -E_PERFEVENT_RUNTIME;

    ret = fetch_format_properties(format_path, &tmp);
    if (ret)
        goto close_dir;

    pmu->prop = tmp;
    ret = fetch_pmu_type(type_path, pmu);
    if (ret) {
        cleanup_property_list(tmp);
        goto close_dir;
    }

    ret = fetch_events(events_dir, &ev, pmu, events_path);
    if (ret) {
        cleanup_property_list(tmp);
        goto close_dir;
    }

    pmu->ev = ev;
    ret = 0;
 close_dir:
    closedir(events_dir);
    return ret;
}

/*
 * Populates the entire list of available PMUs in pmus.
 */
static int populate_pmus(struct pmu **pmus)
{
    DIR *pmus_dir;
    struct pmu *tmp, *head = NULL, *ptr = NULL;
    struct dirent *dir;
    char pmu_path[PATH_MAX];
    int ret = -1;

    pmus_dir = opendir(DEV_DIR);
    if (!pmus_dir)
        return ret;

    while ((dir = readdir(pmus_dir)) != NULL) {
        /* Ignore the . and .. */
        if (!strncmp(dir->d_name, ".", 1) ||
            !strncmp(dir->d_name, "..", 2)) {
            continue;
        }

        memset(pmu_path, '\0', PATH_MAX);
        snprintf(pmu_path, PATH_MAX, "%s%s", DEV_DIR, dir->d_name);
        tmp = calloc(1, sizeof(*tmp));
        if (!tmp) {
            ret = -E_PERFEVENT_REALLOC;
            goto free_pmulist;
        }
        tmp->next = NULL;
        ret = fetch_format_and_events(pmu_path, tmp);
        if (ret) {
            /*
             * If there was in issue initializing any event
             * for this pmu, we should free up this pmu.
             * However, we should continue looking for other
             * pmus. Clear the entire list only for allocation
             * errors.
             */
            if (ret == -E_PERFEVENT_REALLOC) {
                /*
                 * Only in this case, we would clean up the entire
                 * list.
                 */
                goto free_pmu;
            }
            cleanup_pmu(tmp);
            continue;
        }

        tmp->name = strdup(dir->d_name);
        if (!tmp->name) {
            ret = -E_PERFEVENT_REALLOC;
            goto free_pmu;
        }

        if (!head) {
            head = tmp;
            ptr = head;
        } else {
            ptr->next = tmp;
            ptr = ptr->next;
        }
    }

    closedir(pmus_dir);

    *pmus = head;
    return 0;
 free_pmu:
    cleanup_pmu(tmp);
 free_pmulist:
    cleanup_pmu_list(head);
    closedir(pmus_dir);
    return ret;
}

/*
 * Finds the cpumask related to a pmu. If no cpumask file is present,
 * it assumes the default mask, i.e., the set of all cpus.
 */
void setup_cpu_config(struct pmu *pmu_ptr, int *ncpus, int **cpuarr,
                      cpulist_t *cpus)
{
    FILE *cpulist;
    char cpumask_path[PATH_MAX], *line;
    int *on_cpus;
    size_t len = 0;

    memset(cpumask_path, '\0', PATH_MAX);
    snprintf(cpumask_path, PATH_MAX, "%s%s/%s", DEV_DIR, pmu_ptr->name,
             PMU_CPUMASK);
    cpulist = fopen(cpumask_path, "r");
    /*
     * If this file is not available, then the cpumask is the set of all
     * available cpus.
     */
    if (!cpulist)
        return;

    on_cpus = calloc(cpus->count, sizeof(int));
    if (!on_cpus) {
        fclose(cpulist);
        return;
    }

    if (getline(&line, &len, cpulist) > 0) {
        *ncpus = parse_delimited_list(line, on_cpus);
        if (*ncpus > -1)
            *cpuarr = on_cpus;
        else
            *cpuarr = NULL;
    }

    fclose(cpulist);
}

/*
 * Setup the dynamic events taken from /sys/bus/event_source/devices
 * pmu_list contains the list of all the PMUs and their events upon
 * execution of this function.
 */
int init_dynamic_events(struct pmu **pmu_list) {
    int ret = -1;
    struct pmu *pmus = NULL;

    ret = populate_pmus(&pmus);
    if (ret)
        return ret;

    /*
     * setup the software pmu separately, since, it needs to be
     * setup with a set of static events.
     */
    ret = setup_sw_pmu(pmus);
    if (ret)
        return ret;

    *pmu_list = pmus;
    return 0;
}
