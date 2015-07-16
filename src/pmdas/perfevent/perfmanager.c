/*
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

#include "perfmanager.h"

#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "perfinterface.h"
#include "perflock.h"

#define WAIT_TIME_NS (100 * 1000 * 1000 )

typedef struct monitor {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int running;

    pthread_mutex_t counter_mutex;
    int counter_state;
    int lockfp;

    int has_been_disabled;
    int first_time;

    perfhandle_t *perf;
} monitor_t;

typedef struct manager {
    pthread_t thread;
    monitor_t *monitor;
} manager_t;

/* \brief checkfile check the lock file to see if any locks are held
 * \returns 0 if the counters are to be enabled (no locks are held)
 *          1 if the counter should be disabled (at least 1 read lock is held)
 *          -1 if the call to fcntl failed
 */
static int checkfile(int fp)
{
	int res;
	struct flock fl;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;

	res = fcntl(fp, F_GETLK, &fl);

	if(res != -1) {
		if( fl.l_type == F_UNLCK) {
			return PERF_COUNTER_ENABLE;
		} else {
			return PERF_COUNTER_DISABLE;
		}
	} else {
		perror("fcntl");
		return -1;
	}
}

#define NANOSECS_PER_SEC (1000 * 1000 * 1000)

static void increment_ns(struct timespec *ts, int value_ns) 
{
    ts->tv_nsec += value_ns;
    if(ts->tv_nsec > NANOSECS_PER_SEC ) {
        ts->tv_sec += 1;
        ts->tv_nsec -= NANOSECS_PER_SEC;
    }
}

static monitor_t *monitor_init(int lockfp, perfhandle_t *perf)
{
    monitor_t *m;

    m = malloc( sizeof(monitor_t) );

    if ( 0 == m ) {
        return 0;
    }

    pthread_mutex_init(&m->mutex, NULL);
    pthread_cond_init(&m->cond, NULL);
    m->running = 1;
    pthread_mutex_init(&m->counter_mutex, NULL);
    m->counter_state = PERF_COUNTER_DISABLE;
    m->lockfp = lockfp;
    m->first_time = 1;
    m->has_been_disabled = 1;
    m->perf = perf;

    return m;
}

static void monitor_destroy(monitor_t *del)
{
    if(del->lockfp != -1) {
        close(del->lockfp);
    }
    pthread_mutex_destroy(&del->counter_mutex);
    pthread_cond_destroy(&del->cond);
    pthread_mutex_destroy(&del->mutex);

    free(del);
}

int perf_get_r(perfmanagerhandle_t *inst, perf_counter **data, int *size)
{
    monitor_t *m = ((manager_t *)inst)->monitor;
    int res = 0;
   
    pthread_mutex_lock( &m->counter_mutex );
    /* Read the performance counters if they are enabled or if this
     * is the first call (since the counter data is populated on the 
     * first call regardless of the enable/disable state */
    if(m->counter_state == PERF_COUNTER_ENABLE || m->first_time) 
    {
        res = perf_get(m->perf, data, size);
        m->first_time = 0;

        if(m->has_been_disabled)
        {
            /* The counters were disabled at some point between the last call
             * and this call. Reset the flag, and inform the caller that
             * the counter values can't be trusted
             */
            m->has_been_disabled = 0;
            res = 0;
        }
    }
    pthread_mutex_unlock( &m->counter_mutex );
    return res;
}

int perf_enabled(perfmanagerhandle_t *inst)
{
    manager_t *mgr = (manager_t *)inst;
    int res;
   
    pthread_mutex_lock( &mgr->monitor->counter_mutex );
    res = mgr->monitor->counter_state == PERF_COUNTER_ENABLE;
    pthread_mutex_unlock( &mgr->monitor->counter_mutex );

    return res;
}

static void *runner(void *data)
{
    struct timespec ts;
    int res;

    monitor_t *this = (monitor_t *)data;

    pthread_mutex_lock( &this->mutex );

    while( this->running )
    {
        clock_gettime(CLOCK_REALTIME, &ts);
        increment_ns(&ts, WAIT_TIME_NS);

        res = pthread_cond_timedwait( &this->cond, &this->mutex, &ts );
        if( res == ETIMEDOUT ) 
        {
            /* Check the lock file */
            res = checkfile( this->lockfp );
            if ( res != -1 ) {
                pthread_mutex_lock(&this->counter_mutex);
                if ( this->counter_state != res ) {
                    perf_counter_enable( this->perf, res );
                    if( res == PERF_COUNTER_DISABLE) 
                    {
                        this->has_been_disabled = 1;
                    }
                    this->counter_state = res;
                }
                pthread_mutex_unlock(&this->counter_mutex);
            }
            else {
                fprintf(stderr, "TODO");
                // Handle error
            }
        }

    }

    pthread_mutex_unlock( &this->mutex );

    pthread_exit(data);

    /* Note: you never get here because pthread_exit() never returns, but the
     * presence of the return statement keeps the compiler happy */
    return data;
}

perfmanagerhandle_t *manager_init(const char *configfilename)
{
    int res;
    int fp;
    manager_t *mgr;

    mgr = malloc(sizeof(manager_t) );
    if( 0 == mgr )
    {
        return 0;
    }

	fp = open(get_perf_alloc_lockfile(), O_CREAT | O_RDWR, S_IRWXU |  S_IRGRP | S_IROTH );

	if( fp < 0 ) {
        free(mgr);
		return 0;
	}

    perfhandle_t *perf = perf_event_create(configfilename);

    if( 0 == perf) {
        free(mgr);
	close(fp);
        return 0;
    }

    mgr->monitor = monitor_init(fp, perf);
    if( 0 == mgr->monitor)
    {
        free(mgr);
	close(fp);
        return 0;
    }

    res = pthread_create(&mgr->thread, NULL, runner, mgr->monitor);
    if(res != 0 ) {
        mgr->thread = 0;
        monitor_destroy(mgr->monitor);
        free(mgr);
        return 0;
    }

    return (perfmanagerhandle_t *)mgr;
}

void manager_destroy(perfmanagerhandle_t *m)
{
    manager_t *mgr = (manager_t *)m;

    /* Signal thread to exit */
    pthread_mutex_lock(&mgr->monitor->mutex);
    mgr->monitor->running = 0;
    pthread_cond_signal(&mgr->monitor->cond);
    pthread_mutex_unlock(&mgr->monitor->mutex);

    pthread_join(mgr->thread, NULL);

    monitor_destroy(mgr->monitor);
    free(mgr); 

    free_perf_alloc_lockfile();
}
