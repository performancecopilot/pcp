/*
 * Copyright (c) 2021 Red Hat.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include <assert.h>
#include <sys/resource.h>
#include "pmapi.h"
#include "pmda.h"
#include "pmwebapi.h"
#include "schema.h"

typedef enum server_metric {
    SERVER_PID,
    SERVER_CPU_USR,
    SERVER_CPU_SYS,
    SERVER_CPU_TOT,
    SERVER_MEM_MAXRSS,
    SERVER_MEM_DATASZ,
    NUM_SERVER_METRIC
} server_metric;

typedef struct timerCallback {
    void			*data;
    pmServerTimerCallBack	callback;
    struct timerCallback	*next;
} timerCallback;

static timerCallback	*timerCallbackList;
static uv_timer_t	pmwebapi_timer;
static int		callseq;
static uv_mutex_t	timerCallbackMutex = PTHREAD_MUTEX_INITIALIZER;
static struct mmv_registry *server_registry; /* global generic server metrics */
static void		*server_map;

static void
webapi_timer_worker(uv_timer_t *arg)
{
    timerCallback	*timer;
    uv_handle_t         *handle = (uv_handle_t *)arg;

    (void)handle;
    uv_mutex_lock(&timerCallbackMutex);
    callseq++;
    for (timer = timerCallbackList; timer; timer = timer->next) {
    	if (timer->callback)
	    timer->callback(timer->data);
    }
    uv_mutex_unlock(&timerCallbackMutex);
}

/*
 * Register given callback function and it's private data in the
 * global timer list. Callbacks must be non-blocking and short,
 * e.g. to refresh instrumentation, garbage collection, etc.
 */
int
pmServerRegisterTimer(pmServerTimerCallBack callback, void *data)
{
    timerCallback	*timer = (timerCallback *)malloc(sizeof(timerCallback));

    if (timer == NULL)
    	return -ENOMEM;
    timer->data = data;
    timer->callback = callback;
    uv_mutex_lock(&timerCallbackMutex);
    if (timerCallbackList == NULL) {
    	uv_timer_init(uv_default_loop(), &pmwebapi_timer);
	uv_timer_start(&pmwebapi_timer, webapi_timer_worker, 1000, 1000);
    }
    timer->next = timerCallbackList;
    timerCallbackList = timer;
    uv_mutex_unlock(&timerCallbackMutex);

    return 0;
}

int
pmServerReleaseTimer(void *data)
{
    timerCallback	*timer;

    uv_mutex_lock(&timerCallbackMutex);
    /* TODO */
    uv_mutex_unlock(&timerCallbackMutex);

    return -EINVAL; /* not found */
}

/* stop timer and free all timers */
void
pmServerReleaseAllTimers(void)
{
    timerCallback	*timer, *next;

    uv_mutex_lock(&timerCallbackMutex);
    uv_timer_stop(&pmwebapi_timer);
    for (timer = timerCallbackList; timer; timer = next) {
	next = timer->next;
	free(timer);
    }
    timerCallbackList = NULL;
    uv_mutex_unlock(&timerCallbackMutex);
}

/*
 * timer callback to refresh generic server metrics
 */
static void
server_metrics_refresh(void *data)
{
    double		usr, sys;
    unsigned long	datasz = 0;
    struct rusage	usage = {0};
    pmAtomValue		*value;

    __pmProcessDataSize(&datasz);
    __pmProcessRunTimes(&usr, &sys);
    usr *= 1000.0; /* milliseconds */
    sys *= 1000.0;
    if (getrusage(RUSAGE_SELF, &usage) < 0)
	return;

    if ((value = mmv_lookup_value_desc(server_map, "cpu.user", NULL)) != NULL)
	mmv_set_value(server_map, value, usr);
    if ((value = mmv_lookup_value_desc(server_map, "cpu.sys", NULL)) != NULL)
	mmv_set_value(server_map, value, sys);
    if ((value = mmv_lookup_value_desc(server_map, "cpu.total", NULL)) != NULL)
	mmv_set_value(server_map, value, usr+sys);
    if ((value = mmv_lookup_value_desc(server_map, "mem.maxrss", NULL)) != NULL)
	mmv_set_value(server_map, value, usage.ru_maxrss);
    if ((value = mmv_lookup_value_desc(server_map, "mem.datasz", NULL)) != NULL)
	mmv_set_value(server_map, value, (double)datasz);
}

/*
 * Register and set up generic server metrics.
 * See pmproxy/src/server.c for calling example.
 */
int
pmServerSetMetricRegistry(struct mmv_registry *registry)
{

    pmAtomValue		*value;
    pmInDom		noindom = MMV_INDOM_NULL;
    pmUnits		nounits = MMV_UNITS(0,0,0,0,0,0);
    pmUnits		units_kbytes = MMV_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0);
    pmUnits		units_msec = MMV_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0);

    pid_t		pid = getpid();
    char		buffer[64];

    if (server_registry) {
	fprintf(stderr, "%s: Error: server instrumentation already registered\n", pmGetProgname());
    	return -EINVAL;
    }

    server_registry = registry;
    mmv_stats_add_metric(registry, "pid", SERVER_PID,
		MMV_TYPE_U32, MMV_SEM_DISCRETE, nounits, noindom,
		"server PID",
		"PID for the current server invocation");
    pmsprintf(buffer, sizeof(buffer), "%u", pid);
    mmv_stats_add_metric_label(registry, SERVER_PID,
		"pid", buffer, MMV_NUMBER_TYPE, 0);

    mmv_stats_add_metric(registry, "cpu.user", SERVER_CPU_USR,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_msec, noindom,
	"server user CPU",
	"server process user CPU time counter");

    mmv_stats_add_metric(registry, "cpu.sys", SERVER_CPU_SYS,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_msec, noindom,
	"server system CPU",
	"server process system CPU time counter");

    mmv_stats_add_metric(registry, "cpu.total", SERVER_CPU_TOT,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_msec, noindom,
	"server system + user CPU",
	"server process system + user CPU time");

    mmv_stats_add_metric(registry, "mem.maxrss", SERVER_MEM_MAXRSS,
	MMV_TYPE_U64, MMV_SEM_INSTANT, units_kbytes, noindom,
	"server maximum RSS",
	"server process maximum resident set memory size");

    mmv_stats_add_metric(registry, "mem.datasz", SERVER_MEM_DATASZ,
	MMV_TYPE_U64, MMV_SEM_INSTANT, units_kbytes, noindom,
	"server virtual data size",
	"server process data memory size, returned from sbrk(2)");

    if ((server_map = mmv_stats_start(registry)) == NULL) {
	fprintf(stderr, "%s: Error: server instrumentation disabled\n", pmGetProgname());
	return -EINVAL;
    }

    /* PID doesn't change, register it once */
    if ((value = mmv_lookup_value_desc(server_map, "pid", NULL)) != NULL)
	mmv_set_value(server_map, value, pid);

    /* register the refresh timer */
    pmServerRegisterTimer(server_metrics_refresh, server_map);

    /* success */
    return 0;
}
