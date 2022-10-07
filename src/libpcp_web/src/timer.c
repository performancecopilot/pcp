/*
 * Copyright (c) 2021-2022 Red Hat.
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
#include "load.h"
#include "libpcp.h"
#include "mmv_stats.h"
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

typedef enum server_metric {
    SERVER_PID,
    SERVER_CPU_USR,
    SERVER_CPU_SYS,
    SERVER_CPU_TOT,
    SERVER_MEM_MAXRSS,
    SERVER_MEM_DATASZ,
    NUM_SERVER_METRIC
} server_metric_t;

typedef struct timer_callback {
    int				seq;
    void			*data;
    pmWebTimerCallBack		callback;
    struct timer_callback	*next;
} timer_callback_t;

typedef struct timer_server {
    uv_timer_t		timer;
    int			seq;
    uv_loop_t		*events;
    uv_mutex_t		callback_mutex;
    timer_callback_t	*callback_list;
    mmv_registry_t	*registry;	/* generic server metrics */
    pmAtomValue		*metrics[NUM_SERVER_METRIC]; /* direct lookup table */
} timer_server_t;

static timer_server_t server;

int
pmWebTimerSetup(void)
{
    /* initialize base sbrk */
    __pmProcessDataSize(NULL);
    uv_mutex_init(&server.callback_mutex);
    return 0;
}

int
pmWebTimerSetEventLoop(void *events)
{
    server.events = (uv_loop_t *)events;
    uv_timer_init(server.events, &server.timer);
    return 0;
}

void
pmWebTimerClose(void)
{
    timer_callback_t	*timer, *p = NULL;

    uv_mutex_lock(&server.callback_mutex);
    timer = server.callback_list;
    while (timer) {
	p = timer->next;
	free(timer);
	timer = p;
    }
    server.callback_list = NULL;
    uv_timer_stop(&server.timer);
    uv_mutex_unlock(&server.callback_mutex);
}

static void
timer_worker(uv_timer_t *arg)
{
    timer_callback_t	*timer;

    uv_mutex_lock(&server.callback_mutex);
    for (timer = server.callback_list; timer; timer = timer->next) {
    	if (timer->callback)
	    timer->callback(timer->data);
    }
    uv_mutex_unlock(&server.callback_mutex);
}

/*
 * Register given callback function and its private data in the
 * global timer list. Callbacks must be non-blocking and short,
 * e.g. to refresh instrumentation, garbage collection, etc.
 */
int
pmWebTimerRegister(pmWebTimerCallBack callback, void *data)
{
    timer_callback_t	*timer;

    if ((timer = malloc(sizeof(timer_callback_t))) == NULL)
    	return -ENOMEM;

    timer->seq = server.seq++;
    timer->data = data;
    timer->callback = callback;
    uv_mutex_lock(&server.callback_mutex);
    if (server.callback_list == NULL)
	uv_timer_start(&server.timer, timer_worker, 1000, 1000);
    timer->next = server.callback_list;
    server.callback_list = timer;
    uv_mutex_unlock(&server.callback_mutex);

    return timer->seq;
}

/*
 * Release a previously registered timer. If there are no
 * remaining registered timers, stop the global uv timer.
 */
int
pmWebTimerRelease(int seq)
{
    timer_callback_t	*timer, *p = NULL;
    int			sts = -ESRCH;

    uv_mutex_lock(&server.callback_mutex);
    for (timer = server.callback_list; timer; p = timer, timer = timer->next) {
	if (timer->seq == seq) {
	    if (timer == server.callback_list)
	    	server.callback_list = timer->next;
	    else
	    	p->next = timer->next;
	    free(timer);
	    /* success */
	    sts = 0;
	    break;
	}
    }
    if (server.callback_list == NULL)
	uv_timer_stop(&server.timer);
    uv_mutex_unlock(&server.callback_mutex);

    return sts;
}

/*
 * timer callback to refresh generic server metrics
 */
