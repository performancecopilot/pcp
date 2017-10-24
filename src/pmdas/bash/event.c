/*
 * Event support for the bash tracing PMDA
 *
 * Copyright (c) 2016 Red Hat.
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

#include <unistd.h>
#include <ctype.h>
#include "event.h"
#include "pmda.h"

static char *prefix = "pmdabash";
static char *pcptmpdir;			/* probably /var/tmp */
static char pidpath[MAXPATHLEN];

/*
 * Extract time of creation of the trace files.  Initially uses header file
 * as a reference since its created initially and then never modified again.
 * Subsequent calls will use last modification to the trace data file.
 */
void
process_stat_timestamp(bash_process_t *process, struct timeval *timestamp)
{
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
    timestamp->tv_sec = process->stat.st_mtime.tv_sec;
    timestamp->tv_usec = process->stat.st_mtime.tv_nsec / 1000;
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
    timestamp->tv_sec = process->stat.st_mtimespec.tv_sec;
    timestamp->tv_usec = process->stat.st_mtimespec.tv_nsec / 1000;
#elif defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESPEC_T)
    timestamp->tv_sec = process->stat.st_mtim.tv_sec;
    timestamp->tv_usec = process->stat.st_mtim.tv_nsec / 1000;
#else
!bozo!
#endif
}

/*
 * Parse the header file (/path/.pid) containing xtrace metadata.
 * Helper routine, used during initialising of a tracked shell.
 */
static void
process_head_parser(bash_process_t *verify, const char *buffer, size_t size)
{
    char *p = (char *)buffer, *end = (char *)buffer + size - 1;
    char script[1024];
    int version = 0;
    int date = 0;

    p += extract_int(p, "version:", sizeof("version:")-1, &version);
    p += extract_int(p, "ppid:", sizeof("ppid:")-1, &verify->parent);
    p += extract_int(p, "date:", sizeof("date:")-1, &date);
    extract_cmd(p, end - p, "+", sizeof("+")-1, script, sizeof(script));

    if (date) {
	/* Use the given starttime of the script from the header */
	verify->starttime.tv_sec = date;
	verify->starttime.tv_usec = 0;
    } else {
	/* Use a timestamp from the header as a best-effort guess */
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
	verify->starttime.tv_sec = verify->stat.st_mtime.tv_sec;
	verify->starttime.tv_usec = verify->stat.st_mtime.tv_nsec / 1000;
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
	verify->starttime.tv_sec = verify->stat.st_mtimespec.tv_sec;
	verify->starttime.tv_usec = verify->stat.st_mtimespec.tv_nsec / 1000;
#elif defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESPEC_T)
	verify->starttime.tv_sec = verify->stat.st_mtim.tv_sec;
	verify->starttime.tv_usec = verify->stat.st_mtim.tv_nsec / 1000;
#else
!bozo!
#endif
    }
    verify->version = version;

    size = 16 + strlen(script);		/* pid and script name */
    verify->instance = malloc(size);
    pmsprintf(verify->instance, size, "%u %s", verify->pid, script);

    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "process header v%d: inst='%s' ppid=%d",
			verify->version, verify->instance, verify->parent);
}

/*
 * Verify the header file (/path/.pid) containing xtrace metadata.
 * Helper routine, used during initialising of a tracked shell.
 */
static int
process_head_verify(const char *filename, bash_process_t *verify)
{
    size_t size;
    char buffer[1024];
    int fd = open(filename, O_RDONLY);

    if (fd < 0)
	return fd;
    if (fstat(fd, &verify->stat) < 0 || !S_ISREG(verify->stat.st_mode)) {
	close(fd);
	return -1;
    }

    size = read(fd, buffer, sizeof(buffer));
    if (size > 0)
	process_head_parser(verify, buffer, size);
    close(fd);

    /* make sure we only parse header/trace file formats we understand */
    if (verify->version < MINIMUM_VERSION || verify->version > MAXIMUM_VERSION)
	return -1;
    return 0;
}

