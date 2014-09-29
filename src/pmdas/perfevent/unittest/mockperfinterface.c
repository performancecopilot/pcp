#include "perfinterface.h"
#include <stdlib.h>
#include <stdio.h>

perfhandle_t *perf_event_create(const char *configfile)
{
    return malloc(1);
}

void perf_counter_destroy(perf_counter *data, int size)
{
}

void perf_event_destroy(perfhandle_t *inst)
{
    free(inst);
}

int perf_counter_enable(perfhandle_t *inst, int enable)
{
    fprintf(stderr,"perf_counter_enable -> %s\n", (enable == PERF_COUNTER_ENABLE) ? "ENABLED" : "DISABLED" );
    return 0;
}

int perf_get(perfhandle_t *inst, perf_counter **data, int *size)
{
    return -E_PERFEVENT_RUNTIME;
}

const char *perf_strerror(int err)
{
    return "fake error";
}
