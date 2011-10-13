/*
 * Generic event queue support for PMDAs
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

#include "queues.h"
#include <ctype.h>

static event_queue_t *queues;
static int numqueues;
static event_queue_t *queue_lookup(int handle);

static event_client_t *clients;
static int numclients;
static event_client_t *client_lookup(int context);

typedef void (*clientVisitCallBack)(event_client_t *, event_queue_t *, void *);
static void client_iterate(clientVisitCallBack, event_queue_t *, void *);

int
pmdaEventNewQueue(const char *name, size_t maxmemory)
{
    event_queue_t *queue;
    size_t size;
    int i;

    for (i = 0; i < numqueues; i++)
	if (queues[i].inuse == 0 && strcmp(queues[i].name, name) == 0)
	    return -EEXIST;

    for (i = 0; i < numqueues; i++)
	if (queues[i].inuse == 0)
	    break;
    if (i == numqueues) {
	/* no free slots, extend the available set */
	size = (numqueues + 1) * sizeof(event_queue_t);
	queues = realloc(queues, size);
	if (!queues)
	    __pmNoMem("pmdaEventNewQueue", size, PM_FATAL_ERR);
	numqueues++;
    }

    /* "i" now indexes into a free slot */
    queue = &queues[i];
    memset(queue, 0, sizeof(*queue));
    TAILQ_INIT(&queue->tailq); 
    queue->eventarray = pmdaEventNewArray();
    queue->maxmemory = maxmemory;
    queue->inuse = 1;
    queue->name = name;
    return i;
}

int
pmdaEventQueueHandle(const char *name)
{
    int i;

    for (i = 0; i < numqueues; i++)
	if (queues[i].inuse && strcmp(queues[i].name, name) == 0)
	    return i;
    return -ESRCH;
}

static event_queue_t *
queue_lookup(int handle)
{
    if (handle >= numqueues || handle < 0)
	return NULL;
    if (queues[handle].inuse)
	return &queues[handle];
    return NULL;
}

int
pmdaEventQueueCounter(int handle, pmAtomValue *atom)
{
    event_queue_t *queue = queue_lookup(handle);

    if (!queue)
	return -EINVAL;
    atom->ul = queue->count;
    return 0;
}

int
pmdaEventQueueClients(int handle, pmAtomValue *atom)
{
    event_queue_t *queue = queue_lookup(handle);

    if (!queue)
	return -EINVAL;
    atom->ul = queue->numclients;
    return 0;
}

int
pmdaEventQueueMemory(int handle, pmAtomValue *atom)
{
    event_queue_t *queue = queue_lookup(handle);

    if (!queue)
	return -EINVAL;
    atom->ull = (__uint64_t)queue->qsize;
    return 0;
}

int
pmdaEventQueueBytes(int handle, pmAtomValue *atom)
{
    event_queue_t *queue = queue_lookup(handle);

    if (!queue)
	return -EINVAL;
    atom->ull = queue->bytes;
    return 0;
}

static void
queue_drop(event_client_t *client, event_queue_t *queue, void *data)
{
    event_t *event = (event_t *)data;

    if (pmDebug & DBG_TRACE_APPL1)
	__pmNotifyErr(LOG_INFO, "Visited ctx=%d event %p (last was %p)",
			client->context, event, client->last);

    if (client->last != NULL && client->last == event) {
	client->last = TAILQ_NEXT(event, events);
	client->missed++;

	if (pmDebug & DBG_TRACE_APPL1)
	    __pmNotifyErr(LOG_INFO, "Missed %s event %p for ctx=%d",
			queue->name, event, client->context);
    }
}

int
pmdaEventQueueAppend(int handle, void *buffer, size_t bytes, struct timeval *tv)
{
    event_queue_t *queue = queue_lookup(handle);
    event_t *event, *next;

    if (pmDebug & DBG_TRACE_APPL1)
	__pmNotifyErr(LOG_INFO, "Event enqueue, handle %d (%d bytes)",
			handle, (int)bytes);
    if (!queue)
	return -EINVAL;

    /*
     * We may need to make room in the event queue.  If so, start at the head
     * and madly drop events until sufficient space exists or all are freed.
     * Bump the missed counter for each client who missed an event we had to
     * throw away.
     */
    event = TAILQ_FIRST(&queue->tailq);
    while (event) {
	if (bytes <= queue->maxmemory - queue->qsize)
	    break;
	next = TAILQ_NEXT(event, events);

	if (pmDebug & DBG_TRACE_APPL1)
	    __pmNotifyErr(LOG_INFO, "Dropping %s: e=%p sz=%d max=%d qsz=%d",
				    queue->name, event, (int)event->size,
				    (int)queue->maxmemory, queue->qsize);

	/* Walk clients - if event last seen, drop it and bump missed count */
	client_iterate(queue_drop, queue, event);

	if (pmDebug & DBG_TRACE_APPL1)
	    __pmNotifyErr(LOG_INFO, "Removing %s event %p (%d bytes)",
				    queue->name, event, (int)event->size);

	TAILQ_REMOVE(&queue->tailq, event, events);
	queue->qsize -= event->size;
	free(event);
	event = next;
    }

    if ((event = malloc(sizeof(event_t) + bytes + 1)) == NULL) {
	__pmNotifyErr(LOG_ERR, "event dup allocation failure: %d bytes",
			(int)(bytes + 1));
	return -ENOMEM;
    }

    /* Track the actual event data */
    event->count = queue->numclients;
    memcpy(event->buffer, buffer, bytes);
    memcpy(&event->time, tv, sizeof(*tv));
    event->size = bytes;

    /* Update event queue tracking stats */
    queue->count++;
    queue->bytes += bytes;

    /* Finally, store the event in the queue */
    TAILQ_INSERT_TAIL(&queue->tailq, event, events);
    queue->qsize += bytes;

    if (pmDebug & DBG_TRACE_APPL1)
	__pmNotifyErr(LOG_INFO, "Inserted %s event %p (%d bytes) clients = %d.",
			queue->name, event, (int)event->size, event->count);
    return 0;
}