static void
server_metrics_refresh(void *map)
{
    double		usr, sys;
    unsigned long long	datasz = 0;
#ifdef HAVE_GETRUSAGE
    struct rusage	usage = {0};

    (void)getrusage(RUSAGE_SELF, &usage);
#endif

    __pmProcessDataSize((unsigned long*) &datasz);
    __pmProcessRunTimes(&usr, &sys);
    usr *= 1000.0; /* milliseconds */
    sys *= 1000.0;

    /* exported as uint64 but manipulated as double */
    mmv_set_value(map, server.metrics[SERVER_CPU_USR], usr);
    mmv_set_value(map, server.metrics[SERVER_CPU_SYS], sys);
    mmv_set_value(map, server.metrics[SERVER_CPU_TOT], usr + sys);

#ifdef HAVE_GETRUSAGE
    mmv_set(map, server.metrics[SERVER_MEM_MAXRSS], &usage.ru_maxrss);
#endif

    /* exported as uint64 but manipulated as ulong/ulong long */
    mmv_set(map, server.metrics[SERVER_MEM_DATASZ], &datasz);
}

/*
 * Register and set up generic server metrics.
 */
int
pmWebTimerSetMetricRegistry(struct mmv_registry *registry)
{
    pmUnits		nounits = MMV_UNITS(0,0,0,0,0,0);
    pmUnits		units_kbytes = MMV_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0);
    pmUnits		units_msec = MMV_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0);
    pid_t		pid = getpid();
    char		buffer[64];
    void		*map;

    if (server.registry) {
	pmNotifyErr(LOG_ERR, "%s: server instrumentation already registered\n",
		"pmWebTimerSetMetricRegistry");
    	return -EINVAL;
    }

    server.registry = registry;
    mmv_stats_add_metric(registry, "pid", SERVER_PID,
		MMV_TYPE_U32, MMV_SEM_DISCRETE, nounits, MMV_INDOM_NULL,
		"PID for the current process",
		"Process identifier for the current process");
    pmsprintf(buffer, sizeof(buffer), "%u", pid);
    mmv_stats_add_metric_label(registry, SERVER_PID,
		"pid", buffer, MMV_NUMBER_TYPE, 0);

    mmv_stats_add_metric(registry, "cpu.user", SERVER_CPU_USR,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_msec, MMV_INDOM_NULL,
	"user CPU time",
	"Cumulative process user CPU time from times(2)");

    mmv_stats_add_metric(registry, "cpu.sys", SERVER_CPU_SYS,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_msec, MMV_INDOM_NULL,
	"system CPU time",
	"Cumulative process system CPU time from times(2)");

    mmv_stats_add_metric(registry, "cpu.total", SERVER_CPU_TOT,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_msec, MMV_INDOM_NULL,
	"system + user CPU time",
	"Cumulative process system and user CPU time from times(2)");

    mmv_stats_add_metric(registry, "mem.maxrss", SERVER_MEM_MAXRSS,
	MMV_TYPE_U64, MMV_SEM_INSTANT, units_kbytes, MMV_INDOM_NULL,
	"maximum RSS",
	"Maximum memory resident set size from getrusage(2)");

    mmv_stats_add_metric(registry, "mem.datasz", SERVER_MEM_DATASZ,
	MMV_TYPE_U64, MMV_SEM_INSTANT, units_kbytes, MMV_INDOM_NULL,
	"virtual data size",
	"Process memory virtual data size from sbrk(2)");

    if ((map = mmv_stats_start(registry)) == NULL) {
	pmNotifyErr(LOG_ERR, "%s: server instrumentation disabled",
		"pmWebTimerSetMetricRegistry");
	return -EINVAL;
    }

    server.metrics[SERVER_PID] = mmv_lookup_value_desc(map, "pid", NULL);
    server.metrics[SERVER_CPU_USR] = mmv_lookup_value_desc(map, "cpu.user", NULL);
    server.metrics[SERVER_CPU_SYS] = mmv_lookup_value_desc(map, "cpu.sys", NULL);
    server.metrics[SERVER_CPU_TOT] = mmv_lookup_value_desc(map, "cpu.total", NULL);
    server.metrics[SERVER_MEM_MAXRSS] = mmv_lookup_value_desc(map, "mem.maxrss", NULL);
    server.metrics[SERVER_MEM_DATASZ] = mmv_lookup_value_desc(map, "mem.datasz", NULL);

    /* PID doesn't change, set it once */
    mmv_set(map, server.metrics[SERVER_PID], &pid);

    /* register the refresh timer */
    return pmWebTimerRegister(server_metrics_refresh, map);
}
