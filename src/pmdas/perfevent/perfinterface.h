/*
 * perfevent interface
 *
 * Copyright (c) 2013 Joe White
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
#ifndef PERFINTERFACE_H_
#define PERFINTERFACE_H_

#include <stdint.h>
#include <perfmon/pfmlib_perf_event.h>
#include "rapl-interface.h"
#include "architecture.h"

typedef struct perf_data_t_
{
    uint64_t value;
    uint64_t time_enabled;
    uint64_t time_running;
    int id;
} perf_data;

typedef struct perf_counter_t_
{
    char *name;
    int counter_disabled;
    perf_data *data;
    int ninstances;
} perf_counter;

typedef struct perf_derived_data_t_
{
    double value;
} perf_derived_data;

typedef struct perf_counter_list_t_
{
    perf_counter *counter;
    double scale;
    struct perf_counter_list_t_ *next;
} perf_counter_list;

typedef struct perf_derived_counter_t_
{
    char *name;
    perf_derived_data *data;
    int ninstances;
    perf_counter_list *counter_list;
} perf_derived_counter;

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

typedef struct event_t_ {
    char *name;
    int disable_event;
    eventcpuinfo_t *info;
    int ncpus;
} event_t;

typedef struct event_list_t_ {
    event_t *event;
    double scale;
    struct event_list_t_ *next;
} event_list_t;

typedef struct derived_event_t_ {
    char *name;
    event_list_t *event_list;
} derived_event_t;

typedef struct dynamic_event_t_ {
    char *pmu;
    char *event;
    struct dynamic_event_t_ *next;
} dynamic_event_t;

typedef struct perfdata_t_
{
    int nevents;
    event_t *events;

    int nderivedevents;
    derived_event_t *derived_events;

    /* information about the architecture (number of cpus, numa nodes etc) */
    archinfo_t *archinfo;

    /* internal state to keep track of cpus for events added in 'round
     * robin' mode */
    int roundrobin_cpu_idx;
    int roundrobin_nodecpu_idx;
} perfdata_t;

typedef intptr_t perfhandle_t;

perfhandle_t *perf_event_create(const char *configfile);

void perf_counter_destroy(perf_counter *data, int size, perf_derived_counter *derived_counter, int derived_size);

void perf_event_destroy(perfhandle_t *inst);

#define PERF_COUNTER_ENABLE 0
#define PERF_COUNTER_DISABLE 1
int perf_counter_enable(perfhandle_t *inst, int enable);

int perf_get(perfhandle_t *inst, perf_counter **data, int *size, perf_derived_counter **derived_counter, int *derived_size);

#define E_PERFEVENT_LOGIC 1
#define E_PERFEVENT_REALLOC 2
#define E_PERFEVENT_RUNTIME 3

const char *perf_strerror(int err);

#endif /* PERFINTERFACE_H_ */
