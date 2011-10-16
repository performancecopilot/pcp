/*
 * Event queue support for PMDAs
 *
 * Copyright (c) 2011 Red Hat Inc.
 * Copyright (c) 2011 Nathan Scott.  All rights reserved.
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

#ifndef _QUEUES_H
#define _QUEUES_H

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <sys/queue.h>

/*
 * Queue access methods that are missing on glibc <= 2.3, e.g. RHEL4
 */ 
#ifndef TAILQ_NEXT	
#define	TAILQ_NEXT(elm, field)	((elm)->field.tqe_next)
#endif

#ifndef TAILQ_FIRST
#define TAILQ_FIRST(head)	((head)->tqh_first)
#endif

#ifndef TAILQ_EMPTY
#define TAILQ_EMPTY(head)               ((head)->tqh_first == NULL)
#endif

#ifndef TAILQ_LAST
#define TAILQ_LAST(head, headname) \
        (*(((struct headname *)((head)->tqh_last))->tqh_last))
#endif

#ifndef TAILQ_PREV
#define TAILQ_PREV(elm, headname, field) \
        (*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))
#endif

typedef struct event {
    TAILQ_ENTRY(event)	events;
    struct timeval	time;		/* timestamp for this event */
    int			count;		/* events reference count */
    size_t		size;		/* buffer size in bytes */
    char		buffer[];
} event_t;

TAILQ_HEAD(tailqueue, event);

typedef struct event_queue {
    const char		*name;		/* callers identifier for this queue */
    size_t		maxmemory;	/* max data bytes that can be queued */
    int			inuse;		/* is this queue in use or free */
    int			eventarray;	/* event records for this queue */
    __uint32_t		numclients;	/* export: number of active clients */
    __uint32_t		qsize;		/* total data in the queue (< maxmem) */
    __uint32_t		count;		/* exported: event counter */
    __uint64_t		bytes;		/* exported: data throughput */
    struct tailqueue	tailq;		/* queue of events for clients */
} event_queue_t;

typedef struct event_client {
    int		context;
    int		inuse;
    int		access;
    int		active;
    int		missed;
    int		unused;
    event_t	*last;
    void	*filter;
    pmdaEventApplyFilterCallBack apply;
    pmdaEventReleaseFilterCallBack release;
} event_client_t;

#endif /* _QUEUES_H */
