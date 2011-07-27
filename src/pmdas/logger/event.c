/*
 * event support for the Logger PMDA
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include "event.h"
#include "percontext.h"
#include "util.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

static void event_cleanup(void);
static int eventarray;

int numlogfiles;
struct EventFileData *logfiles;

struct ctx_client_data {
    unsigned int	active_logfile;
    unsigned int	missed_count;
    struct event	*last;
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
		fstat(fd, &logfiles[i].pathstat);
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
	    } else {
		if (fd > maxfd)
		    maxfd = fd;
		FD_SET(fd, &fds);
	    }
	}

	logfiles[i].fd = fd;		/* keep file descriptor (or error) */
	TAILQ_INIT(&logfiles[i].queue);	/* initialize our queue */
    }
}

static void
event_missed(int ctx, int logfile, void *user_data, void *call_data)
{
    struct ctx_client_data *c = (struct ctx_client_data *)user_data;
    struct event *e = (struct event *)call_data;

    if (pmDebug & DBG_TRACE_APPL1)
	__pmNotifyErr(LOG_INFO, "Visited ctx=%d event %p (ctx last=%p)",
			ctx, e, c[logfile].last);

    if (c[logfile].last == NULL || c[logfile].last != e)
	return;
    c[logfile].last = TAILQ_NEXT(e, events);
    c[logfile].missed_count++;

    if (pmDebug & DBG_TRACE_APPL1)
	__pmNotifyErr(LOG_INFO, "Missed %s event %p for context %d",
			logfiles[logfile].pathname, e, ctx);
}

static char *
event_print(char *buffer, int size)
{
    static char msg[64];
    int i;

    strncpy(msg, buffer, sizeof(msg)-4);
    msg[sizeof(msg)-4] = '\0';
    for (i = 0; i < sizeof(msg-4) && i < size; i++) {
	if (isspace(msg[i]))
	    msg[i] = ' ';
	else if (!isprint(msg[i]))
	    msg[i] = '.';
    }
    if (size > sizeof(msg)-4)
	strcat(msg, "...");
    return msg;
}

void
event_enqueue(unsigned int logfile, char *buffer, int length)
{
    struct event *e, *next;

    if (pmDebug & DBG_TRACE_APPL1)
	__pmNotifyErr(LOG_INFO, "Event enqueue, logfile %d (%d bytes)",
			logfile, length);

    /*
     * We may need to make room in the event queue.  If so, start at the head
     * and madly drop events until sufficient space exists or all are freed.
     * Bump the missed counter for each client who missed an event we had to
     * throw away.
     */
    e = TAILQ_FIRST(&logfiles[logfile].queue);
    while (e) {
	if (length <= maxmem - logfiles[logfile].queuesize)
	    break;

	next = TAILQ_NEXT(e, events);

	if (pmDebug & DBG_TRACE_APPL1)
	    __pmNotifyErr(LOG_INFO, "Dropping %s: e=%p sz=%d max=%ld qsz=%ld",
			  logfiles[logfile].pmnsname, e, e->size, maxmem,
			  logfiles[logfile].queuesize);

	/* Walk clients - if event last seen, drop it and bump missed count */
	ctx_iterate(event_missed, logfile, e);

	if (pmDebug & DBG_TRACE_APPL1)
	    __pmNotifyErr(LOG_INFO, "Removing %s event %p (%d bytes)",
			  logfiles[logfile].pmnsname, e, e->size);

	TAILQ_REMOVE(&logfiles[logfile].queue, e, events);
	logfiles[logfile].queuesize -= e->size;
	free(e);
	e = next;
    }

    e = malloc(sizeof(struct event) + length + 1);
    if (e == NULL) {
	__pmNotifyErr(LOG_ERR, "event dup allocation failure: %d bytes",
			(int)(length + 1));
	return;
    }

    /* Track event data */
    e->clients = logfiles[logfile].numclients;
    memcpy(e->buffer, buffer, length);
    e->size = length;

    /* Update logfile event tracking stats */
    logfiles[logfile].count++;
    logfiles[logfile].bytes += length;

    /* Store event in queue */
    TAILQ_INSERT_TAIL(&logfiles[logfile].queue, e, events);
    logfiles[logfile].queuesize += length;

    if (pmDebug & DBG_TRACE_APPL1)
	__pmNotifyErr(LOG_INFO, "Inserted %s event %p (%d bytes) clients = %d.",
			logfiles[logfile].pmnsname, e, e->size, e->clients);
}