int
queue_fetch(event_queue_t *queue, pmAtomValue *atom, event_client_t *client,
	    pmdaEventDecodeCallBack queue_decoder, void *data)
{
    event_t *event, *next;
    int records, key, sts;

    if (pmDebug & DBG_TRACE_APPL2)
	__pmNotifyErr(LOG_INFO, "queue_fetch: ctx=%d\n", client->context);

    /*
     * Ensure the way we keep track of which clients are interested
     * in which queues is up to date.
     */
    if (client->active == 0) {
	client->active = 1;
	client->last = TAILQ_LAST(&queue->tailq, tailqueue);
	queue->numclients++;
    }

    event = client->last;
    if (pmDebug & DBG_TRACE_APPL2)
	__pmNotifyErr(LOG_INFO, "queue_fetch start, last event=%p\n", event);

    sts = records = 0;
    key = queue->eventarray;
    pmdaEventResetArray(key);

    while (event != NULL) {
	char message[64];
	int filter;

	filter = pmdaEventFilter(client->context, event->buffer, event->size);
	if (pmDebug & DBG_TRACE_APPL1)
	    __pmNotifyErr(LOG_INFO, "%s parameter: \"%s\"", 
				    filter ? "Filtering" : "Adding",
				    __pmdaEventPrint(event->buffer, event->size,
					 message, sizeof(message)));
	if (!filter) {
	    if (pmDebug & DBG_TRACE_APPL1)
		__pmNotifyErr(LOG_INFO, "Adding param: \"%s\"", 
				    __pmdaEventPrint(event->buffer, event->size,
					message, sizeof(message)));
	    sts = pmdaEventAddRecord(key, &event->time, PM_EVENT_FLAG_POINT);
	    if (sts < 0)
		break;
	    sts = queue_decoder(key, event->buffer, event->size, data);
	    if (sts < 0)
		break;
	    records++;
	}

	next = TAILQ_NEXT(event, events);

	/* Remove the current one (if its use count hits zero) */
	if (--event->count <= 0) {
	    if (pmDebug & DBG_TRACE_APPL1)
		__pmNotifyErr(LOG_INFO, "Removing %s event %p in fetch",
					queue->name, event);
	    TAILQ_REMOVE(&queue->tailq, event, events);
	    queue->qsize -= event->size;
	    free(event);
	}

	/* Go on to the next event. */
	event = next;
    }

    /*
     * Did this client miss any events?  The "extra" one is the last previously
     * observed event (pointed at by per-context last pointer) - so, event only
     * missed once we move past *more* than just that last observed event.
     */
    if (sts == 0) {
	sts = client->missed - 1;
	client->missed = 0;
	if (sts > 0) {
	    struct timeval timestamp;
	    gettimeofday(&timestamp, NULL);
	    sts = pmdaEventAddMissedRecord(key, &timestamp, sts);
	    records++;
	} else {
	    sts = 0;
	}
    }

    /* Update queue tail pointer for this client. */
    client->last = TAILQ_LAST(&queue->tailq, tailqueue);

    atom->vbp = records ? (pmValueBlock *)pmdaEventGetAddr(key) : NULL;
    return sts;
}

int
pmdaEventQueueRecords(int handle, pmAtomValue *atom, int context,
	    pmdaEventDecodeCallBack queue_decoder, void *data)
{
    event_queue_t *queue = queue_lookup(handle);
    event_client_t *client = client_lookup(context);
    int sts;

    if (!queue)
	return -EINVAL;

    if ((sts = queue_fetch(queue, atom, client, queue_decoder, data)) != 0)
	return sts;
    return (atom->vbp == NULL) ? PMDA_FETCH_NOVALUES : PMDA_FETCH_STATIC;
}

