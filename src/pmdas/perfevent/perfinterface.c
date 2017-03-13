/* perf interface implementation
 *
 * Copyright (C) 2013  Joe White
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

#include "perfinterface.h"
#include "rapl-interface.h"
#include "configparser.h"
#include "architecture.h"
#include "parse_events.h"
#include <perfmon/pfmlib_perf_event.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#define SYSFS_DEVICES "/sys/bus/event_source/devices"
#define BUF_SIZE 1024
#define MAX_EVENT_NAME 1024

#define EVENT_TYPE_PERF 0
#define EVENT_TYPE_RAPL 1

#define RAW_VALUE 0
#define TIME_ENABLED 1
#define TIME_RUNNING 2

const char *perf_strerror(int err)
{
    const char *ret = "Unknown error";
    switch(err)
    {
        case -E_PERFEVENT_LOGIC:
            ret = "Internal logic error";
            break;
        case -E_PERFEVENT_REALLOC:
            ret = "Memory allocation error";
            break;
        case -E_PERFEVENT_RUNTIME:
            ret = "Runtime error";
    }

    return ret;
}

static void free_eventcpuinfo(eventcpuinfo_t *del)
{
    if(NULL == del)
    {
        return;
    }
    if(del->fd != -1)
    {
        close(del->fd);
    }
    free(del->fstr);
}

static void free_event(event_t *del)
{
    int i;
    if(NULL == del)
    {
        return;
    }

    for(i = 0; i < del->ncpus; ++i)
    {
        free_eventcpuinfo( &del->info[i] );
    }

    free(del->info);
    free(del->name);
}

static void free_perfdata(perfdata_t *del)
{
    int i;

    if(0 == del ) {
        return;
    }
    for ( i = 0; i < del->nevents; ++i )
    {
        free_event(&del->events[i]);
    }
    free(del->events);
    free_architecture(del->archinfo);
    free(del->archinfo);
    free(del);

    pfm_terminate();
}

/*
 * Search an event from the event list
 */
static event_t *search_event(perfdata_t *inst, const char *event_name)
{
    int i;

    for (i = 0; i < inst->nevents; i++) {
        if (!strcmp(event_name, inst->events[i].name)) {
            return ((inst->events) + i);
        }
    }

    return NULL;
}

/*
 * Free up the event list "event_list"
 */
static void free_event_list(event_list_t *event_list)
{
    event_list_t *tmp;

    tmp = event_list;
    while(tmp) {
        tmp = tmp->next;
        free(event_list);
        event_list = tmp;
    }
}

/* Right now, only capable of parsing event and umask */
static int parse_and_get_config(char *config_str, uint64_t *config)
{
    char *start_token, *end_token = NULL, *end_ptr = NULL, *value_ptr;
    uint64_t event_sel = 0, umask = 0;

    if (!config_str)
        return -1;

    /* Search for event= */
    start_token = config_str;
    /*
     * Start looking for tokens.
     * Currently, supported: "event" and "umask"
     */
    while (1) {
        value_ptr = strchr(start_token, '=');
        if (!value_ptr) {
            fprintf(stderr, "Error in config string\n");
            return -1;
        }
        end_token = strchr(start_token, ',');
        if (!end_token)
            end_ptr = end_token - 1;
        else
            end_ptr = config_str + strlen(config_str - 1);

        if (!strncmp(start_token, "event=", strlen("event=")))
            event_sel = strtoull(value_ptr + 1, &end_ptr, 16);
        else if (!strncmp(start_token, "umask=", strlen("umask=")))
            umask = strtoull(value_ptr + 1, &end_ptr, 16);
        else
            break;
        /* No more token to parse after this */
        if (!end_token)
            break;
        /* Point after ',' */
        start_token = end_token + 1;
    }

    /*
     * We have the event and umask fields, find the config value
     * umask : config[15:8]
     * event_sel : config[7:0]
     */
    if (event_sel && umask)
        *config = (umask << 8) | event_sel;
    else
        return -1;
    /* Search for umask= */
    return 0;
}

