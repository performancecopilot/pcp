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

#include "event.h"
#include "percontext.h"
#include "util.h"

static void event_cleanup(void);
static int eventarray;

int numlogfiles;
struct EventFileData *logfiles;

struct ctx_client_data {
    unsigned int	active_logfile;
    struct event      **last;
};

static void *
ctx_start_callback(int ctx)
{
    struct ctx_client_data *c = ctx_get_user_data();

    if (c == NULL) {
	c = calloc(numlogfiles, sizeof(struct ctx_client_data));
	if (c == NULL) {
	    __pmNotifyErr(LOG_ERR, "allocation failure");
	    return NULL;
	}
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
event_init(void)
{
    char cmd[MAXPATHLEN];
    int	i, fd;

    eventarray = pmdaEventNewArray();

    ctx_register_callbacks(ctx_start_callback, ctx_end_callback);

    for (i = 0; i < numlogfiles; i++) {
	size_t pathlen = strlen(logfiles[i].pathname);

	/*
	 * We support 2 kinds of PATHNAMEs:
	 * (1) Regular paths.  These paths are opened normally.
	 * (2) Pipes.  If the path ends in '|', the filename is
	 *     interpreted as a command which pipes input to us.
	 */
	if (logfiles[i].pathname[pathlen - 1] != '|') {
	    fd = open(logfiles[i].pathname, O_RDONLY|O_NONBLOCK);
	    if (fd < 0) {
		if (logfiles[i].fd >= 0)	/* log once only */
		    __pmNotifyErr(LOG_ERR, "open: %s - %s",
				logfiles[i].pathname, strerror(errno));
	    } else {
		lseek(fd, 0, SEEK_END);
	    }
	}
	else {
	    strncpy(cmd, logfiles[i].pathname, sizeof(cmd));
	    cmd[pathlen - 1] = '\0';	/* get rid of the '|' */
	    rstrip(cmd);	/* Remove all trailing whitespace. */
	    fd = start_cmd(cmd, &logfiles[i].pid);
	    if (fd < 0) {
		if (logfiles[i].fd >= 0)	/* log once only */
		    __pmNotifyErr(LOG_ERR, "pipe: %s - %s",
					logfiles[i].pathname, strerror(errno));
	    }
	}

	logfiles[i].fd = fd;		/* keep file descriptor (or error) */
	TAILQ_INIT(&logfiles[i].head);	/* initialize our queue */
    }
}

static int
event_create(int logfile)
{
    ssize_t c;

    /* Allocate a new event. */
    struct event *e = malloc(sizeof(struct event));
    if (e == NULL) {
	__pmNotifyErr(LOG_ERR, "allocation failure");
	return -1;
    }

    /* Read up to BUF_SIZE bytes at a time. */
    c = read(logfiles[logfile].fd, e->buffer, sizeof(e->buffer) - 1);

    /* If we've got EOF (0 bytes read) or EBADF (fd isn't valid - most
     * likely a closed pipe), just ignore the error. */
    if (c == 0 || (c < 0 && (errno == EBADF || errno == EISDIR))) {
	free(e);
	return 0;
    }
    if (c < 0) {
	__pmNotifyErr(LOG_ERR, "read failure on %s: %s",
		      logfiles[logfile].pathname, strerror(errno));
	free(e);
	return -1;
    }

    /* Update logfile event tracking stats. */
    logfiles[logfile].count++;
    logfiles[logfile].bytes += c;

    /* Store event in queue. */
    e->clients = logfiles[logfile].numclients;
    e->buffer[c] = '\0';
    TAILQ_INSERT_TAIL(&logfiles[logfile].head, e, events);

    if (pmDebug & DBG_TRACE_APPL1)
	__pmNotifyErr(LOG_INFO, "Inserted item, clients = %d.", e->clients);

    return 0;
}

int
event_get_clients_per_logfile(unsigned int logfile)
{
    return logfiles[logfile].numclients;
}

int
event_fetch(pmValueBlock **vbpp, unsigned int logfile)
{
    struct event *e, *next;
    struct timeval stamp;
    pmAtomValue atom;
    int rc;
    int records = 0;
    struct ctx_client_data *c = ctx_get_user_data();

    *vbpp = NULL;

    /*
     * Make sure the way we keep track of which clients are interested in
     * which logfiles is up to date.
     */
    if (c[logfile].active_logfile == 0) {
	c[logfile].active_logfile = 1;
	c[logfile].last = logfiles[logfile].head.tqh_last;
	logfiles[logfile].numclients++;
    }

    if (logfiles[logfile].fd < 0)
	return PM_ERR_AGAIN;

    /* Update the event queue with new data (if any). */
    if ((rc = event_create(logfile)) < 0)
	return rc;

    pmdaEventResetArray(eventarray);
    gettimeofday(&stamp, NULL);
    if ((rc = pmdaEventAddRecord(eventarray, &stamp, PM_EVENT_FLAG_POINT)) < 0)
	return rc;

    e = *c[logfile].last;
    while (e != NULL) {
	/* Add the string parameter.  Note that pmdaEventAddParam()
	 * copies the string, so we can free it soon after. */
	atom.cp = e->buffer;

	if (pmDebug & DBG_TRACE_APPL1)
	    __pmNotifyErr(LOG_INFO, "Adding param: %s", e->buffer);

	rc = pmdaEventAddParam(eventarray,
			       logfiles[logfile].pmid, PM_TYPE_STRING, &atom);
	if (rc < 0)
	    return rc;
	records++;

	/* Get the next event. */
	next = e->events.tqe_next;

	/* Remove the current one (if its use count is at 0). */
	if (--e->clients <= 0) {
	    TAILQ_REMOVE(&logfiles[logfile].head, e, events);
	    free(e);
	}

	/* Go on to the next event. */
	e = next;
    }

    /* Update queue pointer. */
    c[logfile].last = logfiles[logfile].head.tqh_last;

    if (records > 0)
	*vbpp = (pmValueBlock *)pmdaEventGetAddr(eventarray);
    return 0;
}

static void
event_cleanup(void)
{
    struct event *e, *next;
    struct ctx_client_data *c = ctx_get_user_data();
    int logfile;

    /* We've lost a client.  Cleanup. */
    for (logfile = 0; logfile < numlogfiles; logfile++) {
	if (c[logfile].active_logfile == 0)
	    continue;

	logfiles[logfile].numclients--;
	e = *c[logfile].last;
	while (e != NULL) {
	    /* Get the next event. */
	    next = e->events.tqe_next;

	    /* Remove the current one (if its use count is at 0). */
	    if (--e->clients <= 0) {
		TAILQ_REMOVE(&logfiles[logfile].head, e, events);
		free(e);
	    }

	    /* Go on to the next event. */
	    e = next;
	}
    }
}

void
event_shutdown(void)
{
    int i;

    __pmNotifyErr(LOG_INFO, "%s: Shutting down...", __FUNCTION__);

    for (i = 0; i < numlogfiles; i++) {
	if (logfiles[i].pid != 0) {
	    stop_cmd(logfiles[i].pid);
	    logfiles[i].pid = 0;
	}
	if (logfiles[i].fd > 0) {
	    close(logfiles[i].fd);
	    logfiles[i].fd = 0;
	}
    }
}
