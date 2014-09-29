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
    perf_data *data;
    int ninstances;
} perf_counter;

typedef intptr_t perfhandle_t;

perfhandle_t *perf_event_create(const char *configfile);

void perf_counter_destroy(perf_counter *data, int size);

void perf_event_destroy(perfhandle_t *inst);

#define PERF_COUNTER_ENABLE 0
#define PERF_COUNTER_DISABLE 1
int perf_counter_enable(perfhandle_t *inst, int enable);

int perf_get(perfhandle_t *inst, perf_counter **data, int *size);

#define E_PERFEVENT_LOGIC 1
#define E_PERFEVENT_REALLOC 2
#define E_PERFEVENT_RUNTIME 3

const char *perf_strerror(int err);

#endif /* PERFINTERFACE_H_ */
