/*
 * Copyright (C) 2008 Aconex.  All Rights Reserved.
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
#include "pmapi.h"
#include "impl.h"
#include <string.h>
#include <winbase.h>

int
posix2win32(char *pri)
{
    if (strcmp(pri, "alert") == 0 || strcmp(pri, "warning") == 0 ||
	strcmp(pri, "warn") == 0)
	return EVENTLOG_WARNING_TYPE;
    if (strcmp(pri, "crit") == 0 || strcmp(pri, "emerg") == 0 ||
	strcmp(pri, "err") == 0 || strcmp(pri, "error") == 0 ||
	strcmp(pri, "panic") == 0)
	return EVENTLOG_ERROR_TYPE;
    if (strcmp(pri, "info") == 0 || strcmp(pri, "notice") == 0 ||
	strcmp(pri, "debug") == 0)
	return EVENTLOG_INFORMATION_TYPE;
    return -1;
}

void
append(char *buffer, int bsize, char *string)
{
    static int spaced;	/* first argument needs no whitespace-prefix */
    static int offset;

    if (spaced)
	offset += pmsprintf(buffer + offset, bsize - offset, " %s", string);
    else {
	offset += pmsprintf(buffer + offset, bsize - offset, "%s", string);
	spaced = 1;	/* remainder will all be space-prefixed */
    }
}

int
main(int argc, char **argv)
{
    HANDLE sink;
    LPCSTR msgptr;
    char buffer[256];
    char msg[32*1024];
    char *pri = NULL;
    char *tag = NULL;
    int error = 0;
    int iflag = 0;
    int sflag = 0;
    int priority;
    int c;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "ip:st:?")) != EOF) {
	switch (c) {
	case 'i':	/* process ID */
	    iflag = 1;
	    break;
	case 'p':	/* pri (facility.level) */
	    pri = optarg;
	    break;
	case 's':	/* stderr too */
	    sflag = 1;
	    break;
	case 't':	/* tag (prefix) */
	    tag = optarg;
	    break;
	default:
	    error++;
	}
    }

    if (error) {
	fprintf(stderr, "Usage: %s [ options ] message\n\n"
			"Options:\n"
			"  -i          log process identifier with each line\n"
			"  -s          log message to standard error as well\n"
			"  -p pri      enter message with specified priority\n"
			"  -t tag      mark the line with the specified tag\n",
		pmProgname);
	return 2;
    }

    /*
     * Parse priority.  Discard facility, pick out valid level names.
     */
    if (!pri)
	priority = EVENTLOG_INFORMATION_TYPE;	/* default event type */
    else {
	char *p = strrchr(pri, '.');
	if (p)
	    pri = p;
	priority = posix2win32(pri);
	if (!priority)
	    priority = EVENTLOG_INFORMATION_TYPE;	/* default event type */
    }

    /*
     * Construct the message from all contributing components.
     */
    if (iflag) {
	pmsprintf(buffer, sizeof(buffer), "[%" FMT_PID "]", (pid_t)getpid());
	append(msg, sizeof(msg), buffer);
    }
    if (tag) {
	pmsprintf(buffer, sizeof(buffer), "%s:", tag);
	append(msg, sizeof(msg), buffer);
    }
    for (c = optind; c < argc; c++)	/* insert the remaining text */
	append(msg, sizeof(msg), argv[c]);

    /*
     * Optionally write to the standard error stream (as well).
     */
    if (sflag) {
	fputs(msg, stderr);
	fputc('\n', stderr);
    }

    sink = RegisterEventSource(NULL, "Application");
    if (!sink) {
	fprintf(stderr, "%s: RegisterEventSource failed (%ld)\n",
			pmProgname, GetLastError());
	return 1;
    }
    msgptr = msg;
    if (!ReportEvent(sink, priority, 0, 0, NULL, 1, 0, &msgptr, NULL))
	fprintf(stderr, "%s: ReportEvent failed (%ld)\n",
			pmProgname, GetLastError());
    DeregisterEventSource(sink);
    return 0;
}