int
event_create(unsigned int logfile)
{
    int j;
    char *s, *p;
    size_t offset;
    ssize_t bytes;
    static char *buffer;
    static int bufsize;

    if (pmDebug & DBG_TRACE_APPL1)
	__pmNotifyErr(LOG_INFO, "Event create: logfile %d", logfile);

    /* Allocate a new event buffer to hold the initial read. */
    if (!buffer) {
	bufsize = 16 * getpagesize();
	buffer = memalign(getpagesize(), bufsize);
	if (!buffer) {
	    __pmNotifyErr(LOG_ERR, "event buffer allocation failure");
	    return -1;
	}
    }

    offset = 0;
multiread:
    bytes = read(logfiles[logfile].fd, buffer + offset, bufsize - 1 - offset);
    /*
     * Ignore the error if:
     * - we've got EOF (0 bytes read)
     * - EBADF (fd isn't valid - most likely a closed pipe)
     * - EAGAIN/EWOULDBLOCK (fd is marked nonblocking and read would block)
     * - EISDIR (fd is a directory - (possibly temporary) config file botch)
     */
    if (bytes == 0)
	return 0;
    if (bytes < 0 && (errno == EBADF || errno == EISDIR))
	return 0;
    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
	return 0;
    if (bytes > maxmem)
	return 0;
    if (bytes < 0) {
	__pmNotifyErr(LOG_ERR, "read failure on %s: %s",
		      logfiles[logfile].pathname, strerror(errno));
	return -1;
    }

    buffer[bufsize-1] = '\0';
    for (s = p = buffer, j = 0; *s != '\0' && j < bufsize-1; s++, j++) {
	if (*s != '\n')
	    continue;
	*s = '\0';
	bytes = (s+1) - p;
	event_enqueue(logfile, p, bytes);
	p = s + 1;
    }
    /* did we just do a full buffer read? */
    if (p == buffer) {
	__pmNotifyErr(LOG_ERR, "Ignoring long (%d bytes) line: \"%s\"",
				(int)bytes, event_print(p, bytes));
    } else if (j == bufsize - 1) {
	offset = bufsize-1 - (p - buffer);
	memmove(buffer, p, offset);
	goto multiread;	/* read rest of line */
    }
    return 1;
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
    struct ctx_client_data *c = ctx_get_user_data();
    int records = 0;
    int sts = ctx_get_user_access() || !logfiles[logfile].restricted;

    if (pmDebug & DBG_TRACE_APPL2)
	__pmNotifyErr(LOG_INFO, "%s called, ctx access=%d\n",
				__FUNCTION__, sts);
    *vbpp = NULL;
    if (sts != 1)
	return PM_ERR_PERMISSION;

    /*
     * Make sure the way we keep track of which clients are interested in
     * which logfiles is up to date.
     */
    if (c[logfile].active_logfile == 0) {
	c[logfile].active_logfile = 1;
	c[logfile].last = TAILQ_LAST(&logfiles[logfile].queue, tailqueue);
	logfiles[logfile].numclients++;
    }

    gettimeofday(&stamp, NULL);
    pmdaEventResetArray(eventarray);
    if ((sts = pmdaEventAddRecord(eventarray, &stamp, PM_EVENT_FLAG_POINT)) < 0)
	return sts;

    e = c[logfile].last;
    if (pmDebug & DBG_TRACE_APPL2)
	__pmNotifyErr(LOG_INFO, "%s phase2 e=%p\n", __FUNCTION__, e);

    while (e != NULL) {
	/*
	 * Add the string parameter.  Note that pmdaEventAddParam()
	 * copies the string, so we can free it soon after.
	 */
	atom.cp = e->buffer;

	if (pmDebug & DBG_TRACE_APPL1)
	    __pmNotifyErr(LOG_INFO, "Adding param: \"%s\"", 
				    event_print(e->buffer, e->size));

	sts = pmdaEventAddParam(eventarray,
				logfiles[logfile].pmid, PM_TYPE_STRING, &atom);
	if (sts < 0)
	    return sts;
	records++;

	next = TAILQ_NEXT(e, events);

	/* Remove the current one (if its use count is at 0). */
	if (--e->clients <= 0) {
	    if (pmDebug & DBG_TRACE_APPL1)
		__pmNotifyErr(LOG_INFO, "Removing %s event %p in fetch",
					logfiles[logfile].pmnsname, e);
	    TAILQ_REMOVE(&logfiles[logfile].queue, e, events);
	    logfiles[logfile].queuesize -= e->size;
	    free(e);
	}

	/* Go on to the next event. */
	e = next;
    }

    /*
     * Did this client miss any events?  The "extra" one is the last previously
     * observed event (pointed at by per-context last pointer) - so, event only
     * missed once we move past *more* than just that last observed event.
     */
    sts = c[logfile].missed_count - 1;
    c[logfile].missed_count = 0;
    if (sts > 0) {
	sts = pmdaEventAddMissedRecord(eventarray, &stamp, sts);
	if (sts < 0)
	    return sts;
	records++;
    }

    /* Update queue pointer. */
    c[logfile].last = TAILQ_LAST(&logfiles[logfile].queue, tailqueue);

    if (records > 0)
	*vbpp = (pmValueBlock *)pmdaEventGetAddr(eventarray);
    else if (logfiles[logfile].fd < 0)
	return PM_ERR_AGAIN;

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
	e = c[logfile].last;
	while (e != NULL) {
	    next = TAILQ_NEXT(e, events);

	    /* Remove the current one (if use count is at 0). */
	    if (--e->clients <= 0) {
		if (pmDebug & DBG_TRACE_APPL1)
		    __pmNotifyErr(LOG_INFO, "Removing %s event %p",
				  logfiles[logfile].pmnsname, e);
		TAILQ_REMOVE(&logfiles[logfile].queue, e, events);
		logfiles[logfile].queuesize -= e->size;
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