/*
 * Verify the data files associated with a traced bash process.
 * Helper routine, used during initialising of a tracked shell.
 */
static int
process_verify(const char *bashname, bash_process_t *verify)
{
    int fd;
    char *endnum;
    char path[MAXPATHLEN];
    struct stat stat;

    verify->pid = (pid_t) strtoul(bashname, &endnum, 10);
    if (*endnum != '\0' || verify->pid < 1)
	return -1;

    pmsprintf(path, sizeof(path), "%s%c.%s", pidpath, __pmPathSeparator(), bashname);
    if (process_head_verify(path, verify) < 0)
	return -1;

    pmsprintf(path, sizeof(path), "%s%c%s", pidpath, __pmPathSeparator(), bashname);
    if ((fd = open(path, O_RDONLY | O_NONBLOCK)) < 0)
	return -1;
    if (fstat(fd, &stat) < 0 || !S_ISFIFO(stat.st_mode)) {
	close(fd);
	return -1;
    }
    verify->fd = fd;
    return 0;
}

/*
 * Finally allocate memory for a verified, traced bash process.
 * Helper routine, used during initialising of a tracked shell.
 */
static bash_process_t *
process_alloc(const char *bashname, bash_process_t *init, int numclients)
{
    int queueid = pmdaEventNewActiveQueue(bashname, bash_maxmem, numclients);
    bash_process_t *bashful = malloc(sizeof(bash_process_t));

    if (pmDebugOptions.appl1)
	__pmNotifyErr(LOG_DEBUG, "process_alloc: %s, queueid=%d", bashname, queueid);

    if (!bashful) {
	__pmNotifyErr(LOG_ERR, "process allocation out of memory");
	return NULL;
    }
    if (queueid < 0) {
	__pmNotifyErr(LOG_ERR, "attempt to dup queue for %s", bashname);
	free(bashful);
	return NULL;
    }

    /* Tough access situation - how to log without this? */
    pmdaEventSetAccess(pmdaGetContext(), queueid, 1);

    bashful->fd = init->fd;
    bashful->pid = init->pid;
    bashful->parent = init->parent;
    bashful->queueid = queueid;
    bashful->exited = 0;
    bashful->finished = 0;
    bashful->noaccess = 0;
    bashful->version = init->version;
    bashful->padding = 0;

    memcpy(&bashful->starttime, &init->starttime, sizeof(struct timeval));
    memcpy(&bashful->stat, &init->stat, sizeof(struct stat));
    /* copy of first stat time, identifies first event */
    process_stat_timestamp(bashful, &bashful->startstat);
    /* copy of pointer to dynamically allocated memory */
    bashful->instance = init->instance;

    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "process_alloc: %s", bashful->instance);

    return bashful;
}

int
event_start(bash_process_t *bp, struct timeval *timestamp)
{
    int	start = memcmp(timestamp, &bp->startstat, sizeof(*timestamp));

    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "check start event for %s (%d), %ld vs %ld",
		bp->instance, start, (long int)bp->startstat.tv_sec, (long int)timestamp->tv_sec);

    return start == 0;
}

/*
 * Initialise a bash process data structure using the header and
 * trace file.  Ready to accept event traces from this shell on
 * completion of this routine - file descriptor setup, structure
 * filled with all metadata (exported) about this process.
 * Note: this is using an on-stack process structure, only if it
 * all checks out will we allocate memory for it, and keep it.
 */
static int
process_init(const char *bashname, bash_process_t **bp)
{
    bash_process_t init = { 0 };

    pmAtomValue atom;
    pmdaEventClients(&atom);

    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "process_init: %s (%d clients)",
			bashname, atom.ul);

    if (process_verify(bashname, &init) < 0)
	return -1;
    *bp = process_alloc(bashname, &init, atom.ul);
    if (*bp == NULL)
	return -1;
    return 0;
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
	int	sts = 0;
	bufsize = 16 * getpagesize();
#ifdef HAVE_POSIX_MEMALIGN
	sts = posix_memalign((void **)&buffer, getpagesize(), bufsize);
