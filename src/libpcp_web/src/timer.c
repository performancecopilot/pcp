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

/* direct lookup table */
static pmAtomValue *server_values[NUM_SERVER_METRIC];

typedef struct timerCallback {
    int				seq;
    void			*data;
    pmWebTimerCallBack		callback;
    struct timerCallback	*next;
} timerCallback;

static timerCallback	*timerCallbackList;
static uv_timer_t	pmwebapi_timer;
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
pmWebTimerRegister(pmWebTimerCallBack callback, void *data)
{
    static int		seq = 0;
    timerCallback	*timer = (timerCallback *)malloc(sizeof(timerCallback));

    if (timer == NULL)
    	return -ENOMEM;
    timer->seq = seq++;
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

    return timer->seq;
}

/*
 * Release a previously registered timer. If there are no
 * remaining registered timers, stop the global uv timer.
 */
int
pmWebTimerRelease(int seq)
{
    int			sts = -EINVAL;
    timerCallback	*timer, *prev = NULL;

    uv_mutex_lock(&timerCallbackMutex);
    for (timer = timerCallbackList; timer; prev = timer, timer = timer->next) {
	if (timer->seq == seq) {
	    if (timer == timerCallbackList)
	    	timerCallbackList = timer->next;
	    else
	    	prev->next = timer->next;
	    free(timer);
	    /* success */
	    sts = 0;
	    break;
	}
    }
    if (timerCallbackList == NULL)
	uv_timer_stop(&pmwebapi_timer);
    uv_mutex_unlock(&timerCallbackMutex);

    return sts;
}

/*
 * timer callback to refresh generic server metrics
 */
static void
server_metrics_refresh(void *map)
{
    double		usr, sys;
    unsigned long	datasz = 0;
    struct rusage	usage = {0};

    __pmProcessDataSize(&datasz);
    __pmProcessRunTimes(&usr, &sys);
    usr *= 1000.0; /* milliseconds */
    sys *= 1000.0;
    if (getrusage(RUSAGE_SELF, &usage) < 0)
	return;

    if (server_values[SERVER_CPU_USR])
	mmv_set_value(map, server_values[SERVER_CPU_USR], usr);
    if (server_values[SERVER_CPU_SYS])
	mmv_set_value(map, server_values[SERVER_CPU_SYS], sys);
    if (server_values[SERVER_CPU_TOT])
	mmv_set_value(map, server_values[SERVER_CPU_TOT], usr+sys);
    if (server_values[SERVER_MEM_MAXRSS])
	mmv_set_value(map, server_values[SERVER_MEM_MAXRSS], usage.ru_maxrss);
    if (server_values[SERVER_MEM_DATASZ])
	mmv_set_value(map, server_values[SERVER_MEM_DATASZ], (double)datasz);
}

/*
 * Register and set up generic server metrics.
 * See pmproxy/src/server.c for calling example.
 */
int
pmWebTimerSetMetricRegistry(struct mmv_registry *registry)
{
    pmInDom		noindom = MMV_INDOM_NULL;
    pmUnits		nounits = MMV_UNITS(0,0,0,0,0,0);
    pmUnits		units_kbytes = MMV_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0);
    pmUnits		units_msec = MMV_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0);
    pid_t		pid = getpid();
    char		buffer[64];

    if (server_registry) {
	pmNotifyErr(LOG_ERR, "%s: server instrumentation already registered\n",
		"pmWebTimerSetMetricRegistry");
    	return -EINVAL;
    }

    server_registry = registry;
    mmv_stats_add_metric(registry, "pid", SERVER_PID,
		MMV_TYPE_U32, MMV_SEM_DISCRETE, nounits, noindom,
		"Identifier",
		"Identifier for the current process");
    pmsprintf(buffer, sizeof(buffer), "%u", pid);
    mmv_stats_add_metric_label(registry, SERVER_PID,
		"pid", buffer, MMV_NUMBER_TYPE, 0);

    mmv_stats_add_metric(registry, "cpu.user", SERVER_CPU_USR,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_msec, noindom,
	"user CPU time",
	"process user CPU time counter");

    mmv_stats_add_metric(registry, "cpu.sys", SERVER_CPU_SYS,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_msec, noindom,
	"system CPU time",
	"process system CPU time counter");

    mmv_stats_add_metric(registry, "cpu.total", SERVER_CPU_TOT,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_msec, noindom,
	"system + user CPU time",
	"process system + user CPU time");

    mmv_stats_add_metric(registry, "mem.maxrss", SERVER_MEM_MAXRSS,
	MMV_TYPE_U64, MMV_SEM_INSTANT, units_kbytes, noindom,
	"maximum RSS",
	"process maximum resident set memory size");

    mmv_stats_add_metric(registry, "mem.datasz", SERVER_MEM_DATASZ,
	MMV_TYPE_U64, MMV_SEM_INSTANT, units_kbytes, noindom,
	"virtual data size",
	"process data memory size, returned from sbrk(2)");

    if ((server_map = mmv_stats_start(registry)) == NULL) {
	pmNotifyErr(LOG_ERR, "%s: server instrumentation disabled",
		"pmWebTimerSetMetricRegistry");
	return -EINVAL;
    }

    /* PID doesn't change, set it once */
    if ((server_values[SERVER_PID] = mmv_lookup_value_desc(server_map, "pid", NULL)) != NULL)
	mmv_set_value(server_map, server_values[SERVER_PID], pid);
    server_values[SERVER_CPU_USR] = mmv_lookup_value_desc(server_map, "cpu.user", NULL);
    server_values[SERVER_CPU_SYS] = mmv_lookup_value_desc(server_map, "cpu.sys", NULL);
    server_values[SERVER_CPU_TOT] = mmv_lookup_value_desc(server_map, "cpu.total", NULL);
    server_values[SERVER_MEM_MAXRSS] = mmv_lookup_value_desc(server_map, "mem.maxrss", NULL);
    server_values[SERVER_MEM_DATASZ] = mmv_lookup_value_desc(server_map, "mem.datasz", NULL);

    /* initialize base sbrk */
    __pmProcessDataSize(NULL);

    /* register the refresh timer */
    return pmWebTimerRegister(server_metrics_refresh, server_map);
}