static int search_for_config(char *device_path, uint64_t config, char *event_file)
{
    char events_path[PATH_MAX], event_path[PATH_MAX], *ptr, *buf = NULL;
    DIR *events_dir;
    struct dirent *entry;
    uint64_t parsed_config = 0;
    int ret = -1;

    snprintf(events_path, PATH_MAX, "%s/events/", device_path);
    events_dir = opendir(events_path);
    if (NULL == events_dir) {
        fprintf(stderr, "Error in opening %s\n", events_path);
        return -1;
    }

    while ((entry = readdir(events_dir)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        snprintf(event_path, PATH_MAX, "%s/events/%s", device_path, entry->d_name);

        buf = calloc(sizeof(char), BUF_SIZE);
        if (NULL == buf) {
            fprintf(stderr, "Error in allocating memory for buf\n");
            ret = -E_PERFEVENT_REALLOC;
            break;
        }
        ret = get_file_string(event_path, buf);
        if (ret < 0) {
            free(buf);
            continue;
        }

        /* Check whether atleast "event=" is present */
        ptr = strstr(buf, "event=");
        if (!ptr) {
            free(buf);
            continue;
        }

        ret = parse_and_get_config(buf, &parsed_config);
        if (ret < 0) {
            fprintf(stderr, "parse_and_get_config failed\n");
            free(buf);
            break;
        }
        if (parsed_config == config) {
            strncpy(event_file, entry->d_name, strlen(entry->d_name));
            ret = 0;
            break;
        }
        if (buf)
            free(buf);
    }

    closedir(events_dir);
    return ret;
}

static int find_and_fetch_scale(char *path_str, uint64_t config,
                                double *scale)
{
    char *device_path, *event_file, scale_path[PATH_MAX], *buf;
    int ret = -1;

    device_path = calloc(PATH_MAX, sizeof(char));
    if (NULL == device_path) {
        fprintf(stderr, "Error in allocating memory\n");
        return -E_PERFEVENT_REALLOC;
    }
    snprintf(device_path, PATH_MAX, "%s", path_str);

    event_file = calloc(MAX_EVENT_NAME, sizeof(char));
    if (!event_file) {
        fprintf(stderr, "Error in allocating memory for event_file\n");
        ret = -E_PERFEVENT_REALLOC;
        goto free_dev;
    }

    /* Need to free up event_file after using this call */
    ret = search_for_config(device_path, config, event_file);
    if (ret) {
        fprintf(stderr, "search_for_config failed\n");
        goto free_event;
    }

    /* Got the right event name in event_file, fetch the scale */
    snprintf(scale_path, PATH_MAX, "%s/events/%s.scale", device_path, event_file);
    buf = calloc(BUF_SIZE, sizeof(char));
    if (!buf) {
        fprintf(stderr, "Error in allocating memory to buf\n");
        ret = -E_PERFEVENT_REALLOC;
        goto free_event;
    }

    ret = get_file_string(scale_path, buf);
    if (ret) {
        fprintf(stderr, "Couldn't read scale from get_file_string, %s\n", scale_path);
        goto free_buf;
    }
    *scale = strtod(buf, NULL);

 free_buf:
    free(buf);
 free_event:
    free(event_file);
 free_dev:
    free(device_path);
    return ret;
}

static int parse_sysfs_perf_event_scale(int type, uint64_t config,
                                        double *scale)
{
    DIR *devices_dir;
    struct dirent* entry;
    char fullpath[PATH_MAX];
    char *path_str, *buf = NULL;
    int fetched_type = -1, ret = -1;

    devices_dir = opendir(SYSFS_DEVICES);
    if (NULL == devices_dir) {
        fprintf(stderr, "Error in opening %s\n", SYSFS_DEVICES);
        return ret;
    }

    path_str = calloc(PATH_MAX, sizeof(char));
    if (!path_str) {
        fprintf(stderr, "Error in allocating memory to path_str\n");
        goto close_dir;
    }

    while ((entry = readdir(devices_dir)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        snprintf(fullpath, PATH_MAX, "%s/%s", SYSFS_DEVICES, entry->d_name);
        snprintf(path_str, PATH_MAX, "%s/type", fullpath);

        buf = calloc(BUF_SIZE, sizeof(char));
        if (!buf) {
            fprintf(stderr, "Error in allocating memory to buf\n");
            ret = -1;
            goto close_dir;
        }

        ret = get_file_string(path_str, buf);
        if (ret < 0) {
            free(buf);
            goto close_dir;
        }
        fetched_type = (int)strtol(buf, NULL, 10);
        free(buf);

        if (fetched_type < 0) {
            ret = -1;
            goto close_dir;
        }
        if (fetched_type == type)
            break;
    }

    if (fetched_type == type)
        ret = find_and_fetch_scale(fullpath, config, scale);

 close_dir:
    closedir(devices_dir);
    return ret;

}

static int fetch_perf_scale(char *event_name, double *scale)
{
    event_t *event;
    eventcpuinfo_t *info;
    pfm_perf_encode_arg_t arg;
    int type, ret;
    uint64_t config;

    event = calloc(1, sizeof(event_t));
    if (!event)
        return -1;
    event->name = strdup(event_name);
    info = event->info;
    info = calloc((sizeof *info),  1);
    event->ncpus = 0;

    info->type = EVENT_TYPE_PERF;

    /* ABI compatibility, set before calling libpfm */
    info->hw.size = sizeof(info->hw);

    memset(&arg, 0, sizeof(arg));
    arg.attr = &(info->hw);
    arg.fstr = &(info->fstr); /* info->fstr is NULL */

    ret = pfm_get_os_event_encoding(event_name, PFM_PLM0|PFM_PLM3, PFM_OS_PERF_EVENT_EXT, &arg);

    if (ret != PFM_SUCCESS) {
        fprintf(stderr, "pfm_get_os_event_encoding failed \"%s\": %s\n",
                event_name, pfm_strerror(ret));
        free_eventcpuinfo(info);
        ret = -1;
        goto free_all;
    }

    type = info->hw.type;
    config = info->hw.config;

    ret = parse_sysfs_perf_event_scale(type, config, scale);
    if (ret) {
        free_eventcpuinfo(info);
        ret = -1 ;
    }

 free_all:
    free_eventcpuinfo(event->info);
    free(event->name);
    free(event);

    return ret;
}

/*
 * Setup a derived event
 */
static int perf_setup_derived_event(perfdata_t *inst, pmcderived_t *derived_pmc)
{
    derived_event_t *curr, *derived_events = inst->derived_events;
    int nderivedevents = inst->nderivedevents;
    event_t *event;
    pmcsetting_t *derived_setting;
    pmcSettingLists_t *setting_list;
    event_list_t *ptr, *tmp, *event_list;
    int cpuconfig, clear_history = 0, ret;

    tmp = NULL;
    event_list = NULL;
    if (NULL == derived_pmc->setting_lists) {
        fprintf(stderr, "No derived_pmc settings\n");
        return -E_PERFEVENT_LOGIC;
    }

    /*
     * If a certain setting_list is not available, then we need to check if the
     * next one is available.
     */
    setting_list = derived_pmc->setting_lists;

    while (setting_list) {
        event_list = NULL;
        derived_setting = setting_list->derivedSettingList;
        clear_history = 0;
        if (derived_setting)
            cpuconfig = derived_setting->cpuConfig;
        while (derived_setting) {
            event = search_event(inst, derived_setting->name);
            if (NULL == event) {
                fprintf(stderr, "Derived setting %s not found\n", derived_setting->name);
                clear_history = 1;
                break;
            }

            if (cpuconfig != derived_setting->cpuConfig) {
                fprintf(stderr, "Mismatch in cpu configuration\n");
                free_event_list(event_list);
                return -E_PERFEVENT_LOGIC;
            }

            tmp = calloc(1, sizeof(*tmp));
            if (NULL == tmp) {
                free_event_list(event_list);
                return -E_PERFEVENT_REALLOC;
            }
            tmp->event = event;

            if (derived_setting->need_perf_scale) {
                ret = fetch_perf_scale(event->name, &tmp->scale);
                if (ret < 0) {
                    fprintf(stderr, "Couldn't fetch perf_scale for the %s event\n",
                            event->name);
                    free_event_list(event_list);
                    return ret;
                }
            }
            else
                tmp->scale = derived_setting->scale;

            tmp->next = NULL;
            derived_setting = derived_setting->next;

            if (NULL == event_list) {
                event_list = tmp;
                ptr = event_list;
            } else {
                ptr->next = tmp;
                ptr = ptr->next;
            }
        }

        /* There was a event mismatch in the curr list, so, discard this list */
        if (clear_history)
            free_event_list(event_list);

        /* All the events in the curr list have been successfully found */
        if (NULL == derived_setting)
            break;
        setting_list = setting_list->next;
    }

    /* If clear_history is still on, then, none of the events were found */
    if (clear_history) {
        fprintf(stderr, "None of the derived settings found\n");
        return -E_PERFEVENT_LOGIC;
    }

    derived_events = realloc(derived_events,
                             (nderivedevents + 1) * sizeof(*derived_events));
    if (NULL == derived_events) {
        free(inst->derived_events);
        inst->nderivedevents = 0;
        inst->derived_events = NULL;
        free_event_list(event_list);
        return -E_PERFEVENT_REALLOC;
    }

    curr = derived_events + nderivedevents;
    curr->name = strdup(derived_pmc->name);
    curr->event_list = event_list;
    (inst->nderivedevents)++;
    inst->derived_events = derived_events;

    return 0;
}


/* Setup an event
 */
static int perf_setup_event(perfdata_t *inst, const char *eventname, const int cpuSetting)
{
    int i;
    int ncpus, ret;
    int *cpuarr;

    event_t *events = inst->events;
    int nevents = inst->nevents;
    archinfo_t *archinfo = inst->archinfo;

    /* Increase size of event array */
    events = realloc(events, (nevents + 1) * sizeof(*events) );
    if(NULL == events)
    {
        free(inst->events);
        inst->nevents = 0;
        inst->events = NULL;
        return -E_PERFEVENT_REALLOC;
    }

    switch(cpuSetting)
    {
        case CPUCONFIG_ROUNDROBIN_CPU:
            cpuarr = &archinfo->cpus.index[inst->roundrobin_cpu_idx];
            ncpus = 1;
            inst->roundrobin_cpu_idx = (inst->roundrobin_cpu_idx + 1) % archinfo->cpus.count;
            break;
        case CPUCONFIG_EACH_CPU:
            cpuarr = archinfo->cpus.index;
            ncpus = archinfo->cpus.count;
            break;
        case CPUCONFIG_EACH_NUMANODE:
            cpuarr = archinfo->cpunodes[0].index;
            ncpus = archinfo->cpunodes[0].count;
            break;
        case CPUCONFIG_ROUNDROBIN_NUMANODE:
            cpuarr = archinfo->cpunodes[inst->roundrobin_nodecpu_idx].index;
            ncpus = archinfo->cpunodes[inst->roundrobin_nodecpu_idx].count;
            inst->roundrobin_nodecpu_idx = (inst->roundrobin_nodecpu_idx + 1) % archinfo->ncpus_per_node;
            break;
        default:
            if(cpuSetting < archinfo->cpus.count) {
                cpuarr = &archinfo->cpus.index[cpuSetting];
            } else {
                cpuarr = &archinfo->cpus.index[0];
            }
            ncpus = 1;
            break;
    }

    event_t *curr = events + nevents;
    curr->name = strdup(eventname);
    curr->info = malloc( (sizeof *(curr->info)) * ncpus );
    curr->ncpus = 0;

    eventcpuinfo_t *info = &curr->info[0];

    for(i = 0; i < ncpus; ++i)
    {
        memset(info, 0, sizeof *info);
        info->fd = -1;
        info->cpu = cpuarr[i];

        if( 0 == strncmp(eventname, "RAPL:", 5) ) {
            // try to use rapl interface
            info->type = EVENT_TYPE_RAPL;
            ret = rapl_get_os_event_encoding( eventname, info->cpu, &info->rapldata );

            if( ret != 0 ) {
                fprintf(stderr, "rapl_get_os_event_encoding failed on cpu%d for \"%s\": %s\n",
                        info->cpu, eventname, "" );
                free_eventcpuinfo(info);
                break;
            }

            ret = rapl_open( &info->rapldata );

            if( ret != 0 ) {
                fprintf(stderr, "rapl_open failed on cpu%d for \"%s\": %s\n", 
                        info->cpu, curr->name, strerror(errno) );
                free_eventcpuinfo(info);
                continue;
            }

        } else {

            info->type = EVENT_TYPE_PERF;

            /* ABI compatibility, set before calling libpfm */
            info->hw.size = sizeof(info->hw);

            pfm_perf_encode_arg_t arg;
            memset(&arg, 0, sizeof(arg));
            arg.attr = &(info->hw);
            arg.fstr = &(info->fstr); /* info->fstr is NULL */

            ret = pfm_get_os_event_encoding(eventname, PFM_PLM0|PFM_PLM3, PFM_OS_PERF_EVENT_EXT, &arg);

            if (ret != PFM_SUCCESS) {
                fprintf(stderr, "pfm_get_os_event_encoding failed on cpu%d for \"%s\": %s\n", 
                        info->cpu, eventname, pfm_strerror(ret));
                free_eventcpuinfo(info);
                break;
            }

            info->idx = arg.idx;

            info->hw.disabled = 1;
            info->hw.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
            info->fd = perf_event_open(&info->hw, -1, info->cpu, -1, 0);
            if(info->fd == -1)
            {
                fprintf(stderr, "perf_event_open failed on cpu%d for \"%s\": %s\n", 
                        info->cpu, curr->name, strerror(errno) );
                free_eventcpuinfo(info);
                continue;
            }
        }

        /* The event was configured sucessfully */
	curr->disable_event = 0;
        ++info;
        ++(curr->ncpus);
    }

    if(curr->ncpus > 0)
    {
        ++nevents;
        ret = 0;
    }
    else
    {
        free_event(curr);
        ret = -E_PERFEVENT_RUNTIME;
    }

    inst->events = events;
    inst->nevents = nevents;
    return ret;
}

/*
 * Setup the dynamic events
 */
static int perf_setup_dynamic_events(perfdata_t *inst,
				     pmcsetting_t *dynamic_setting,
				     struct pmu *pmu_list)
{
    int i, ncpus, ret = 0, *cpuarr = NULL, nevents = inst->nevents;
    event_t *events = inst->events;
    archinfo_t *archinfo = inst->archinfo;
    struct pmu *pmu_ptr;
    struct pmu_event *event_ptr;
    char eventname[BUF_SIZE];
    pmcsetting_t *ptr;
    int disable_event;

    for (pmu_ptr = pmu_list; pmu_ptr; pmu_ptr = pmu_ptr->next) {
        for (event_ptr = pmu_ptr->ev; event_ptr;
             event_ptr = event_ptr->next) {
            ncpus = 0;
            disable_event = 1;
            /* Setup the event name */
            snprintf(eventname, BUF_SIZE, "%s.%s", pmu_ptr->name,
                     event_ptr->name);
            for (ptr = dynamic_setting; ptr; ptr = ptr->next) {
                if (!strncmp(eventname, ptr->name,
                             strlen(eventname)))
                    disable_event = 0;
            }

            /* Increase the size of event array */
            events = realloc(events, (nevents + 1) * sizeof(*events));
            if (!events) {
                free(inst->events);
                inst->nevents = 0;
                inst->events = NULL;
                return -E_PERFEVENT_REALLOC;
            }

            setup_cpu_config(pmu_ptr, &ncpus, &cpuarr,
                             &archinfo->cpus);

            if (ncpus <= 0) { /* Assume default cpu set */
                cpuarr = archinfo->cpus.index;
                ncpus = archinfo->cpus.count;
            }

            event_t *curr = events + nevents;
            curr->name = strdup(eventname);

            curr->disable_event = disable_event;
            if (disable_event) {
                ++nevents;
                ret = 0;
                continue;
            }
            curr->info = calloc(ncpus, (sizeof *(curr->info)) * ncpus);
            curr->ncpus = 0;

            eventcpuinfo_t *info = &curr->info[0];

            for(i = 0; i < ncpus; ++i) {
                memset(info, 0, sizeof *info);
                info->fd = -1;
                info->cpu = cpuarr[i];
                info->type = EVENT_TYPE_PERF;
                info->hw.size = sizeof(info->hw);

                pfm_perf_encode_arg_t arg;
                memset(&arg, 0, sizeof(arg));

                info->idx = arg.idx;

                info->hw.type = pmu_ptr->type;
                info->hw.size = sizeof(info->hw);
                info->hw.config = event_ptr->config;
                info->hw.config1 = event_ptr->config1;
                info->hw.config2 = event_ptr->config2;
                info->hw.disabled = 1;
                info->hw.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

                info->fd = perf_event_open(&info->hw, -1, info->cpu, -1, 0);
                if(info->fd == -1) {
                    fprintf(stderr, "perf_event_open failed on cpu%d for \"%s\": %s\n",
                            info->cpu, curr->name, strerror(errno) );
                    free_eventcpuinfo(info);
                    continue;
                }

                /* The event was configured sucessfully */
                ++info;
                ++(curr->ncpus);
            }

            if(curr->ncpus > 0) {
                ++nevents;
                ret = 0;
            } else {
                free_event(curr);
                ret = -E_PERFEVENT_RUNTIME;
            }
        }
    }


    inst->events = events;
    inst->nevents = nevents;
    return ret;
}

#define START_STATE 0
#define IN_LIST_STATE 1
static char punctuator(int *state)
{
    char c=' ';
    switch(*state)
    {
        case START_STATE:
            *state = IN_LIST_STATE;
            c = '(';
            break;
        case IN_LIST_STATE:
            c = ',';
            break;
    }
    return c;
}

static void log_events_for_pmu(pfm_pmu_info_t *pinfo)
{
    pfm_event_info_t einfo;
    pfm_event_attr_info_t ainfo;
    int i,k;
    pfm_err_t ret;

    memset(&einfo, 0, sizeof(einfo));
    einfo.size = sizeof(einfo);
    memset(&ainfo, 0, sizeof(ainfo));
    ainfo.size = sizeof(ainfo);

    for (i = pinfo->first_event; i != -1; i = pfm_get_event_next(i)) 
    {
        ret = pfm_get_event_info(i, PFM_OS_PERF_EVENT_EXT, &einfo);
        if (ret != PFM_SUCCESS)
            continue;

        if(einfo.pmu == pinfo->pmu)
        {
            fprintf(stderr, "%s::%s ", pinfo->name, einfo.name );
            int state = START_STATE;

            pfm_for_each_event_attr(k, &einfo) 
            {
                ret = pfm_get_event_attr_info(einfo.idx, k, PFM_OS_PERF_EVENT_EXT, &ainfo);
                if (ret != PFM_SUCCESS) {
                    fprintf(stderr, "cannot get attribute info: %s", pfm_strerror(ret));
                    continue;
                }

                if (ainfo.type == PFM_ATTR_UMASK)
                {
                    fprintf(stderr, "%c%s", punctuator(&state), ainfo.name);
                }
            }

            if(state == IN_LIST_STATE) fprintf(stderr, ")");
            fprintf(stderr, "\n");
        }
    }
}

static int enumerate_active_pmus(char **activepmus, int logevents)
{
    pfm_pmu_info_t pinfo;
    pfm_pmu_t j;
    pfm_err_t ret;
    int nactive = 0;

    memset(&pinfo, 0, sizeof(pinfo));
    pinfo.size = sizeof(pinfo);

    /*
     * enumerate all detected PMU models
     */
    pfm_for_all_pmus(j) 
    {
        ret = pfm_get_pmu_info(j, &pinfo);
        if (ret != PFM_SUCCESS)
            continue;

        if (0 == pinfo.is_present)
            continue;

        fprintf(stderr, "Found PMU: %s (%s) identification %d (%d events %d generic counters %d fixed counters)\n",
                pinfo.name, pinfo.desc, pinfo.pmu, pinfo.nevents, pinfo.num_cntrs, pinfo.num_fixed_cntrs);
        activepmus[nactive] = strdup(pinfo.name);
        ++nactive;

        if(logevents)
            log_events_for_pmu(&pinfo);
    }

    return nactive;
}

static pmcsetting_t *search_active_pmus(char **activepmus, int nactive, configuration_t *perfconfig)
{
    int i, j;
    pmctype_t *entry = NULL;

    for(i = 0; i < perfconfig->nConfigEntries; ++i)
    {
        for(entry = perfconfig->configArr[i].pmcTypeList; entry; entry = entry->next)
        {
            for(j = 0; j < nactive; ++j)
            {
                if( 0 == strcmp(entry->name, activepmus[j]) )
                {
                    fprintf(stderr, "Using configuration entry [%s]\n", entry->name);
                    return perfconfig->configArr[i].pmcSettingList;
                }
            }
        }        
    }

    return NULL;
}

static pmcsetting_t *find_perf_settings(configuration_t *perfconfig)
{
    char *activepmus[PFM_PMU_MAX + 1];
    int nactive = 0;
    int i;
    pmcsetting_t *ret = NULL;

    if((NULL == perfconfig) || (perfconfig->nConfigEntries < 1) )
    {
        return NULL;
    }
    
    nactive = enumerate_active_pmus(activepmus, 1);

    ret = search_active_pmus(activepmus, nactive, perfconfig);

    for(i = 0; i < nactive; ++i)
    {
        free(activepmus[i]);
    }

    return ret;
}

static uint64_t scaled_value_delta(eventcpuinfo_t *info)
{
    uint64_t dv;
    double dt_r, dt_e;

    dv   = info->values[RAW_VALUE] - info->previous[RAW_VALUE];
    dt_r = info->values[TIME_RUNNING] - info->previous[TIME_RUNNING];
    dt_e = info->values[TIME_ENABLED] - info->previous[TIME_ENABLED];

    info->previous[RAW_VALUE] = info->values[RAW_VALUE];
    info->previous[TIME_RUNNING] = info->values[TIME_RUNNING];
    info->previous[TIME_ENABLED] = info->values[TIME_ENABLED];

    if( (dt_r > dt_e) || ( dt_r == 0 ) )
    {
        return dv;
    }

    double scale = dt_e / dt_r;

    return dv * scale;
}

void perf_event_destroy(perfhandle_t *inst)
{
    perfdata_t *del = (perfdata_t *)inst;
    free_perfdata(del);
    rapl_destroy();
}

int perf_counter_enable(perfhandle_t *inst, int enable)
{
    int idx, cpuidx, ret;
    int n = 0;
    perfdata_t *pdata = (perfdata_t *)inst;

    for(idx = 0; idx < pdata->nevents; ++idx)
    {
        event_t *event = &pdata->events[idx];
	if (event->disable_event) {
	    ++n;
	    continue;
	}

        for(cpuidx = 0; cpuidx < event->ncpus; ++cpuidx)
        {
            eventcpuinfo_t *info = &event->info[cpuidx];

            if( info->type == EVENT_TYPE_PERF && info->fd >= 0 ) 
            {
                int request = (enable == PERF_COUNTER_ENABLE) ? PERF_EVENT_IOC_ENABLE : PERF_EVENT_IOC_DISABLE;
                ret = ioctl(info->fd, request, 0);
                if( ret == -1 )
                {
                    fprintf(stderr, "ioctl failed for cpu%d for \"%s\": %s\n", info->cpu, event->name, strerror(errno) );
                }
                else
                {
                    ++n;
                }
            }
        }
    }

    return n;
}

static perf_counter *get_counter(perf_counter **counters, int size, const char *str)
{
    perf_counter *pcounter = *counters;
    int idx, ncounters = size;

    for (idx = 0; idx < ncounters; idx++) {
        if (!strcmp(pcounter[idx].name, str)) {
            return (pcounter + idx);
        }
    }
    return NULL;
}

static int perf_derived_get(perf_derived_counter **derived_counters,
                            int *derived_size, perfdata_t *pdata,
                            perf_counter **counters, int *size)
{
    int idx, cpuidx;

    perf_derived_counter *pdcounter = *derived_counters;
    int nderivedcounters = *derived_size;

    if(NULL == pdcounter || nderivedcounters != pdata->nderivedevents)
    {
        pdcounter = malloc(pdata->nderivedevents * sizeof *pdcounter);
        if (NULL == pdcounter) {
            return -E_PERFEVENT_REALLOC;
        }
        memset(pdcounter, 0, pdata->nderivedevents * sizeof *pdcounter);
        nderivedcounters = pdata->nderivedevents;

        for (idx = 0; idx < pdata->nderivedevents; idx++) {
            derived_event_t *derived_event = &pdata->derived_events[idx];
            event_list_t *event_list = derived_event->event_list;
            perf_counter_list *counter_list, *ptr, *curr;
            perf_counter *counter;

            counter_list = ptr =curr = NULL;
            pdcounter[idx].name = derived_event->name;

            while (event_list != NULL) {
                counter = get_counter(counters, *size, event_list->event->name);
                if (counter != NULL) {
                    ptr = calloc(1, sizeof(*ptr));
                    if (!ptr)
                        return -E_PERFEVENT_REALLOC;
                    ptr->counter = counter;
                    ptr->scale = event_list->scale;
                    ptr->next = NULL;
                    if (counter_list == NULL) {
                        counter_list = ptr;
                        curr = ptr;
                    } else {
                        curr->next = ptr;
                        curr = curr->next;
                    }
                }
                event_list = event_list->next;
            }
            /*
             * For every counter in a derived_event, we have ninstances and
             * they should match
             */
            pdcounter[idx].counter_list = counter_list;
            if (pdcounter[idx].counter_list != NULL)
                pdcounter[idx].ninstances = (pdcounter[idx].counter_list)->counter->ninstances;
            pdcounter[idx].data = calloc(pdcounter[idx].ninstances, sizeof(uint64_t));
        }
        *derived_counters = pdcounter;
        *derived_size = nderivedcounters;
    }

    if (pdcounter) {
        nderivedcounters = *derived_size;

        for (idx = 0; idx < nderivedcounters; idx++) {
            perf_counter_list *clist;
            perf_counter *ctr;

            for (cpuidx = 0; cpuidx < pdcounter[idx].ninstances; cpuidx++) {
                pdcounter[idx].data[cpuidx].value = 0;
                clist = pdcounter[idx].counter_list;
                while(clist) {
                    ctr = clist->counter;
                    pdcounter[idx].data[cpuidx].value += (ctr->data[cpuidx].value *
                                                          clist->scale);
                    clist = clist->next;
                }
            }
        }
    }

    return 0;
}

int perf_get(perfhandle_t *inst, perf_counter **counters, int *size,
             perf_derived_counter **derived_counters, int *derived_size)
{
    int cpuidx, idx, events_read;

    if(NULL == inst)
    {
        return -E_PERFEVENT_LOGIC;
    }

    if(NULL == counters)
    {
        return -E_PERFEVENT_LOGIC;
    }

    perfdata_t *pdata = (perfdata_t *)inst;
    perf_counter *pcounter = *counters;
    int ncounters = *size;

    if(NULL == pcounter || ncounters != pdata->nevents)
    {
        pcounter = malloc(pdata->nevents * sizeof *pcounter);
        memset(pcounter, 0, pdata->nevents * sizeof *pcounter);
        ncounters = pdata->nevents;
    }

    events_read = 0;
    for(idx = 0; idx < pdata->nevents; ++idx)
    {
        event_t *event = &pdata->events[idx];

        pcounter[idx].name = event->name;
	pcounter[idx].counter_disabled = event->disable_event;
	if (event->disable_event) {
	    continue;
	}

        if(0 == pcounter[idx].data)
        {
            pcounter[idx].data = malloc(event->ncpus * sizeof *pcounter[idx].data );
            memset(pcounter[idx].data, 0, event->ncpus * sizeof *pcounter[idx].data);
            pcounter[idx].ninstances = event->ncpus;
        }

        for(cpuidx = 0; cpuidx < event->ncpus; ++cpuidx)
        {
            eventcpuinfo_t *info = &event->info[cpuidx];

            int ret;

            if( info->type == EVENT_TYPE_PERF ) {
                ret = read(info->fd, info->values, sizeof(info->values));
                if (ret != sizeof(info->values)) {
                    if (ret == -1)
                        fprintf(stderr, "cannot read event %s on cpu %d:%d\n", event->name, info->cpu, ret);
                    else
                        fprintf(stderr, "could not read event %s on cpu %d\n", event->name, info->cpu);
                    continue;
                }
                else
                {
                    ++events_read;
                }

                pcounter[idx].data[cpuidx].value += scaled_value_delta(info);
                pcounter[idx].data[cpuidx].time_enabled = info->values[TIME_ENABLED];
                pcounter[idx].data[cpuidx].time_running = info->values[TIME_RUNNING];
                pcounter[idx].data[cpuidx].id = info->cpu;
            } else {

                ret = rapl_read( &info->rapldata, &info->values[0] );
                if ( ret != 0 ) {
                    fprintf(stderr, "cannot read event %s on cpu %d:%d\n", event->name, info->cpu, ret);
                    continue;
                }

                pcounter[idx].data[cpuidx].value = info->values[0];
                pcounter[idx].data[cpuidx].time_enabled = 1;
                pcounter[idx].data[cpuidx].time_running = 1;
                pcounter[idx].data[cpuidx].id = info->cpu;
            }
        }
    }

    *counters = pcounter;
    *size = ncounters;

    if (pcounter != 0)
        perf_derived_get(derived_counters, derived_size, pdata, counters, size);

    return events_read;
}

perfhandle_t *perf_event_create(const char *config_file)
{
    int ret, i;
    perfdata_t *inst = 0;
    configuration_t *perfconfig = 0;
    pmcsetting_t *pmcsetting = 0;
    pmcderived_t *derivedpmc = 0;
    struct pmu *pmu_list = NULL;

    ret = pfm_initialize();
    if (ret != PFM_SUCCESS)
    {
        fprintf(stderr, "pfm_initialize failed %d\n", ret);
        return 0;
    }

    inst = malloc(sizeof(perfdata_t));
    if(!inst)
    {
        return 0;
    }
    memset(inst, 0, sizeof *inst);

    rapl_init();

    inst->archinfo = get_architecture();

    perfconfig = parse_configfile(config_file);
    if(NULL == perfconfig)
    {
        fprintf(stderr, "parse_configfile failed to parse \"%s\"\n", config_file);
        goto out;
    }

    pmcsetting = find_perf_settings(perfconfig);
    if(NULL == pmcsetting)
    {
        fprintf(stderr, "find_perf_settings unable to find suitable config entry\n");
    }

    while(pmcsetting)
    {
        (void) perf_setup_event(inst, pmcsetting->name, pmcsetting->cpuConfig);

        pmcsetting = pmcsetting->next;
    }

    /* Setup the dynamic events */
    if (perfconfig->dynamicpmc) {
	ret = init_dynamic_events(&pmu_list);
	if (!ret)
	    perf_setup_dynamic_events(inst,
				      perfconfig->dynamicpmc->dynamicSettingList,
				      pmu_list);
    }
    /* Setup the derived events */
    if (inst && perfconfig && perfconfig->nDerivedEntries)
    {
        for (i = 0; i < perfconfig->nDerivedEntries; i++) {
            int ret;
            derivedpmc = &(perfconfig->derivedArr[i]);
            ret = perf_setup_derived_event(inst, derivedpmc);
            if (ret < 0)
                fprintf(stderr, "Unable to setup derived event : %s\n", derivedpmc->name);
        }
    }

    free_configuration(perfconfig);

out:

    if(0 == inst->nevents)
    {
        free_perfdata(inst);
        rapl_destroy();
        inst = 0;
    }

    return (perfhandle_t *)inst;
}

void perf_counter_destroy(perf_counter *data, int size, perf_derived_counter *derived_counters, int derived_size)
{
    if(NULL == data)
    {
        return;
    }

    int i;
    for(i = 0; i < size; ++i)
    {
        free(data[i].data);
    }

    if (NULL == derived_counters)
    {
        return;
    }
    for (i = 0; i < derived_size; ++i)
        {
            perf_counter_list *tmp, *clist = NULL;

            free(derived_counters[i].name);
            free(derived_counters[i].data);
            tmp = clist = derived_counters[i].counter_list;
            while (clist) {
                clist = clist->next;
                free(tmp);
                tmp = clist;
            }
        }

    free(data);
}