/*
 * We've lost a client (disconnected).
 * Cleanup any back references across all of the queues.
 */
static void
queue_cleanup(event_queue_t *queue, event_client_t *client)
{
    event_t *event, *next;

    if (client->active == 0)
	return;

    event = client->last;
    while (event) {
	next = TAILQ_NEXT(event, events);

	/* Remove the current event (if use count hits zero) */
	if (--event->count <= 0) {
	    if (pmDebug & DBG_TRACE_APPL1)
		__pmNotifyErr(LOG_INFO, "Removing %s event %p",
				queue->name, event);
	    TAILQ_REMOVE(&queue->tailq, event, events);
	    queue->qsize -= event->size;
	    free(event);
	}
	event = next;
    }

    queue->numclients--;
}


char *
__pmdaEventPrint(const char *buffer, int bufsize, char *msg, int msgsize)
{
    int i;

    if (msgsize < 4)
	return NULL;
    strncpy(msg, buffer, msgsize - 4);
    msg[msgsize - 4] = '\0';
    for (i = 0; i < (msgsize - 4) && i < bufsize; i++) {
	if (isspace(msg[i]))
	    msg[i] = ' ';
	else if (!isprint(msg[i]))
	    msg[i] = '.';
    }
    if (bufsize > msgsize - 4)
	strcat(msg, "...");
    return msg;
}

int
pmdaEventNewClient(int context)
{
    event_client_t *client;
    int size, i;

    for (i = 0; i < numclients; i++) {
	if (clients[i].inuse == 0)
	   break;
    }
    if (i == numclients) {
	/* no free slots, extend the available set */
	size = (numclients + 1) * sizeof(event_client_t);
	clients = realloc(clients, size);
	if (!clients)
	    __pmNoMem("pmdaEventNewClient", size, PM_FATAL_ERR);
	numclients++;

	if (pmDebug & DBG_TRACE_APPL2)
	    __pmNotifyErr(LOG_INFO, "%s: new client, slot=%d (total=%d)\n",
		      __FUNCTION__, i, numclients);
    }

    /* "i" now indexes into a free slot */
    client = &clients[i];
    memset(client, 0, sizeof(*client));
    client->context = context;
    client->inuse = 1;
    return i;
}

static event_client_t *
client_lookup(int context)
{
    int i;

    for (i = 0; i < numclients; i++)
	if (clients[i].context == context && clients[i].inuse)
	    return &clients[i];
    return NULL;
}

/*
 * Visit each active context and run a supplied callback routine
 */
static void
client_iterate(clientVisitCallBack visit, event_queue_t *queue, void *data)
{
    int i;

    for (i = 0; i < numclients; i++)
	if (clients[i].inuse)
	    visit(&clients[i], queue, data);
}

int
pmdaEventEndClient(int context)
{
    event_client_t *client = client_lookup(context);
    int i;

    if (pmDebug & DBG_TRACE_APPL2)
	fprintf(stderr, "pmdaEventEndClient: ctx=%d slot=%d\n",
		context, client ? (int)(client - clients) : 0);

    if (!client) {
	/*
	 * This is expected ... when a context is closed in pmcd
	 * (or for a local context or for dbpmda or ...) all the
	 * PMDAs with a registered pmdaEndContextCallBack will be
	 * called and some of the PMDAs may not have not serviced
	 * any previous requests for that context.
	 */
	return 0;
    }

    for (i = 0; i < numqueues; i++)
	queue_cleanup(&queues[i], client);

    if (client->release)
	client->release(context, client->filter);
    memset(client, 0, sizeof(*client));
    return 0;
}

int
pmdaEventClients(pmAtomValue *atom)
{
    __uint32_t i, c = 0;

    for (i = 0; i < numclients; i++)
	if (clients->inuse)
	    c++;
    atom->ul = c;
    return 0;
}

/*
 * Marks context as having been pmStore'd into (access allowed),
 * adds optional filtering data for the current client context.
 */
int
pmdaEventSetFilter(int context, void *filter,
		   pmdaEventApplyFilterCallBack apply,
		   pmdaEventReleaseFilterCallBack release)
{
    event_client_t *client = client_lookup(context);

    if (!client)
	return -EINVAL;

    /* first, free up any existing filter */
    if (client->filter)
	client->release(context, client->filter);

    client->apply = apply;
    client->filter = filter;
    client->release = release;
    client->access = 1;
    return 0;
}

int
pmdaEventSetAccess(int context, int allow)
{
    event_client_t *client = client_lookup(context);

    if (!client)
	return -EINVAL;
    client->access = allow;
    return 0;
}

int
pmdaEventFilter(int context, void *data, int size)
{
    event_client_t *client = client_lookup(context);

    if (!client)
	return -EINVAL;
    if (client->filter)
	return client->apply(context, client->filter, data, size);
    if (!client->access)
	return PM_ERR_PERMISSION;
    return 0;
}
