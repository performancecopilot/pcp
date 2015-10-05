/*
 * Event queue support for PMDAs
 *
 * Copyright (c) 2011 Red Hat Inc.
 * Copyright (c) 2011 Nathan Scott.  All rights reserved.
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
 *
 * Portions Copyright (c) 1991, 1993
 *
 * The Regents of the University of California.  All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _QUEUES_H
#define _QUEUES_H

/*
 * Extracts of TAILQ implementation from <sys/queue.h> included directly
 * for platforms without support (Win32, older Linux variants).
 *
 * A tail queue is headed by a pair of pointers, one to the head of the
 * list and the other to the tail of the list. The elements are doubly
 * linked so that an arbitrary element can be removed without a need to
 * traverse the list. New elements can be added to the list before or
 * after an existing element, at the head of the list, or at the end of
 * the list. A tail queue may be traversed in either direction.
 *
 * For details on the use of these macros, see the queue(3) manual page.
 */ 

#define	_TAILQ_HEAD(name, type, qual)					\
struct name {								\
	qual type *tqh_first;		/* first element */		\
	qual type *qual *tqh_last;	/* addr of last next element */	\
}
#define TAILQ_HEAD(name, type)	_TAILQ_HEAD(name, struct type,)

#define	_TAILQ_ENTRY(type, qual)					\
struct {								\
	qual type *tqe_next;		/* next element */		\
	qual type *qual *tqe_prev;	/* address of previous next element */\
}
#define TAILQ_ENTRY(type)	_TAILQ_ENTRY(struct type,)

#define	TAILQ_INIT(head) do {						\
	(head)->tqh_first = NULL;					\
	(head)->tqh_last = &(head)->tqh_first;				\
} while (0)

#define	TAILQ_INSERT_HEAD(head, elm, field) do {			\
	if (((elm)->field.tqe_next = (head)->tqh_first) != NULL)	\
		(head)->tqh_first->field.tqe_prev =			\
		    &(elm)->field.tqe_next;				\
	else								\
		(head)->tqh_last = &(elm)->field.tqe_next;		\
	(head)->tqh_first = (elm);					\
	(elm)->field.tqe_prev = &(head)->tqh_first;			\
} while (0)

#define	TAILQ_INSERT_TAIL(head, elm, field) do {			\
	(elm)->field.tqe_next = NULL;					\
	(elm)->field.tqe_prev = (head)->tqh_last;			\
	*(head)->tqh_last = (elm);					\
	(head)->tqh_last = &(elm)->field.tqe_next;			\
} while (0)

#define	TAILQ_REMOVE(head, elm, field) do {				\
	if (((elm)->field.tqe_next) != NULL)				\
		(elm)->field.tqe_next->field.tqe_prev = 		\
		    (elm)->field.tqe_prev;				\
	else								\
		(head)->tqh_last = (elm)->field.tqe_prev;		\
	*(elm)->field.tqe_prev = (elm)->field.tqe_next;			\
} while (0)

#define	TAILQ_EMPTY(head)		((head)->tqh_first == NULL)
#define	TAILQ_FIRST(head)		((head)->tqh_first)
#define	TAILQ_NEXT(elm, field)		((elm)->field.tqe_next)

#define	TAILQ_LAST(head, headname) \
	(*(((struct headname *)((head)->tqh_last))->tqh_last))
#define	TAILQ_PREV(elm, headname, field) \
	(*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))


/*
 * Data structures used in the PMDA event queue implementation
 * Every event is timestamped and linked into one (tail) queue.
 * Events know nothing about the clients accessing them.
 */

typedef struct event {
    TAILQ_ENTRY(event)	events;		/* link into queue of events */
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
    __uint32_t		count;		/* exported: event counter */
    __uint64_t		bytes;		/* exported: data throughput */
    __uint64_t		qsize;		/* data in the queue (<= maxmem) */
    struct tailqueue	tailq;		/* queue of events for clients */
} event_queue_t;

/*
 * Data structures used in the PMDA event client implementation
 * Each client is one PCP tool invocation (e.g. pmevent) and has
 * a link back to those queues which it has fetched/stored into
 * at some point in the past.  The "last" event pointer, gives a
 * pointer to the last observed event for that client, which is
 * used as the starting point for a subsequent fetch request (or
 * when dropping events, should the client not be keeping up).
 */

typedef struct event_clientq {
    int			active;		/* client interest in this queue */
    int			missed;		/* count of events missed on queue */
    int			access;		/* is access restricted/permitted */
    event_t		*last;		/* last event seen on this queue */
    void		*filter;	/* filter data for the event queue */
    pmdaEventApplyFilterCallBack apply;		/* actual filter callback */
    pmdaEventReleaseFilterCallBack release;	/* remove filter callback */
} event_clientq_t;

typedef struct event_client {
    int			context;	/* client context identifier */
    int			inuse;		/* is this table slot in use */
    int			nclientq;	/* allocated size of clientq */
    event_clientq_t	*clientq;	/* per-queue client state */
} event_client_t;

#endif /* _QUEUES_H */
