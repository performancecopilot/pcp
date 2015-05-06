/*
 * Event support for the Logger PMDA
 *
 * Copyright (c) 2011-2012 Red Hat.
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

#include "event.h"
#include "pmda.h"
#include "util.h"
#include <ctype.h>
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

static int numlogfiles;
static event_logfile_t *logfiles;

void
event_init(pmID pmid)
{
    char cmd[MAXPATHLEN];
    int	i, fd;

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
		if (fstat(fd, &logfiles[i].pathstat) < 0)
		    if (logfiles[i].fd >= 0)	/* log once only */
			__pmNotifyErr(LOG_ERR, "fstat: %s - %s",
				    logfiles[i].pathname, strerror(errno));
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
	logfiles[i].pmid = pmid;	/* string param metric identifier */
	logfiles[i].queueid = pmdaEventNewQueue(logfiles[i].pmnsname, maxmem);
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

/*
 * Ensure given name (identifier) can be used as a namespace entry.
 */
static int
valid_pmns_name(char *name)
{
    if (!isalpha((int)name[0]))
	return 0;
    for (; *name != '\0'; name++)
	if (!isalnum((int)*name) && *name != '_')
	    return 0;
    return 1;
}

/*
 * Parse the configuration file and do initial data structure setup.
 */
int
event_config(const char *fname)
{
    FILE		*configFile;
    event_logfile_t	*logfile;
    int			sts = 0;
    size_t		len;
    char		line[MAXPATHLEN * 2];
    char		*ptr, *name, *noaccess;

    configFile = fopen(fname, "r");
    if (configFile == NULL) {
	__pmNotifyErr(LOG_ERR, "event_config: %s: %s", fname, strerror(errno));
	return -1;
    }

    while (!feof(configFile)) {
	if (fgets(line, sizeof(line), configFile) == NULL) {
	    if (feof(configFile))
		break;
	    __pmNotifyErr(LOG_ERR, "event_config: fgets: %s", strerror(errno));
	    sts = -1;
	    break;
	}

	/*
	 * fgets() puts the '\n' at the end of the buffer.  Remove
	 * it.  If it isn't there, that must mean that the line is
	 * longer than our buffer.
	 */
	len = strlen(line);
	if (len == 0)			/* Ignore empty strings. */
	    continue;
	if (line[len - 1] != '\n') { /* String must be too long */
	    __pmNotifyErr(LOG_ERR, "event_config: config line too long: '%s'",
			  line);
	    sts = -1;
	    break;
	}
	line[len - 1] = '\0';		/* Remove the '\n'. */

	/* Strip all trailing whitespace. */
	rstrip(line);

	/* If the string is now empty or a comment, just ignore the line. */
	len = strlen(line);
	if (len == 0)
	    continue;
	if (line[0] == '#')
	    continue;

	/* Skip past all leading whitespace to find the start of
	 * NAME. */
	ptr = name = lstrip(line);

	/* Now we need to split the line into 3 parts: NAME, ACCESS
	 * and PATHNAME.  NAME can't have whitespace in it, so look
	 * for the first non-whitespace. */
	while (*ptr != '\0' && ! isspace((int)*ptr)) {
	    ptr++;
	}
	/* If we're at the end, we didn't find any whitespace, so
	 * we've only got a NAME, with no ACCESS/PATHNAME. */
	if (*ptr == '\0') {
	    __pmNotifyErr(LOG_ERR, "event_config: badly formatted "
				   " configuration file line: '%s'", line);
	    sts = -1;
	    break;
	}
	/* Terminate NAME at the 1st whitespace. */
	*ptr++ = '\0';

	/* Make sure NAME isn't too long. */
	if (strlen(name) > MAXPATHLEN) {
	    __pmNotifyErr(LOG_ERR, "event_config: name too long: '%s'", name);
	    sts = -1;
	    break;
	}

	/* Make sure NAME is valid. */
	if (valid_pmns_name(name) == 0) {
	    __pmNotifyErr(LOG_ERR, "event_config: invalid name: '%s'", name);
	    sts = -1;
	    break;
	}

	/* Skip past any extra whitespace between NAME and ACCESS */
	ptr = noaccess = lstrip(ptr);

	/* Look for the next whitespace, and that terminate ACCESS */
	while (*ptr != '\0' && ! isspace((int)*ptr)) {
	    ptr++;
	}

	/* If we're at the end, we didn't find any whitespace, so
	 * we've only got NAME and ACCESS with no/PATHNAME. */
	if (*ptr == '\0') {
	    __pmNotifyErr(LOG_ERR, "event_config: badly formatted "
				   " configuration file line: '%s'", line);
	    sts = -1;
	    break;
	}
	/* Terminate ACCESS at the 1st whitespace. */
	*ptr++ = '\0';

	/* Skip past any extra whitespace between ACCESS and PATHNAME */
	ptr = lstrip(ptr);

	/* Make sure PATHNAME (the rest of the line) isn't too long. */
	if (strlen(ptr) > MAXPATHLEN) {
	    __pmNotifyErr(LOG_ERR, "event_config: path is too long: '%s'", ptr);
	    sts = -1;
	    break;
	}

	/* Now we've got a reasonable NAME/ACCESS/PATHNAME.  Save them. */
	len = (numlogfiles + 1) * sizeof(event_logfile_t);
	logfiles = realloc(logfiles, len);
	if (logfiles == NULL) {
	    __pmNoMem("event_config", len, PM_FATAL_ERR);
	    sts = -1;
	    break;
	}
	logfile = &logfiles[numlogfiles];
	memset(logfile, 0, sizeof(*logfile));
	logfile->noaccess = (noaccess[0] == 'y' || noaccess[0] == 'Y');
	strncpy(logfile->pmnsname, name, sizeof(logfile->pmnsname));
	logfile->pmnsname[sizeof(logfile->pmnsname)-1] = '\0';
	strncpy(logfile->pathname, ptr, sizeof(logfile->pathname));
	logfile->pathname[sizeof(logfile->pathname)-1] = '\0';
	/* remaining fields filled in after pmdaInit() is called. */
	numlogfiles++;

	if (pmDebug & DBG_TRACE_APPL0)
	    __pmNotifyErr(LOG_INFO, "event_config: new logfile %s (%s)",
				logfile->pathname, logfile->pmnsname);
    }

    fclose(configFile);
    if (sts < 0) {
	free(logfiles);
	return sts;
    }
    if (numlogfiles == 0) {
	__pmNotifyErr(LOG_ERR, "event_config: no valid log files found");
	return -1;
    }
    return numlogfiles;
}

