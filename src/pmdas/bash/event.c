/*
 * Event support for the bash tracing PMDA
 *
 * Copyright (c) 2012 Nathan Scott.  All rights reversed.
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

#include "event.h"
#include "pmda.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <regex.h>

static char *prefix = "pcp-bash";
static char *pcptmpdir;			/* probably /var/tmp */
static char pidpath[MAXPATHLEN];

static int
process_init(const char *bashname, bash_process_t *bash)
{
    char *endnum;
    char path[MAXPATHLEN];

    if (pmDebug & DBG_TRACE_APPL2)
	__pmNotifyErr(LOG_DEBUG, "process_init check: %s", bashname);

    bash->pid = (pid_t) strtoul(bashname, &endnum, 10);
    if (*endnum != '\0' || bash->pid < 1)
	return -1;
    snprintf(path, sizeof(path), "%s%c%s", pidpath, __pmPathSeparator(), bashname);
    if ((bash->fd = open(path, O_RDONLY | O_NONBLOCK)) < 0)
	return -1;
    if (fstat(bash->fd, &bash->stat) < 0 || !S_ISFIFO(bash->stat.st_mode)) {
	close(bash->fd);
	return -1;
    }

    if (pmDebug & DBG_TRACE_APPL2)
	__pmNotifyErr(LOG_DEBUG, "process_init pass: %s", path);
    return 0;
}

static bash_process_t *
process_fill(const char *bashname, bash_process_t *init)
{
    int queueid = pmdaEventNewQueue(bashname, bash_maxmem);
    size_t size = sizeof(*init) + strlen(bashname) + 1;
    bash_process_t *bashful = malloc(size);

    if (!bashful) {
	__pmNotifyErr(LOG_ERR, "process allocation out of memory");
	return NULL;
    }
    if (queueid < 0) {
	__pmNotifyErr(LOG_ERR, "attempt to dup queue for %s", bashname);
	free(bashful);
	return NULL;
    }

    bashful->fd = init->fd;
    bashful->pid = init->pid;
    bashful->stat = init->stat;
    bashful->first = 1;
    bashful->queueid = queueid;
    strcpy(bashful->basename, bashname);

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "process_fill: %s, queueid=%d", bashname, queueid);

    return bashful;
}

static void
process_timestamp(bash_process_t *process, struct timeval *timestamp)
{
    timestamp->tv_sec = process->stat.st_mtim.tv_sec;
    timestamp->tv_usec = process->stat.st_mtim.tv_nsec / 1000;
}

static int
process_read(bash_process_t *process)
{
    int j;
    char *s, *p;
    size_t offset;
    ssize_t bytes;
    struct timeval timestamp;

    static char *buffer;
    static int bufsize;

    /*
     * Using a static (global) event buffer to hold initial read.
     * The aim is to reduce memory allocation until we know we'll
     * need to keep something.
     */
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
    if (process->fd < 0)
    	return 0;

    bytes = read(process->fd, buffer + offset, bufsize - 1 - offset);

    /*
     * Ignore the error if:
     * - we've got EOF (0 bytes read)
     * - EAGAIN/EWOULDBLOCK (fd is marked nonblocking and read would block)
     */
    if (bytes == 0)
	return 0;
    if (bytes < 0 && (errno == EBADF))
	return 0;
    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
	return 0;
    if (bytes > bash_maxmem)
	return 0;
    if (bytes < 0) {
	__pmNotifyErr(LOG_ERR, "read failure on process %s: %s",
		      process->basename, strerror(errno));
	return -1;
    }

    process_timestamp(process, &timestamp);

    buffer[bufsize-1] = '\0';
    for (s = p = buffer, j = 0; *s != '\0' && j < bufsize-1; s++, j++) {
	if (*s != '\n')
	    continue;
	*s = '\0';
	bytes = (s+1) - p;
	pmdaEventQueueAppend(process->queueid, p, bytes, &timestamp);
	p = s + 1;
    }
    /* did we just do a full buffer read? */
    if (p == buffer) {
	char msg[64];
	__pmNotifyErr(LOG_ERR, "Ignoring long (%d bytes) line: \"%s\"", (int)
			bytes, __pmdaEventPrint(p, bytes, msg, sizeof(msg)));
    } else if (j == bufsize - 1) {
	offset = bufsize-1 - (p - buffer);
	memmove(buffer, p, offset);
	goto multiread;	/* read rest of line */
    }
    return 1;
}

static void
process_done(bash_process_t *process)
{
    struct timeval timestamp;

    if (process->exited == 0) {
	process->exited = (__pmProcessExists(process->pid) == 0);
	/* empty event inserted into queue to denote bash has exited */
	if (process->exited == 1) {
	    process_timestamp(process, &timestamp);
	    pmdaEventQueueAppend(process->queueid, NULL, 0, &timestamp);
	}
    }
}

void
event_refresh(pmInDom bash_indom)
{
    struct dirent **files;
    bash_process_t init, *bp;
    int i, id, sts, num = scandir(pidpath, &files, NULL, NULL);

    if (pmDebug & DBG_TRACE_APPL2)
	__pmNotifyErr(LOG_DEBUG, "event_refresh");

    pmdaCacheOp(bash_indom, PMDA_CACHE_INACTIVE);

    for (i = 0; i < num; i++) {
	char	*processid = files[i]->d_name;

	if (processid[0] == '.')
	    continue;
	if (process_init(processid, &init) < 0)
	    continue;

	/* either create or re-activate a bash process structure */
	sts = pmdaCacheLookupName(bash_indom, processid, &id, (void **)&bp);
	if ((sts != PMDA_CACHE_INACTIVE) &&
	   ((bp = process_fill(processid, &init)) == NULL))
	    continue;
	pmdaCacheStore(bash_indom, PMDA_CACHE_ADD, bp->basename, (void *)bp);

	/* read any/all new events for this bash process, enqueue 'em */
	process_read(bp);

	/* check if process is running and generate end marker if not */
	process_done(bp);
    }
}

void
event_init(void)
{
    pcptmpdir = pmGetConfig("PCP_TMP_DIR");
    sprintf(pidpath, "%s%c%s", pcptmpdir, __pmPathSeparator(), prefix);
}
