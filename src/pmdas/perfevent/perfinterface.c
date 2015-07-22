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
#include <perfmon/pfmlib_perf_event.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define EVENT_TYPE_PERF 0
#define EVENT_TYPE_RAPL 1

typedef struct eventcpuinfo_t_ {
    uint64_t values[3];
    uint64_t previous[3];
    int type;
    int fd;
    perf_event_attr_t hw; /* perf_event_attr struct passed to perf_event_open() */
    int idx; /* opaque libpfm event identifier */
    char *fstr; /* fstr from library, must be freed */
    rapl_data_t rapldata;
    int cpu;
} eventcpuinfo_t;

#define RAW_VALUE 0
#define TIME_ENABLED 1
#define TIME_RUNNING 2

typedef struct event_t_ {
    char *name;
    eventcpuinfo_t *info;
    int ncpus;
} event_t;

typedef struct perfdata_t_
{
    int nevents;
    event_t *events;

    /* information about the architecture (number of cpus, numa nodes etc) */
    archinfo_t *archinfo;

    /* internal state to keep track of cpus for events added in 'round
     * robin' mode */
    int roundrobin_cpu_idx;
    int roundrobin_nodecpu_idx;
} perfdata_t;

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

int perf_get(perfhandle_t *inst, perf_counter **counters, int *size)
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

    return events_read;
}


perfhandle_t *perf_event_create(const char *config_file)
{
    int ret;
    perfdata_t *inst = 0;
    configuration_t *perfconfig = 0;
    pmcsetting_t *pmcsetting = 0;

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

void perf_counter_destroy(perf_counter *data, int size)
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

    free(data);
}

