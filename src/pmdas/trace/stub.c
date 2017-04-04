/*
 * stub.c - libpcp_trace stubs
 *
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcp/trace.h>

int	__pmstate = 0;

int
pmtracebegin(const char *tag)
{
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_API)
	fprintf(stderr, "pmtracebegin: start of transaction '%s'\n", tag);
#endif
    return 0;
}

int
pmtraceend(const char *tag)
{
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_API)
	fprintf(stderr, "pmtraceend: end of transaction '%s'\n", tag);
#endif
    return 0;
}

int
pmtraceabort(const char *tag)
{
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_API)
	fprintf(stderr, "pmtraceabort: transaction '%s' aborted\n", tag);
#endif
    return 0;
}

int
pmtraceobs(const char *label, double value)
{
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_API)
	fprintf(stderr, "pmtraceobs: observation '%s', value=%f\n", label, value);
#endif
    return 0;
}

int
pmtracecounter(const char *label, double value)
{
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_API)
	fprintf(stderr, "pmtracecounter: counter '%s', value=%f\n", label, value);
#endif
    return 0;
}

int
pmtracepoint(const char *label)
{
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_API)
	fprintf(stderr, "pmtracepoint: trace point '%s' reached\n", label);
#endif
    return 0;
}

int
pmtracestate(int code)
{
    return(__pmstate |= code);
}

char *
pmtraceerrstr(int code)
{
    static const struct {
	int	code;
	char	*msg;
    } errtab[] = {
	{ PMTRACE_ERR_TAGNAME,
		"Invalid tag name - tag names cannot be NULL" },
	{ PMTRACE_ERR_INPROGRESS,
		"Transaction is already in progress - cannot begin" },
	{ PMTRACE_ERR_NOPROGRESS,
		"Transaction is not currently in progress - cannot end" },
	{ PMTRACE_ERR_NOSUCHTAG,
		"Transaction tag was not successfully initialised" },
	{ PMTRACE_ERR_TAGTYPE,
		"Tag is already in use for a different type of tracing" },
	{ PMTRACE_ERR_TAGLENGTH,
		"Tag name is too long (maximum 256 characters)" },
	{ PMTRACE_ERR_IPC,
		"IPC protocol failure" },
	{ PMTRACE_ERR_ENVFORMAT,
		"Unrecognised environment variable format" },
	{ PMTRACE_ERR_TIMEOUT,
		"Application timed out connecting to the PMDA" },
	{ PMTRACE_ERR_VERSION,
		"Incompatible versions between application and PMDA" },
	{ PMTRACE_ERR_PERMISSION,
		"Cannot connect to PMDA - permission denied" },
        { PMTRACE_ERR_CONNLIMIT,
                "Cannot connect to PMDA - connection limit reached" },
	{ 0, "" }
    };

    if ((code < 0) && (code > -PMTRACE_ERR_BASE))	/* catch intro(2) errors */
	return strerror(-code);
    else if (code == 0)
	return "No error";
    else {
	int	i;
	for (i=0; errtab[i].code; i++) {
	    if (errtab[i].code == code)
		return errtab[i].msg;
	}
    }
    return "Unknown error code";
}