static int
event_create(event_logfile_t *logfile)
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
    if (logfile->fd < 0)
    	return 0;
    bytes = read(logfile->fd, buffer + offset, bufsize - 1 - offset);
    /*
     * Ignore the error if:
     * - we've got EOF (0 bytes read)
     * - EBADF (fd isn't valid - most likely a closed pipe)
     * - EAGAIN/EWOULDBLOCK (fd is marked nonblocking and read would block)
     * - EINVAL/EISDIR (fd is a directory - config file botch)
     */
    if (bytes == 0)
	return 0;
    if (bytes < 0 && (errno == EBADF || errno == EISDIR || errno == EINVAL))
	return 0;
    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
	return 0;
    if (bytes > maxmem)
	return 0;
    if (bytes < 0) {
	__pmNotifyErr(LOG_ERR, "read failure on %s: %s",
		      logfile->pathname, strerror(errno));
	return -1;
    }

    gettimeofday(&timestamp, NULL);
    buffer[bufsize-1] = '\0';
    for (s = p = buffer, j = 0; *s != '\0' && j < bufsize-1; s++, j++) {
	if (*s != '\n')
	    continue;
	*s = '\0';
	bytes = (s+1) - p;
	pmdaEventQueueAppend(logfile->queueid, p, bytes, &timestamp);
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

void
event_refresh(void)
{
    struct event_logfile *logfile;
    struct stat pathstat;
    int i, fd, sts;

    for (i = 0; i < numlogfiles; i++) {
	logfile = &logfiles[i];

	if (logfile->pid > 0)	/* process pipe */
	    goto events;
	if (stat(logfile->pathname, &pathstat) < 0) {
	    if (logfile->fd >= 0) {
		close(logfile->fd);
		logfile->fd = -1;
	    }
	    memset(&logfile->pathstat, 0, sizeof(logfile->pathstat));
	} else {
	    /* reopen if no descriptor before, or log rotated (new file) */
	    if (logfile->fd < 0 ||
	        logfile->pathstat.st_ino != pathstat.st_ino ||
		logfile->pathstat.st_dev != pathstat.st_dev) {
		if (logfile->fd >= 0)
		    close(logfile->fd);
		fd = open(logfile->pathname, O_RDONLY|O_NONBLOCK);
		if (fd < 0 && logfile->fd >= 0)	/* log once */
		    __pmNotifyErr(LOG_ERR, "open: %s - %s",
				logfile->pathname, strerror(errno));
		logfile->fd = fd;
	    } else {
		if ((S_ISREG(pathstat.st_mode)) &&
		    (memcmp(&logfile->pathstat.st_mtime, &pathstat.st_mtime,
			    sizeof(pathstat.st_mtime))) == 0)
		    continue;
	    }
	    logfile->pathstat = pathstat;
events:
	    do {
		sts = event_create(logfile);
	    } while (sts != 0);
	}
    }
}

int
event_logcount(void)
{
    return numlogfiles;
}

int
event_queueid(int handle)
{
    if (handle < 0 || handle >= numlogfiles)
	return 0;

    /* if logfile unrestricted, allow this client access to this queue */
    if (logfiles[handle].noaccess == 0)
	pmdaEventSetAccess(pmdaGetContext(), logfiles[handle].queueid, 1);

    return logfiles[handle].queueid;
}

__uint64_t
event_pathsize(int handle)
{
    if (handle < 0 || handle >= numlogfiles)
	return 0;
    return logfiles[handle].pathstat.st_size;
}

const char *
event_pathname(int handle)
{
    if (handle < 0 || handle >= numlogfiles)
	return NULL;
    return logfiles[handle].pathname;
}

const char *
event_pmnsname(int handle)
{
    if (handle < 0 || handle >= numlogfiles)
	return NULL;
    return logfiles[handle].pmnsname;
}

pmID
event_pmid(int handle)
{
    if (handle < 0 || handle >= numlogfiles)
	return 0;
    return logfiles[handle].pmid;
}

int
event_decoder(int eventarray, void *buffer, size_t size,
		struct timeval *timestamp, void *data)
{
    int sts, handle = *(int *)data;
    pmID pmid = event_pmid(handle);
    pmAtomValue atom;

    sts = pmdaEventAddRecord(eventarray, timestamp, PM_EVENT_FLAG_POINT);
    if (sts < 0)
	return sts;
    atom.cp = buffer;
    sts = pmdaEventAddParam(eventarray, pmid, PM_TYPE_STRING, &atom);
    if (sts < 0)
	return sts;
    return 1;	/* simple decoder, added just one event array */
}

int
event_regex_apply(void *rp, void *data, size_t size)
{
    regex_t *regex = (regex_t *)rp;
    return regexec(regex, data, 0, NULL, 0) == REG_NOMATCH;
}

void
event_regex_release(void *rp)
{
    regex_t *regex = (regex_t *)rp;
    regfree(regex);
}

int
event_regex_alloc(const char *string, void **filter)
{
    regex_t *regex = malloc(sizeof(regex_t));

    if (regex == NULL)
	return -ENOMEM;
    if (regcomp(regex, string, REG_EXTENDED|REG_NOSUB) != 0) {
	free(regex);
	return PM_ERR_BADSTORE;
    }
    *filter = (void *)regex;
    return 0;
}
