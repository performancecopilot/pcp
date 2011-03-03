/*
 * event support for the Logger PMDA
 *
 * Copyright (c) 2011 Red Hat Inc.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include "event.h"
#include "percontext.h"

#define BUF_SIZE 1024

struct event {
    TAILQ_ENTRY(event) events;
    int clients;
    char buffer[BUF_SIZE];
};

static TAILQ_HEAD(tailhead, event) head;
static int eventarray;

/* This has to match the values in the metric table. */
static pmID pmid_string = PMDA_PMID(0,1); /* event.param_string */

struct ctx_client_data {
    struct event **last;	       /* addr of last next element */
};

static int monitorfd = 0;

static void *
ctx_start_callback(int ctx)
{
    struct ctx_client_data *c = ctx_get_user_data();

    if (c == NULL) {
	c = malloc(sizeof(struct ctx_client_data));
	if (c == NULL) {
	    __pmNotifyErr(LOG_ERR, "allocation failure");
	    return NULL;
	}
	c->last = head.tqh_last;
	__pmNotifyErr(LOG_INFO, "Setting last.");
    }
    return c;
}

static void
ctx_end_callback(int ctx, void *user_data)
{
    struct ctx_client_data *c = user_data;
    
    event_cleanup();
    if (c != NULL)
	free(c);
    return;
}

void
event_init(pmdaInterface *dispatch, const char *monitor_path)
{
    /* initialize queue */
    TAILQ_INIT(&head);
    eventarray = pmdaEventNewArray();

    /*
     * fix the domain field in the event parameter PMIDs ...
     * note these PMIDs must match the corresponding metrics in
     * desctab[] and this cannot easily be done automatically
     */
    ((__pmID_int *)&pmid_string)->domain = dispatch->domain;

    ctx_register_callbacks(ctx_start_callback, ctx_end_callback);

    /* We can't really select on the logfile.  Why?  If the logfile is
     * a normal file, select will (continually) return EOF after we've
     * read all the data.  Then we tried a custom main that that read
     * data before handling any message we get on the control channel.
     * That didn't work either, since the client context wasn't set up
     * yet (since that is the 1st control message).  So, now we read
     * data inside the event fetch routine. */

    /* Try to open logfile to monitor */
    monitorfd = open(monitor_path, O_RDONLY|O_NONBLOCK);
    if (monitorfd < 0) {
	__pmNotifyErr(LOG_ERR, "open failure on %s", monitor_path);
	exit(1);
    }

    /* Skip to the end. */
    //(void)lseek(monitorfd, 0, SEEK_END);
}

static int
event_create(int fd)
{
    ssize_t c;

    /* Allocate a new event. */
    struct event *e = malloc(sizeof(struct event));
    if (e == NULL) {
	__pmNotifyErr(LOG_ERR, "allocation failure");
	return -1;
    }

    /* Read up to BUF_SIZE bytes at a time. */
    if ((c = read(fd, e->buffer, sizeof(e->buffer) - 1)) < 0) {
	__pmNotifyErr(LOG_ERR, "read failure: %s", strerror(errno));
	free(e);
	return -1;
    }
    else if (c == 0) {	     /* EOF */
	free(e);
	return 0;
    }

    /* Store event in queue. */
    e->clients = ctx_get_num();
    e->buffer[c] = '\0';
    TAILQ_INSERT_TAIL(&head, e, events);
    __pmNotifyErr(LOG_INFO, "Inserted item, clients = %d.", e->clients);
    return 0;
}

int
event_fetch(pmValueBlock **vbpp)
{
    struct event *e, *next;
    struct timeval stamp;
    pmAtomValue atom;
    int rc;
    int records = 0;
    struct ctx_client_data *c = ctx_get_user_data();
    
    /* Update the event queue with new data (if any). */
    if ((rc = event_create(monitorfd)) < 0)
	return rc;

    if (vbpp == NULL)
	return -1;
    *vbpp = NULL;

    pmdaEventResetArray(eventarray);
    gettimeofday(&stamp, NULL);
    if ((rc = pmdaEventAddRecord(eventarray, &stamp, PM_EVENT_FLAG_POINT)) < 0)
	return rc;

    e = *c->last;
    while (e != NULL) {
	/* Add the string parameter.  Note that pmdaEventAddParam()
	 * copies the string, so we can free it soon after. */
	atom.cp = e->buffer;
	__pmNotifyErr(LOG_INFO, "Adding param: %s", e->buffer);
	if ((rc = pmdaEventAddParam(eventarray, pmid_string, PM_TYPE_STRING,
				    &atom)) < 0)
	    return rc;
	records++;

	/* Get the next event. */
	next = e->events.tqe_next;

	/* Remove the current one (if its use count is at 0). */
	if (--e->clients <= 0) {
	    TAILQ_REMOVE(&head, e, events);
	    free(e);
	}

	/* Go on to the next event. */
	e = next;
    }

    /* Update queue pointer. */
    c->last = head.tqh_last;

    if (records > 0)
	*vbpp = (pmValueBlock *)pmdaEventGetAddr(eventarray);
    else
	*vbpp = NULL;
    return 0;
}

void
event_cleanup(void)
{
    struct event *e, *next;
    struct ctx_client_data *c = ctx_get_user_data();

    /* We've lost a client.  Cleanup. */
    e = *c->last;
    while (e != NULL) {
	/* Get the next event. */
	next = e->events.tqe_next;

	/* Remove the current one (if its use count is at 0). */
	if (--e->clients <= 0) {
	    TAILQ_REMOVE(&head, e, events);
	    free(e);
	}

	/* Go on to the next event. */
	e = next;
    }
}
