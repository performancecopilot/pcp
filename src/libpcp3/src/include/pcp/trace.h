/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef PCP_TRACE_H
#define PCP_TRACE_H

/*
 * Transaction monitoring PMDA (trace) public interface.
 *
 * An example program using this interface can be found at
 * $PCP_DEMOS_DIR/trace/demo.c and contains further doumentation.
 * Also refer to the pmdatrace(1) and pmdatrace(3) man pages and
 * the Performance Co-Pilot Programmer's Guide.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Add a new entry to the table of transactions currently being monitored,
 * or update an existing entry.
 */
extern int pmtracebegin(const char *);

/*
 * Make measurements recorded for the given transaction tag available
 * through the trace PMDA.
 */
extern int pmtraceend(const char *);

/*
 * Cancel a transaction which began from an earlier call to pmtracebegin,
 * without exporting new data through the trace PMDA.
 */
extern int pmtraceabort(const char *);

/*
 * An alternate form of measurement can be obtained using pmtracepoint.
 * This is a count-only measurement, and will result in the trace PMDA
 * exporting the number of times a given code point is passed (ie. the
 * the number of times a particular label has been passed to pmtracepoint.
 */
extern int pmtracepoint(const char *);

/*
 * An extension to pmtracepoint is pmtraceobs, with similar semantics to
 * pmtracepoint except allowing an arbitrary numeric value (double) to be
 * exported up through the PMAPI.
 */
extern int pmtraceobs(const char *, double);

/*
 * Similar to pmtraceobs is pmtracecounter, with the only difference
 * being the way the trace PMDA exports the given numeric value to
 * PMAPI clients (exported with counter semantics, rather than with
 * instantaneous semantics which is the case with pmtraceobs).
 */
extern int pmtracecounter(const char *, double);

/*
 * Should any of these routines return a negative value, the return value
 * can be passed to pmtraceerrstr for the associated error message.
 * This performs a lookup into a static error message table, so the returned
 * pointer must not be freed.
 */
extern char *pmtraceerrstr(int);

#define PMTRACE_ERR_BASE	12000
#define PMTRACE_ERR_TAGNAME	(-PMTRACE_ERR_BASE-0)
#define PMTRACE_ERR_INPROGRESS	(-PMTRACE_ERR_BASE-1)
#define PMTRACE_ERR_NOPROGRESS	(-PMTRACE_ERR_BASE-2)
#define PMTRACE_ERR_NOSUCHTAG	(-PMTRACE_ERR_BASE-3)
#define PMTRACE_ERR_TAGTYPE	(-PMTRACE_ERR_BASE-4)
#define PMTRACE_ERR_TAGLENGTH	(-PMTRACE_ERR_BASE-5)
#define PMTRACE_ERR_IPC		(-PMTRACE_ERR_BASE-6)
#define PMTRACE_ERR_ENVFORMAT	(-PMTRACE_ERR_BASE-7)
#define PMTRACE_ERR_TIMEOUT	(-PMTRACE_ERR_BASE-8)
#define PMTRACE_ERR_VERSION	(-PMTRACE_ERR_BASE-9)
#define PMTRACE_ERR_PERMISSION	(-PMTRACE_ERR_BASE-10)
#define PMTRACE_ERR_CONNLIMIT	(-PMTRACE_ERR_BASE-11)

/*
 * Diagnostic and state switching
 */
extern int pmtracestate(int code);

#define PMTRACE_STATE_NONE    0  /* default: synchronous and no diagnostics */
#define PMTRACE_STATE_API     1  /* debug:   processing just below the API  */
#define PMTRACE_STATE_COMMS   2  /* debug:   shows network-related activity */
#define PMTRACE_STATE_PDU     4  /* debug:   shows app<->PMDA IPC traffic   */
#define PMTRACE_STATE_PDUBUF  8  /* debug:   internal IPC buffer management */
#define PMTRACE_STATE_NOAGENT 16 /* debug:   no PMDA communications at all  */
#define PMTRACE_STATE_ASYNC   32 /* control: use asynchronous PDU protocol  */

#ifdef __cplusplus
}
#endif

#endif /* PCP_TRACE_H */
