/*
 * Generic event queue support for PMDAs
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
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "queues.h"
#include <ctype.h>

static event_queue_t *queues;
static int numqueues;

static event_client_t *clients;
static int numclients;
static event_client_t *client_lookup(int context);

typedef void (*clientVisitCallBack)(event_clientq_t *, event_queue_t *, void *);
static void client_iterate(clientVisitCallBack, int, event_queue_t *, void *);

static event_queue_t *
queue_lookup(int handle)
{
    if (handle >= numqueues || handle < 0)
	return NULL;
    if (queues[handle].inuse)
	return &queues[handle];
    return NULL;
}

/*
 * Drop an event after it has been queued (i.e. client was too slow)
 */
static void
queue_drop(event_clientq_t *clientq, event_queue_t *queue, void *data)
{
    event_t *event = (event_t *)data;

    if (clientq->last != NULL && clientq->last == event) {
	clientq->last = TAILQ_NEXT(event, events);
	clientq->missed++;

	if (pmDebug & DBG_TRACE_LIBPMDA)
	    __pmNotifyErr(LOG_INFO, "Client missed queue %s event %p",
			queue->name, event);
    }
}

static void
queue_drop_bytes(int handle, event_queue_t *queue, size_t bytes)
{
    event_t *event, *next;

    event = TAILQ_FIRST(&queue->tailq);
    while (event) {
	if (bytes <= queue->maxmemory - queue->qsize)
	    break;
	next = TAILQ_NEXT(event, events);

	if (pmDebug & DBG_TRACE_LIBPMDA)
	    __pmNotifyErr(LOG_INFO, "Dropping %s: e=%p sz=%d max=%d qsz=%d",
				    queue->name, event, (int)event->size,
				    (int)queue->maxmemory, (int)queue->qsize);

	/* Walk clients - if event last seen, drop it and bump missed count */
	client_iterate(queue_drop, handle, queue, event);

	if (pmDebug & DBG_TRACE_LIBPMDA)
	    __pmNotifyErr(LOG_INFO, "Removing %s event %p (%d bytes)",
				    queue->name, event, (int)event->size);

	TAILQ_REMOVE(&queue->tailq, event, events);
	queue->qsize -= event->size;
	free(event);
	event = next;
    }
}

int
pmdaEventNewActiveQueue(const char *name, size_t maxmemory, unsigned int numclients)
{
    event_queue_t *queue;
    size_t size;
    int i;

    if (name == NULL || maxmemory <= 0)
	return -EINVAL;

    for (i = 0; i < numqueues; i++)
	if (queues[i].inuse && strcmp(queues[i].name, name) == 0)
	    return -EEXIST;

    for (i = 0; i < numqueues; i++)
	if (queues[i].inuse == 0)
	    break;
    if (i == numqueues) {
	/*
	 * No free slots - extend the available set.
	 * realloc() potential moves "queues" address, fix up 
	 * must tear down existing queues which may have back
	 * references and then re-initialise them afterward.
	 */
	for (i = 0; i < numqueues; i++)
	    queue_drop_bytes(i, &queues[i], INT_MAX);
	size = (numqueues + 1) * sizeof(event_queue_t);
	queues = realloc(queues, size);
	if (!queues)
	    __pmNoMem("pmdaEventNewQueue", size, PM_FATAL_ERR);
	/* realloc moves tailq tqh_last pointer - reset 'em */
	for (i = 0; i < numqueues; i++)
	    TAILQ_INIT(&queues[i].tailq);
	numqueues++;
    }

    /* "i" now indexes into a free slot */
    queue = &queues[i];
    memset(queue, 0, sizeof(*queue));
    TAILQ_INIT(&queue->tailq); 
    queue->eventarray = pmdaEventNewArray();
    queue->numclients = numclients;
    queue->maxmemory = maxmemory;
    queue->inuse = 1;
    queue->name = name;
    return i;
}

