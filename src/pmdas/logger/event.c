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
static pmID pmid_string = PMDA_PMID(0,134); /* event.param_string */

void
event_init(int domain)
{
    /* initialize queue */
    TAILQ_INIT(&head);
    eventarray = pmdaEventNewArray();

    /*
     * fix the domain field in the event parameter PMIDs ...
     * note these PMIDs must match the corresponding metrics in
     * desctab[] and this cannot easily be done automatically
     */
    ((__pmID_int *)&pmid_string)->domain = domain;
}

int
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
	__pmNotifyErr(LOG_ERR, "allocation failure: %s", strerror(errno));
	return -1;
    }
    
    /* Store event in queue. */
    e->clients = ctx_get_num();
    e->buffer[c] = '\0';
    TAILQ_INSERT_TAIL(&head, e, events);
    return 0;
}

int
event_fetch(pmValueBlock **vbpp)
{
    struct event *e, *next;
    struct timeval stamp;
    pmAtomValue atom;
    int rc;
    
    if (vbpp == NULL)
	return -1;
    *vbpp = NULL;

    pmdaEventResetArray(eventarray);
    gettimeofday(&stamp, NULL);
    if ((rc = pmdaEventAddRecord(eventarray, &stamp, PM_EVENT_FLAG_POINT)) < 0)
	return rc;

    e = head.tqh_first;
    while (e != NULL) {
	/* Add the string parameter.  Note that pmdaEventAddParam()
	 * copies the string, so we can free it soon after. */
	atom.cp = e->buffer;
	if ((rc = pmdaEventAddParam(eventarray, pmid_string, PM_TYPE_STRING,
				    &atom)) < 0)
	    return rc;

	/* Get the next event. */
	next = e->events.tqe_next;

	/* Remove the current one (if its use count is at 0). */
	if (--e->clients == 0) {
	    TAILQ_REMOVE(&head, e, events);
	    free(e);
	}

	/* Go on to the next event. */
	e = next;
    }

    *vbpp = (pmValueBlock *)pmdaEventGetAddr(eventarray);
    return 0;
}