#else
#ifdef HAVE_MEMALIGN
	buffer = (char *)memalign(getpagesize(), bufsize);
	if (buffer == NULL) sts = -1;
#else
	buffer = (char *)malloc(bufsize);
	if (buffer == NULL) sts = -1;
#endif
#endif
	if (sts != 0) {
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
    if (bytes < 0 && (errno == EBADF || errno == EISDIR || errno == EINVAL))
	return 0;
    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
	return 0;
    if (bytes > bash_maxmem)
	return 0;
    if (bytes < 0) {
	__pmNotifyErr(LOG_ERR, "read failure on process %s: %s",
		      process->instance, strerror(errno));
	return -1;
    }

    process_stat_timestamp(process, &timestamp);

    buffer[offset+bytes] = '\0';
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
process_unlink(bash_process_t *process, const char *bashname)
{
    char path[MAXPATHLEN];

    pmsprintf(path, sizeof(path), "%s%c%s", pidpath, __pmPathSeparator(), bashname);
    unlink(path);
    pmsprintf(path, sizeof(path), "%s%c.%s", pidpath, __pmPathSeparator(), bashname);
    unlink(path);

    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "process_unlink: removed %s", bashname);
}

static int
process_drained(bash_process_t *process)
{
    pmAtomValue value = { 0 };

    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "process_queue_drained check on queue %d (pid %d)",
		      process->queueid, process->pid);
    if (pmdaEventQueueMemory(process->queueid, &value) < 0)
	return 1;	/* error, consider it drained and cleanup */
    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "process_queue_drained: %s (%llu)", value.ll?"n":"y", (long long)value.ull);
    return value.ull == 0;
}

static void
process_done(bash_process_t *process, const char *bashname)
{
    struct timeval timestamp;

    if (process->exited == 0) {
	process->exited = (__pmProcessExists(process->pid) == 0);

	if (!process->exited)
	    return;
	/* empty event inserted into queue to denote bash has exited */
	if (!process->finished) {
	    process->finished = 1;	/* generate no further events */
	    process_stat_timestamp(process, &timestamp);
	    pmdaEventQueueAppend(process->queueid, NULL, 0, &timestamp);

	    if (pmDebugOptions.appl0)
		__pmNotifyErr(LOG_DEBUG, "process_done: marked queueid %d (pid %d) done",
					process->queueid, process->pid);
	}
    }

    if (process->finished) {
	/* once all clients have seen final events, clean named queue */
	if (process_drained(process))
	    process_unlink(process, bashname);
    }
}

void
event_refresh(pmInDom bash_indom)
{
    struct dirent **files;
    bash_process_t *bp;
    int i, id, sts, num = scandir(pidpath, &files, NULL, NULL);

    if (pmDebugOptions.appl0 && num > 2)
	__pmNotifyErr(LOG_DEBUG, "event_refresh: phase1: %d files", num - 2);

    pmdaCacheOp(bash_indom, PMDA_CACHE_INACTIVE);

    /* (re)activate processes that are actively generating events */
    for (i = 0; i < num; i++) {
	char	*processid = files[i]->d_name;

	if (processid[0] == '.')
	    continue;

	/* either create or re-activate a bash process structure */
	sts = pmdaCacheLookupName(bash_indom, processid, &id, (void **)&bp);
	if (sts != PMDA_CACHE_INACTIVE) {
	    if (process_init(processid, &bp) < 0)
		continue;
	}
	pmdaCacheStore(bash_indom, PMDA_CACHE_ADD, bp->instance, (void *)bp);

	/* read any/all new events for this bash process, enqueue 'em */
	process_read(bp);

	/* check if process is running and generate end marker if not */
	process_done(bp, files[i]->d_name);
    }

    for (i = 0; i < num; i++)
	free(files[i]);
    if (num > 0)
	free(files);
}

void
event_init(void)
{
    int sep = __pmPathSeparator();
    pcptmpdir = pmGetConfig("PCP_TMP_DIR");
    pmsprintf(pidpath, sizeof(pidpath), "%s%c%s", pcptmpdir, sep, prefix);
}
