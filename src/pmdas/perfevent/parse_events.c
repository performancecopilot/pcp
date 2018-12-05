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
 */

#include "pmapi.h"
#include <dirent.h>

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

void cleanup_pmu_list(struct pmu *pmu)
{
    struct pmu *pmu_del, *curr_pmu;

    if (!pmu)
        return;

    for (curr_pmu = pmu; curr_pmu; curr_pmu = pmu_del) {
        pmu_del = curr_pmu->next;
        cleanup_pmu(curr_pmu);
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
        if (!head || strcmp(head->name, tmp->name) >= 0) {
            tmp->next = head;
            head = tmp;
        } else {
            ptr = head;
            while (ptr->next && strcmp(ptr->next->name, tmp->name) < 0)
                ptr = ptr->next;
            tmp->next = ptr->next;
            ptr->next = tmp;
        }
    }
    *events = head;
    return 0;
}

/*
 * Sets up the software PMU.
 */
static int setup_sw_pmu(struct pmu **pmu)
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
    ret = setup_sw_events(&tmp->ev, *pmu);
    if (ret) {
        ret = -1;
        goto err_ret;
    }

    if (*pmu == NULL)
        *pmu = tmp;
    else {
        /* add the sw pmu to the end of the pmu list */
        for (ptr = *pmu; ptr->next; ptr = ptr->next);
        ptr->next = tmp;
    }
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
    struct property *pp, *tmp, *head = NULL;

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
        pmsprintf(property_path, PATH_MAX, "%s/%s", format_path,
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

        if (!head || strcmp(head->name, tmp->name) >= 0) {
            tmp->next = head;
            head = tmp;
        } else {
            pp = head;
            while (pp->next && strcmp(pp->next->name, tmp->name) < 0)
                pp = pp->next;
            tmp->next = pp->next;
            pp->next = tmp;
        }
    }
    closedir(format_dir);
    *prop = head;
    return 0;

 free_property:
    cleanup_property(tmp);
 free_buf:
    free(buf);
 free_prop_list:
    cleanup_property_list(head);
    closedir(format_dir);
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
    int match_count = 0, have = 0;

    for (pi = head; pi; pi = pi->next) {
        pp = pmu->prop;
        while (pp) {
            if (!strcmp(pp->name, pi->name)) {
                match_count++;
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

    /*
     * even if syntax of one property found in event string doesn't exist
     * in the format directory of the PMU, then, we return error.
     */
    if (!match_count)
        return -1;
    for (pi = head; pi; pi = pi->next) {
        have++;
    }
    if (have != match_count)
        return -1;
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
                              struct pmu *pmu, struct pmcsetting *dynamicpmc,
			       char *pmu_name)
{
    struct property_info *pi, *head = NULL, *tmp;
    char *start, *ptr, *nptr, **endptr, *str, eventname[BUF_SIZE];
    struct pmcsetting *pmctmp;

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

	pmsprintf(eventname, sizeof(eventname), "%s.%s", pmu_name, event->name);

        /* Find next property */
        start = strchr(ptr, ',');
        if (!start) {
            str = buf + strlen(buf) - 1;
            endptr = &str;
            nptr = ptr;
	    if ((!strcmp(nptr, "?")) && (!strcmp(pi->name, "chip"))) {
		for (pmctmp = dynamicpmc; pmctmp; pmctmp = pmctmp->next) {
			if (!strncmp(eventname, pmctmp->name, strlen(eventname)))
			{
			     pi->value = pmctmp->chip;
			}
		}
            } else
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
	    if ((!strcmp(nptr, "?")) && (!strcmp(pi->name, "chip"))) {
		for (pmctmp = dynamicpmc; pmctmp; pmctmp = pmctmp->next) {
			if (!strncmp(eventname, pmctmp->name, strlen(eventname)))
			{
			     pi->value = pmctmp->chip;
			}
		}
	    } else
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

    return fetch_event_config(head, event, pmu);
}

/*
 * Fetches all the events(exposed by the kernel) for a PMU. It also
 * initializes the config(s) for each event based on the PMU's property
 * list.
 */
static int fetch_events(DIR *events_dir, struct pmu_event **events,
                        struct pmu *pmu, char *events_path,
			 struct pmcsetting *dynamicpmc, char *pmu_name)
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
        pmsprintf(event_path, PATH_MAX, "%s/%s", events_path, dir->d_name);
        ret = get_file_string(event_path, buf);
        if (ret) {
            ret = -E_PERFEVENT_RUNTIME;
            goto free_buf;
        }

        ret = parse_event_string(buf, tmp, pmu, dynamicpmc, pmu_name);
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
static int fetch_format_and_events(char *pmu_path, struct pmu *pmu,
				   struct pmcsetting *dynamicpmc, char *pmu_name)
{
    DIR *events_dir;
    char format_path[PATH_MAX], events_path[PATH_MAX];
    char type_path[PATH_MAX];
    struct property *tmp = NULL;
    struct pmu_event *ev = NULL;
    int ret;

    pmsprintf(type_path, PATH_MAX, "%s/%s", pmu_path, PMU_TYPE);
    pmsprintf(format_path, PATH_MAX, "%s/%s", pmu_path, FORMAT);
    pmsprintf(events_path, PATH_MAX, "%s/%s", pmu_path, EVENTS);
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

    ret = fetch_events(events_dir, &ev, pmu, events_path, dynamicpmc, pmu_name);
    if (ret) {
        cleanup_property_list(tmp);
        pmu->prop = NULL;
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
static int populate_pmus(struct pmu **pmus, struct pmcsetting *dynamicpmc)
{
    DIR *pmus_dir;
    struct pmu *tmp, *head = NULL, *ptr = NULL;
    struct dirent *dir;
    char pmu_path[PATH_MAX];
    int ret = -1;

    pmus_dir = opendir(dev_dir);
    if (!pmus_dir)
        return ret;

    while ((dir = readdir(pmus_dir)) != NULL) {
        /* Ignore the . and .. */
        if (!strncmp(dir->d_name, ".", 1) ||
            !strncmp(dir->d_name, "..", 2)) {
            continue;
        }

        memset(pmu_path, '\0', PATH_MAX);
        pmsprintf(pmu_path, PATH_MAX, "%s%s", dev_dir, dir->d_name);
        tmp = calloc(1, sizeof(*tmp));
        if (!tmp) {
            ret = -E_PERFEVENT_REALLOC;
            goto free_pmulist;
        }
        tmp->next = NULL;
        tmp->prop = NULL;
        tmp->ev = NULL;
        ret = fetch_format_and_events(pmu_path, tmp, dynamicpmc, dir->d_name);
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

        if (!head || strcmp(head->name, tmp->name) >= 0) {
            tmp->next = head;
            head = tmp;
        } else {
            ptr = head;
            while (ptr->next && strcmp(ptr->next->name, tmp->name) < 0)
                ptr = ptr->next;
            tmp->next = ptr->next;
            ptr->next = tmp;
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
void setup_cpu_config(struct pmu *pmu_ptr, int *ncpus, int **cpuarr)
{
    FILE *cpulist;
    char cpumask_path[PATH_MAX], *line = NULL;
    int *on_cpus = NULL;
    size_t len = 0;

    memset(cpumask_path, '\0', PATH_MAX);
    pmsprintf(cpumask_path, PATH_MAX, "%s%s/%s", dev_dir, pmu_ptr->name,
             PMU_CPUMASK);
    cpulist = fopen(cpumask_path, "r");
    /*
     * If this file is not available, then the cpumask is the set of all
     * available cpus.
     */
    if (!cpulist)
        return;

    if (getline(&line, &len, cpulist) > 0) {
	*ncpus = parse_delimited_list(line, NULL);
        if (*ncpus > 0) {
            on_cpus = calloc(*ncpus, sizeof (*on_cpus));
            if (!on_cpus) {
                fclose(cpulist);
                *cpuarr = NULL;
                return;
            }
        } else {
            fclose(cpulist);
            *cpuarr = NULL;
            return;
        }

        parse_delimited_list(line, on_cpus);
        *cpuarr = on_cpus;
    }

    fclose(cpulist);
}

/*
 * Setup the dynamic events taken from /sys/bus/event_source/devices
 * pmu_list contains the list of all the PMUs and their events upon
 * execution of this function.
 */
int init_dynamic_events(struct pmu **pmu_list, struct pmcsetting *dynamicpmc)
{
    int ret = -1;
    struct pmu *pmus = NULL;
    char *prefix;

    /*
     * Set the path where we can find the PMU devices. If the environment
     * variable SYSFS_PREFIX is set, then we are in testing mode and going
     * to search in some custom sysfs directory.
     */
    memset(dev_dir, '\0', PATH_MAX);
    prefix = getenv("SYSFS_PREFIX");
    if (prefix)
        pmsprintf(dev_dir, PATH_MAX, "%s%s", prefix, DEV_DIR);
    else
        pmsprintf(dev_dir, PATH_MAX, "%s%s", "/sys/", DEV_DIR);

    ret = populate_pmus(&pmus, dynamicpmc);
    if (ret)
        return ret;

    /*
     * setup the software pmu separately, since, it needs to be
     * setup with a set of static events.
     */
    ret = setup_sw_pmu(&pmus);
    if (ret)
        return ret;

    *pmu_list = pmus;
    return 0;
}