int
pmdaEventNewQueue(const char *name, size_t maxmemory)
{
    return pmdaEventNewActiveQueue(name, maxmemory, 0);
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

int
pmdaEventQueueCounter(int handle, pmAtomValue *atom)
{
    event_queue_t *queue = queue_lookup(handle);

    if (!queue)
	return -EINVAL;
    atom->ul = queue->count;
    return PMDA_FETCH_STATIC;
}

int
pmdaEventQueueClients(int handle, pmAtomValue *atom)
{
    event_queue_t *queue = queue_lookup(handle);

    if (!queue)
	return -EINVAL;
    atom->ul = queue->numclients;
    return PMDA_FETCH_STATIC;
}

int
pmdaEventQueueMemory(int handle, pmAtomValue *atom)
{
    event_queue_t *queue = queue_lookup(handle);

    if (!queue)
	return -EINVAL;
    atom->ull = queue->qsize;
    return PMDA_FETCH_STATIC;
}

int
pmdaEventQueueBytes(int handle, pmAtomValue *atom)
{
    event_queue_t *queue = queue_lookup(handle);

    if (!queue)
	return -EINVAL;
    atom->ull = queue->bytes;
    return PMDA_FETCH_STATIC;
}

int
pmdaEventQueueAppend(int handle, void *data, size_t bytes, struct timeval *tv)
{
    event_queue_t *queue = queue_lookup(handle);
    event_t *event;

    if (!queue)
	return -EINVAL;
    if (pmDebug & DBG_TRACE_LIBPMDA)
	__pmNotifyErr(LOG_INFO, "Appending event: queue#%d \"%s\" (%ld bytes)",
			handle, queue->name, (long)bytes);
    if (bytes > queue->maxmemory) {
	__pmNotifyErr(LOG_WARNING, "Event too large for queue %s (%ld > %ld)",
			queue->name, (long)bytes, (long)queue->maxmemory);
	goto done;
    }

    /*
     * We may need to make room in the event queue.  If so, start at the head
     * and madly drop events until sufficient space exists or all are freed.
     * Bump the missed counter for each client who missed an event we had to
     * throw away.
     */
    queue_drop_bytes(handle, queue, bytes);
    if (queue->numclients == 0)
	goto done;

    if ((event = malloc(sizeof(event_t) + bytes + 1)) == NULL) {
	__pmNotifyErr(LOG_ERR, "event allocation failure: %ld bytes",
			(long)(bytes + 1));
	return -ENOMEM;
    }

    /* Track the actual event data */
    event->count = queue->numclients;
    memcpy(event->buffer, data, bytes);
    memcpy(&event->time, tv, sizeof(*tv));
    event->size = bytes;

    /* Finally, store the event in the queue */
    TAILQ_INSERT_TAIL(&queue->tailq, event, events);
    queue->qsize += bytes;

    if (pmDebug & DBG_TRACE_LIBPMDA)
	__pmNotifyErr(LOG_INFO,
			"Inserted %s event %p (%ld bytes) clients = %d.",
			queue->name, event, (long)event->size, event->count);

done:
    /* Update event queue tracking stats (even for no-clients case) */
    queue->bytes += bytes;
    queue->count++;
    return 0;
}

static int
queue_filter(event_clientq_t *clientq, void *data, size_t size)
{
    /* Note: having a filter implies access (optionally) checked there */
    if (clientq->filter) {
	int sts = clientq->apply(clientq->filter, data, size);
	if (pmDebug & DBG_TRACE_LIBPMDA)
	    __pmNotifyErr(LOG_INFO, "Clientq filter applied (%d)\n", sts);
	return sts;
    }
    else if (!clientq->access) {
	if (pmDebug & DBG_TRACE_LIBPMDA)
	    __pmNotifyErr(LOG_INFO, "Clientq access denied\n");
	return -PM_ERR_PERMISSION;
    }
    return 0;
}

static int
queue_fetch(event_queue_t *queue, event_clientq_t *clientq, pmAtomValue *atom,
	    pmdaEventDecodeCallBack queue_decoder, void *data)
{
    event_t *event, *next;
    int records, key, sts;

    /*
     * Ensure the way we keep track of which clients are interested
     * in which queues is up to date.
     */
    if (clientq->active == 0) {
	clientq->active = 1;
	queue->numclients++;
    }
    if (clientq->last == NULL)
	clientq->last = TAILQ_FIRST(&queue->tailq);
    event = clientq->last;

    if (pmDebug & DBG_TRACE_LIBPMDA)
	__pmNotifyErr(LOG_INFO, "queue_fetch start, last event=%p\n", event);

    sts = records = 0;
    key = queue->eventarray;
    pmdaEventResetArray(key);

    while (event != NULL) {
	char	message[64];

	if (queue_filter(clientq, event->buffer, event->size)) {
	    if (pmDebug & DBG_TRACE_LIBPMDA)
		__pmNotifyErr(LOG_INFO, "Culling event (sz=%ld): \"%s\"", 
				(long)event->size,
				__pmdaEventPrint(event->buffer, event->size,
					message, sizeof(message)));
	} else {
	    if (pmDebug & DBG_TRACE_LIBPMDA)
		__pmNotifyErr(LOG_INFO, "Adding event (sz=%ld): \"%s\"", 
				(long)event->size,
				__pmdaEventPrint(event->buffer, event->size,
					message, sizeof(message)));
	    if ((sts = queue_decoder(key,
			event->buffer, event->size, &event->time, data)) < 0)
		break;
	    records += sts;
	    sts = 0;
	}

	next = TAILQ_NEXT(event, events);

	/* Remove the current one (if its use count hits zero) */
	if (--event->count <= 0) {
	    if (pmDebug & DBG_TRACE_LIBPMDA)
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
	sts = clientq->missed - 1;
	clientq->missed = 0;
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
    clientq->last = NULL;

    atom->vbp = records ? (pmValueBlock *)pmdaEventGetAddr(key) : NULL;
    return sts;
}

static event_clientq_t *
client_queue_lookup(int context, int handle, int accessq)
{
    event_client_t *client = client_lookup(context);
    event_queue_t *queue = queue_lookup(handle);
    size_t size;

    /*
     * If context doesn't exist, bail out.  But, if per-client
     * queue information doesn't exist for that context yet, it
     * is create iff an indication of interest was shown by the
     * caller (i.e. "accessq" was set).
     */
    if (!client || !queue)
	return NULL;
    if (handle < client->nclientq)
	return &client->clientq[handle];
    if (!accessq)
	return NULL;

    /* allocate (possibly multiple) queue slots for this client */
    size = (handle + 1) * sizeof(struct event_clientq);
    client->clientq = realloc(client->clientq, size);
    if (!client->clientq)
	__pmNoMem("client_queue_lookup", size, PM_FATAL_ERR);

    /* ensure any new clientq's up to this one are initialised */
    size -= client->nclientq * sizeof(struct event_clientq);
    memset(client->clientq + client->nclientq, 0, size);
    client->nclientq = handle + 1;
    return &client->clientq[handle];
}

int
pmdaEventQueueRecords(int handle, pmAtomValue *atom, int context,
	    pmdaEventDecodeCallBack queue_decoder, void *data)
{
    event_clientq_t *clientq = client_queue_lookup(context, handle, 1);
    event_queue_t *queue = queue_lookup(handle);
    int sts;

    if (!queue || !clientq)
	return -EINVAL;

    sts = queue_fetch(queue, clientq, atom, queue_decoder, data);
    if (sts != 0)
	return sts;
    return (atom->vbp == NULL) ? PMDA_FETCH_NOVALUES : PMDA_FETCH_STATIC;
}

/*
 * We've lost a client (disconnected).
 * Cleanup any filter and any back references across the queues.
 */
static void
queue_cleanup(int handle, event_clientq_t *clientq)
{
    event_queue_t *queue = queue_lookup(handle);
    event_t *event, *next;

    if (clientq->release)
	clientq->release(clientq->filter);

    if (!queue || !clientq->active)
	return;

    event = clientq->last;
    while (event) {
	next = TAILQ_NEXT(event, events);

	/* Remove the current event (if use count hits zero) */
	if (--event->count <= 0) {
	    if (pmDebug & DBG_TRACE_LIBPMDA)
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
    int minsize = msgsize < bufsize ? msgsize : bufsize;
    int i;

    if (msgsize < 4)
	return NULL;
    memcpy(msg, buffer, minsize);
    memset(msg + minsize, '.', msgsize - minsize);
    msg[minsize - 1] = '\0';
    for (i = 0; i < minsize - 1; i++) {
	if (isspace((int)msg[i]))
	    msg[i] = ' ';
	else if (!isprint((int)msg[i]))
	    msg[i] = '.';
    }
    return msg;
}

int
pmdaEventNewClient(int context)
{
    event_client_t *client;
    int size, i;

    for (i = 0; i < numclients; i++) {
	if (clients[i].context == context && clients[i].inuse)
	   return i;
    }
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

	if (pmDebug & DBG_TRACE_LIBPMDA)
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
client_iterate(clientVisitCallBack visit,
		int handle, event_queue_t *queue, void *data)
{
    event_clientq_t *clientq;
    int i;

    for (i = 0; i < numclients; i++) {
	if (!clients[i].inuse)
	    continue;
	clientq = client_queue_lookup(clients[i].context, handle, 0);
	if (clientq && clientq->active)
	    visit(clientq, queue, data);
    }
}

int
pmdaEventEndClient(int context)
{
    event_client_t *client = client_lookup(context);
    int i;

    if (pmDebug & DBG_TRACE_LIBPMDA)
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

    for (i = 0; i < client->nclientq; i++)
	queue_cleanup(i, &client->clientq[i]);
    if (client->clientq)
	free(client->clientq);

    memset(client, 0, sizeof(*client));		/* sets !inuse */
    return 0;
}

int
pmdaEventClients(pmAtomValue *atom)
{
    __uint32_t i, c = 0;

    for (i = 0; i < numclients; i++)
	if (clients[i].inuse)
	    c++;
    atom->ul = c;
    return PMDA_FETCH_STATIC;
}

/*
 * Marks context as having been pmStore'd into (access allowed),
 * adds optional filtering data for the current client context.
 */
int
pmdaEventSetFilter(int context, int handle, void *filter,
		   pmdaEventApplyFilterCallBack apply,
		   pmdaEventReleaseFilterCallBack release)
{
    event_clientq_t *clientq = client_queue_lookup(context, handle, 1);

    if (!clientq)
	return -EINVAL;

    /* first, free up any existing filter */
    if (clientq->filter)
	clientq->release(clientq->filter);

    clientq->apply = apply;
    clientq->filter = filter;
    clientq->release = release;
    clientq->access = 1;
    return 0;
}

int
pmdaEventSetAccess(int context, int handle, int allow)
{
    event_clientq_t *clientq = client_queue_lookup(context, handle, 1);

    if (!clientq)
	return -EINVAL;

    clientq->access = allow;
    return 0;
}
