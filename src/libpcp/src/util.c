/*
 * General Utility Routines
 *
 * Copyright (c) 2012-2018,2021-2022 Red Hat.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
 * Copyright (c) 1995-2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 *
 * Thread-safe notes
 *
 * pmState - no side-effects, don't bother locking
 *
 * dosyslog - no side-effects, other than non-determinism with concurrent
 *	attempts to set/clear the state in pmSyslog() which locking will
 *	not avoid
 *
 * pmProgname - most likely set in main(), not worth protecting here
 *
 * base (in __pmProcessDataSize) - no real side-effects, don't bother
 *	locking
 *
 * done_exit is protected by the util_lock mutex.
 *
 * filelog[] and nfilelog are protected by the util_lock mutex.
 *
 * msgbuf, msgbuflen and msgsize are all protected by the util_lock mutex.
 *
 * the one-trip initialiation of filename is guarded by the __pmLock_extcall
 * mutex.
 *
 * the one-trip initialization of xconfirm is guarded by xconfirm_init
 * ... there is no locking here as the same value would result from
 * concurrent execution, and we don't want to hold util_lock when
 * calling pmGetOptionalConfig()
 *
 * errtype - no side-effects (same value would result from concurrent
 *	execution of the initialization block, unchanged after that),
 *	don't bother locking
 */

#include <stdarg.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>

#include "pmapi.h"
#include "libpcp.h"
#include "fault.h"
#include "deprecated.h"
#include "pmdbg.h"
#include "internal.h"
#include "deprecated.h"

#if defined(HAVE_SYS_TIMES_H)
#include <sys/times.h>
#endif
#if defined(HAVE_SYS_MMAN_H)
#include <sys/mman.h>
#endif
#if defined(HAVE_IEEEFP_H)
#include <ieeefp.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#if defined(IS_DARWIN)
#include <sys/sysctl.h>
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#define STR2(x) #x
#define STR(X) STR2(X)

static FILE	**filelog;
static int	nfilelog;
static int	dosyslog;
static int	pmState = PM_STATE_APPL;
static int	done_exit;
static int	xconfirm_init;
static char 	*xconfirm;

PCP_DATA char	*pmProgname = "pcp";		/* the real McCoy */

PCP_DATA int	pmDebug;			/* the real McCoy ... old style */
PCP_DATA pmdebugoptions_t	pmDebugOptions;	/* the real McCoy ... new style */

static int vpmprintf(const char *, va_list);

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	util_lock = PTHREAD_MUTEX_INITIALIZER;
#else
void			*util_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == util_lock
 */
int
__pmIsUtilLock(void *lock)
{
    return lock == (void *)&util_lock;
}
#endif

char *
pmGetProgname(void)
{
    return pmProgname;
}

/*
 * if onoff == 1, logging is to syslog and stderr, else logging is
 * just to stderr (this is the default)
 */
void
pmSyslog(int onoff)
{
    dosyslog = onoff;
    if (dosyslog)
	openlog("pcp", LOG_PID, LOG_DAEMON);
    else
	closelog();
}

/*
 * This is a wrapper around syslog(3) that writes similar messages to stderr,
 * but if pmSyslog() is called, the messages will really go to syslog
 */
void
pmNotifyErr(int priority, const char *message, ...)
{
    va_list		arg;

    va_start(arg, message);
    notifyerr(priority, message, arg);
    va_end(arg);
}

void
notifyerr(int priority, const char *message, va_list arg)
{
    char		*p;
    char		*level;
    struct timeval	tv;
    char		ct_buf[26];
    time_t		now;

    gettimeofday(&tv, NULL);

    if (dosyslog) {
	char	syslogmsg[2048];

	vsnprintf(syslogmsg, sizeof(syslogmsg), message, arg);
	syslog(priority, "%s", syslogmsg);
    }

    /*
     * do the stderr equivalent
     */

    switch (priority) {
    	case LOG_EMERG :
	    level = "Emergency";
	    break;
    	case LOG_ALERT :
	    level = "Alert";
	    break;
    	case LOG_CRIT :
	    level = "Critical";
	    break;
    	case LOG_ERR :
	    level = "Error";
	    break;
    	case LOG_WARNING :
	    level = "Warning";
	    break;
    	case LOG_NOTICE :
	    level = "Notice";
	    break;
    	case LOG_INFO :
	    level = "Info";
	    break;
    	case LOG_DEBUG :
	    level = "Debug";
	    break;
	default:
	    level = "???";
	    break;
    }

    now = tv.tv_sec;
    ctime_r(&now, ct_buf);
    /* when profiling use "[%.19s.%lu]" for extra precision */
    pmprintf("[%.19s] %s(%" FMT_PID ") %s: ", ct_buf,
		/* (unsigned long)tv.tv_usec, */
		pmGetProgname(), (pid_t)getpid(), level);
    vpmprintf(message, arg);
    /* trailing \n if needed */
    for (p = (char *)message; *p; p++)
	;
    if (p == message || p[-1] != '\n')
	pmprintf("\n");
    pmflush();
}

static void
logheader(const char *progname, FILE *log, const char *act)
{
    char	host[MAXHOSTNAMELEN];
    time_t	now;
    char	ct_buf[26];

    setlinebuf(log);		/* line buffering for log files */
    gethostname(host, MAXHOSTNAMELEN);
    host[MAXHOSTNAMELEN-1] = '\0';
    time(&now);
    ctime_r(&now, ct_buf);
    fprintf(log, "Log for %s on %s %s %s\n", progname, host, act, ct_buf);
}

static void
logfooter(FILE *log, const char *act)
{
    time_t	now;
    char	ct_buf[26];

    time(&now);
    ctime_r(&now, ct_buf);
    fprintf(log, "\nLog %s %s", act, ct_buf);
}

static void
logonexit(void)
{
    int		i;

    PM_LOCK(util_lock);
    if (++done_exit != 1) {
	PM_UNLOCK(util_lock);
	return;
    }

    for (i = 0; i < nfilelog; i++)
	logfooter(filelog[i], "finished");

    PM_UNLOCK(util_lock);
}

/*
 * common code shared by __pmRotateLog and pmOpenLog.
 * Not called at all if the log is "-".
 */
static FILE *
logreopen(const char *progname, const char *logname, FILE *oldstream,
	    int *status)
{
    int		oldfd;
    int		dupoldfd;
    FILE	*outstream = oldstream;
    FILE	*dupoldstream = oldstream;
    char	errmsg[PM_MAXERRMSGLEN];

    /*
     * Do our own version of freopen() because the standard one closes the
     * original stream BEFORE trying to open the new one.  Once it's gone,
     * there's no way to get the closed stream back if the open fails.
     */

    fflush(oldstream);
    *status = 0;		/* set to one if all this works ... */

    if (logname == NULL) {
	/*
	 * no logfile name, no dice
	 */
	return NULL;
    }

    oldfd = fileno(oldstream);
    if ((dupoldfd = dup(oldfd)) >= 0) {
	/*
	 * try to remove the file first ... don't bother if this fails,
	 * but if it succeeds, we at least get a chance to define the
	 * owner and mode, rather than inheriting this from an existing
	 * writeable file ... really only a problem when called as with
	 * uid == 0, e.g. from pmcd(1).
	 */
	unlink(logname);

	oldstream = outstream = freopen(logname, "w", oldstream);
	if (oldstream == NULL) {
	    int		save_error = oserror();	/* need for error message */

	    close(oldfd);
	    if (dup(dupoldfd) != oldfd) {
		/* fd juggling failed! */
		oldstream = NULL;
	    }
	    else {
		/* oldfd now re-instated as at entry */
		oldstream = fdopen(oldfd, "w");
	    }
	    if (oldstream == NULL) {
		/* serious trouble ... choose least obnoxious alternative */
		if (dupoldstream == stderr)
		    oldstream = fdopen(fileno(stdout), "w");
		else
		    oldstream = fdopen(fileno(stderr), "w");
	    }
	    outstream = oldstream;
	    if (oldstream != NULL) {
		/*
		 * oldstream was NULL, but recovered so now fixup
		 * input oldstream ... this is potentially dangerous,
		 * but we're relying on
		 * (a) fflush(oldstream) on entry flushes buffers
		 * (b) fdopen() leaves new oldstream initialized
		 * (c) caller knows nothing about "new" oldstream
		 *     and is never going to fclose() it, so only
		 *     fclose() will come at exit() and should be
		 *     benign (except possibly for a free() of an
		 *     already free()'d buffer)
		 */
		*dupoldstream = *oldstream;	/* struct copy */
		/* put oldstream back for return value */
		outstream = dupoldstream;
	    }
#ifdef HAVE_STRERROR_R_PTR
	    {
		char	*p;
		p = strerror_r(save_error, errmsg, sizeof(errmsg));
		if (p != errmsg)
		    strncpy(errmsg, p, sizeof(errmsg));
		errmsg[sizeof(errmsg)-1] = '\0';
	    }
#else
	    /*
	     * the more normal POSIX and XSI compliant variants always
	     * fill the message buffer
	     */
	    strerror_r(save_error, errmsg, sizeof(errmsg));
#endif
	    pmprintf("%s: cannot open log \"%s\" for writing : %s\n",
		    progname, logname, errmsg);
	    pmflush();
	}
	else {
	    /* yippee */
	    *status = 1;
	}
	close(dupoldfd);
    }
    else {
	pmprintf("%s: cannot redirect log output to \"%s\": %s\n",
		progname, logname, osstrerror_r(errmsg, sizeof(errmsg)));
	pmflush();
    }
    return outstream;
}

FILE *
pmOpenLog(const char *progname, const char *logname, FILE *oldstream,
	    int *status)
{
    if (logname && strncmp(logname, "-", 2) == 0) {
	/*
	 * Special case to just write to existing stream, usually stderr.
	 * Just keep oldstream, no need to reopen or dup anything.
	 */
    	*status = 1;
    }
    else {
    	/* reopen oldstream as "logname" and set status */
	oldstream = logreopen(progname, logname, oldstream, status);
    }

    /*
     * write the preamble, and append oldstream to the list used by the
     * exit handler to write the footer for all open logs
     */
    logheader(progname, oldstream, "started");

    PM_LOCK(util_lock);
    nfilelog++;
    if (nfilelog == 1)
	atexit(logonexit);

    filelog = (FILE **)realloc(filelog, nfilelog * sizeof(FILE *));
    if (filelog == NULL) {
	PM_UNLOCK(util_lock);
	pmNoMem("pmOpenLog", nfilelog * sizeof(FILE *), PM_FATAL_ERR);
	/* NOTREACHED */
    }
    filelog[nfilelog-1] = oldstream;

    PM_UNLOCK(util_lock);
    return oldstream;
}

FILE *
__pmRotateLog(const char *progname, const char *logname, FILE *oldstream,
	    int *status)
{
    int		i;
    FILE	*newstream = oldstream;

    if (logname && strncmp(logname, "-", 2) == 0) {
	/*
	 * Special case, as for pmOpenLog(), see above.
	 * No log rotation is done in this case.
	 */
	*status = 1;
    	return oldstream;
    }

    PM_LOCK(util_lock);
    for (i = 0; i < nfilelog; i++) {
	if (oldstream == filelog[i])
	    break;
    }
    if (i < nfilelog) {
	PM_UNLOCK(util_lock);
	logfooter(oldstream, "rotated");	/* old */
	newstream = logreopen(progname, logname, oldstream, status);
	logheader(progname, newstream, "rotated");	/* new */
	PM_LOCK(util_lock);
	for (i = 0; i < nfilelog; i++) {
	    if (oldstream == filelog[i]) {
		filelog[i] = newstream;
		break;
	    }
	}
    }
    PM_UNLOCK(util_lock);
    return newstream;
}

/* pmID -> string, max length is 20 bytes */
char *
pmIDStr_r(pmID pmid, char *buf, int buflen)
{
    if (pmid == PM_ID_NULL)
	pmsprintf(buf, buflen, "%s", "PM_ID_NULL");
    else if (IS_DYNAMIC_ROOT(pmid))
	/*
	 * this PMID represents the base of a dynamic subtree in the PMNS
	 * ... identified by setting the domain field to the reserved
	 * value DYNAMIC_PMID and storing the real domain of the PMDA
	 * that can enumerate the subtree in the cluster field, while
	 * the item field is zero
	 */
	pmsprintf(buf, buflen, "%d.*.*", pmID_cluster(pmid));
    else
	pmsprintf(buf, buflen, "%d.%d.%d", pmID_domain(pmid), pmID_cluster(pmid), pmID_item(pmid));
    return buf;
}

const char *
pmIDStr(pmID pmid)
{
    static char	idbuf[20];
    pmIDStr_r(pmid, idbuf, sizeof(idbuf));
    return idbuf;
}

/* pmInDom -> string, max length is 20 bytes */
char *
pmInDomStr_r(pmInDom indom, char *buf, int buflen)
{
    __pmInDom_int*	p = (__pmInDom_int*)&indom;
    if (indom == PM_INDOM_NULL)
	pmsprintf(buf, buflen, "%s", "PM_INDOM_NULL");
    else
	pmsprintf(buf, buflen, "%d.%d", p->domain, p->serial);
    return buf;
}

const char *
pmInDomStr(pmInDom indom)
{
    static char	indombuf[20];
    pmInDomStr_r(indom, indombuf, sizeof(indombuf));
    return indombuf;
}

/* double -> string, max length is 8 bytes */
char *
pmNumberStr_r(double value, char *buf, int buflen)
{
    if (value >= 0.0) {
	if (value >= 999995000000000.0)
	    pmsprintf(buf, buflen, " inf?  ");
	else if (value >= 999995000000.0)
	    pmsprintf(buf, buflen, "%6.2fT", (double)((long double)value / (long double)1000000000000.0));
	else if (value >= 999995000.0)
	    pmsprintf(buf, buflen, "%6.2fG", (double)((long double)value / (long double)1000000000.0));
	else if (value >= 999995.0)
	    pmsprintf(buf, buflen, "%6.2fM", (double)((long double)value / (long double)1000000.0));
	else if (value >= 999.995)
	    pmsprintf(buf, buflen, "%6.2fK", value / 1000.0);
	else if (value >= 0.005)
	    pmsprintf(buf, buflen, "%6.2f ", value);
	else
	    pmsprintf(buf, buflen, "%6.2f ", 0.0);
    }
    else {
	if (value <= -99995000000000.0)
	    pmsprintf(buf, buflen, "-inf?  ");
	else if (value <= -99995000000.0)
	    pmsprintf(buf, buflen, "%6.2fT", (double)((long double)value / (long double)1000000000000.0));
	else if (value <= -99995000.0)
	    pmsprintf(buf, buflen, "%6.2fG", (double)((long double)value / (long double)1000000000.0));
	else if (value <= -99995.0)
	    pmsprintf(buf, buflen, "%6.2fM", (double)((long double)value / (long double)1000000.0));
	else if (value <= -99.995)
	    pmsprintf(buf, buflen, "%6.2fK", value / 1000.0);
	else if (value <= -0.005)
	    pmsprintf(buf, buflen, "%6.2f ", value);
	else
	    pmsprintf(buf, buflen, "%6.2f ", 0.0);
    }
    return buf;
}

const char *
pmNumberStr(double value)
{
    static char nbuf[8];
    pmNumberStr_r(value, nbuf, sizeof(nbuf));
    return nbuf;
}

/* flags -> string, max length is 64 bytes */
char *
pmEventFlagsStr_r(int flags, char *buf, int buflen)
{
    /*
     * buffer needs to be long enough to hold each flag name
     * (excluding missed) plus the separation commas, so
     * point,start,end,id,parent (even though it is unlikely that
     * both start and end would be set for the one event record)
     */
    int started = 0;

    if (buflen < 26)
	return NULL;

    if (flags & PM_EVENT_FLAG_MISSED)
	return strcpy(buf, "missed");

    buf[0] = '\0';
    if (flags & PM_EVENT_FLAG_POINT) {
	started++;
	strcat(buf, "point");
    }
    if (flags & PM_EVENT_FLAG_START) {
	if (started++) strcat(buf, ",");
	strcat(buf, "start");
    }
    if (flags & PM_EVENT_FLAG_END) {
	if (started++) strcat(buf, ",");
	strcat(buf, "end");
    }
    if (flags & PM_EVENT_FLAG_ID) {
	if (started++) strcat(buf, ",");
	strcat(buf, "id");
    }
    if (flags & PM_EVENT_FLAG_PARENT) {
	if (started++) strcat(buf, ",");
	strcat(buf, "parent");
    }
    return buf;
}

const char *
pmEventFlagsStr(int flags)
{
    static char ebuf[64];
    pmEventFlagsStr_r(flags, ebuf, sizeof(ebuf));
    return ebuf;
}

const char *
pmSemStr(int sem)
{
    switch (sem) {
	case PM_SEM_COUNTER:
	    return "counter";
	case PM_SEM_INSTANT:
	    return "instant";
	case PM_SEM_DISCRETE:
	    return "discrete";
	default:
	    break;
    }
    return "???";
}

char *
pmSemStr_r(int sem, char *buf, int buflen)
{
    pmsprintf(buf, buflen, "%s", pmSemStr(sem));
    return buf;
}


/*
 * Several PMAPI interfaces allocate a list of strings into a buffer
 * pointed to by (char **) which can be safely freed simply by
 * freeing the pointer to the buffer.
 *
 * Here we provide some functions for manipulating these lists.
 */

/* Add the given item to the list, which may be empty. */
int
__pmStringListAdd(char *item, int numElements, char ***list)
{
    size_t	ptrSize;
    size_t	dataSize;
    size_t	newSize;
    char	*initialString;
    char	*finalString;
    char	**newList;
    int		i;

    /* Compute the sizes of the pointers and data for the current list. */
    if (*list != NULL) {
	ptrSize = numElements * sizeof(**list);
	initialString = **list;
	finalString = (*list)[numElements - 1];
	dataSize = (finalString + strlen(finalString) + 1) - initialString;
    }
    else {
	ptrSize = 0;
	dataSize = 0;
    }

    /*
     * Now allocate a new buffer for the expanded list.
     * We need room for a new pointer and for the new item.
     */
    newSize = ptrSize + sizeof(**list) + dataSize + strlen(item) + 1;
    newList = realloc(*list, newSize);
    if (newList == NULL) {
	pmNoMem("__pmStringListAdd", newSize, PM_FATAL_ERR);
	/* NOTREACHED */
    }

    /*
     * Shift the existing data to make room for the new pointer and
     * recompute each existing pointer.
     */
    finalString = (char *)(newList + numElements + 1);
    if (dataSize != 0) {
	initialString = (char *)(newList + numElements);
	memmove(finalString, initialString, dataSize);
	for (i = 0; i < numElements; ++i) {
	    newList[i] = finalString;
	    finalString += strlen(finalString) + 1;
	}
    }

    /* Now add the new item. */
    newList[numElements] = finalString;
    strcpy(finalString, item);

    *list = newList;
    return numElements + 1;
}

/* Search for the given string in the given string list. */
char *
__pmStringListFind(const char *item, int numElements, char **list)
{
    int e;

    if (list == NULL)
	return NULL; /* no list to search */

    for (e = 0; e < numElements; ++e) {
	if (strcmp(item, list[e]) == 0)
	    return list[e];
    }

    /* Not found. */
    return NULL;
}

/*
 * Save/restore global debugging flag, without locking.
 * Needed since tracing PDUs really messes __pmDump*() routines
 * up when pmNameInDom is called internally.
 */
static void
save_debug(void)
{
    int		i;

    for (i = 0; i < num_debug; i++) {
	debug_map[i].state = *(debug_map[i].options);
	*(debug_map[i].options) = 0;
    }
    pmDebug = 0;
}

static void
restore_debug(void)
{
    int		i;

    for (i = 0; i < num_debug; i++) {
	*(debug_map[i].options) = debug_map[i].state;
	if (debug_map[i].state && debug_map[i].bit != 0)
	    pmDebug |= debug_map[i].bit;
    }
}

static void
dump_valueset(__pmContext *ctxp, FILE *f, pmValueSet *vsp)
{
    pmDesc	desc;
    char	errmsg[PM_MAXERRMSGLEN];
    char	strbuf[20];
    char	*pmid;
    char	*p;
    char	**names;
    int		have_desc = 1;
    int		n, j;

    pmid = pmIDStr_r(vsp->pmid, strbuf, sizeof(strbuf));
    if ((n = pmNameAll_ctx(ctxp, vsp->pmid, &names)) < 0)
	fprintf(f, "  %s (%s):", pmid, "<noname>");
    else {
	fprintf(f, "  %s (", pmid);
	for (j = 0; j < n; j++) {
	    if (j > 0)
		fprintf(f, ", ");
	    fprintf(f, "%s", names[j]);
	}
	fprintf(f, "):");
	free(names);
    }
    if (vsp->numval == 0) {
	fprintf(f, " No values returned!\n");
	return;
    }
    if (vsp->numval < 0) {
	fprintf(f, " %s\n", pmErrStr_r(vsp->numval, errmsg, sizeof(errmsg)));
	return;
    }
    if (__pmGetInternalState() == PM_STATE_PMCS ||
	pmLookupDesc_ctx(ctxp, PM_NOT_LOCKED, vsp->pmid, &desc) < 0) {
	/* don't know, so punt on the most common cases */
	desc.indom = PM_INDOM_NULL;
	have_desc = 0;
    }

    fprintf(f, " numval: %d", vsp->numval);
    fprintf(f, " valfmt: %d vlist[]:\n", vsp->valfmt);
    for (j = 0; j < vsp->numval; j++) {
	pmValue	*vp = &vsp->vlist[j];
	if (vsp->numval > 1 || vp->inst != PM_INDOM_NULL) {
	    fprintf(f, "    inst [%d", vp->inst);
	    if (have_desc &&
		pmNameInDom_ctx(ctxp, desc.indom, vp->inst, &p) >= 0) {
		fprintf(f, " or \"%s\"]", p);
		free(p);
	    }
	    else {
		fprintf(f, " or ???]");
	    }
	    fputc(' ', f);
	}
	else
	    fprintf(f, "   ");
	fprintf(f, "value ");

	if (have_desc)
	    pmPrintValue(f, vsp->valfmt, desc.type, vp, 1);
	else {
	    if (vsp->valfmt == PM_VAL_INSITU)
		pmPrintValue(f, vsp->valfmt, PM_TYPE_UNKNOWN, vp, 1);
	    else if (vsp->valfmt == PM_VAL_DPTR || vsp->valfmt == PM_VAL_SPTR)
		pmPrintValue(f, vsp->valfmt, (int)vp->value.pval->vtype, vp, 1);
	    else
		fprintf(f, "bad valfmt %d", vsp->valfmt);
	}
	fputc('\n', f);
    }
}

/* Internal variant of __pmDumpResult() with current context. */
void
__pmDumpResult_ctx(__pmContext *ctxp, FILE *f, const pmResult *resp)
{
    int		i;

    if (ctxp != NULL)
	PM_ASSERT_IS_LOCKED(ctxp->c_lock);

    save_debug();
    fprintf(f, "pmResult dump from " PRINTF_P_PFX "%p timestamp: %d.%06d ",
	resp, (int)resp->timestamp.tv_sec, (int)resp->timestamp.tv_usec);
    pmPrintStamp(f, &resp->timestamp);
    fprintf(f, " numpmid: %d\n", resp->numpmid);
    for (i = 0; i < resp->numpmid; i++)
	dump_valueset(ctxp, f, resp->vset[i]);
    restore_debug();
}

void
__pmDumpResult(FILE *f, const pmResult *resp)
{
    __pmDumpResult_ctx(NULL, f, resp);
}

/* Internal variant of __pmPrintResult() with current context. */
void
__pmPrintResult_ctx(__pmContext *ctxp, FILE *f, const __pmResult *resp)
{
    int		i;

    if (ctxp != NULL)
	PM_ASSERT_IS_LOCKED(ctxp->c_lock);

    save_debug();
    fprintf(f, "__pmResult dump from " PRINTF_P_PFX "%p timestamp: %" FMT_INT64 ".%09d ",
	resp, resp->timestamp.sec, resp->timestamp.nsec);
    __pmPrintTimestamp(f, &resp->timestamp);
    fprintf(f, " numpmid: %d\n", resp->numpmid);
    for (i = 0; i < resp->numpmid; i++)
	dump_valueset(ctxp, f, resp->vset[i]);
    restore_debug();
}

void
__pmPrintResult(FILE *f, const __pmResult *resp)
{
    __pmPrintResult_ctx(NULL, f, resp);
}

/* Internal variant of __pmDumpHighResResult() with current context. */
void
__pmDumpHighResResult_ctx(__pmContext *ctxp, FILE *f, const pmHighResResult *hresp)
{
    int		i;

    if (ctxp != NULL)
	PM_ASSERT_IS_LOCKED(ctxp->c_lock);

    save_debug();
    fprintf(f, "%s dump from " PRINTF_P_PFX "%p timestamp: %lld.%09lld ",
	    "pmHighResResult", hresp,
	    (long long)hresp->timestamp.tv_sec,
	    (long long)hresp->timestamp.tv_nsec);
    pmPrintHighResStamp(f, &hresp->timestamp);
    fprintf(f, " numpmid: %d\n", hresp->numpmid);
    for (i = 0; i < hresp->numpmid; i++)
	dump_valueset(ctxp, f, hresp->vset[i]);
    restore_debug();
}

void
__pmDumpHighResResult(FILE *f, const pmHighResResult *hresp)
{
    __pmDumpHighResResult_ctx(NULL, f, hresp);
}

static void
print_event_summary(FILE *f, const pmValue *val, int highres)
{
    struct timespec	tsstamp;
    struct timeval	tvstamp;
    pmTimespec 	*tsp;
    pmTimeval 	*tvp;
    unsigned int	flags;
    size_t		size;
    char		*base;
    int			nparams;
    int			nrecords;
    int			nmissed = 0;
    int			r;	/* records */
    int			p;	/* parameters in a record ... */

    if (highres) {
	pmHighResEventArray *hreap = (pmHighResEventArray *)val->value.pval;
	nrecords = hreap->ea_nrecords;
	base = (char *)&hreap->ea_record[0];
	tsp = (pmTimespec *)base;
	tsstamp.tv_sec = tsp->tv_sec;
	tsstamp.tv_nsec = tsp->tv_nsec;
    }
    else {
	pmEventArray *eap = (pmEventArray *)val->value.pval;
	nrecords = eap->ea_nrecords;
	base = (char *)&eap->ea_record[0];
	tvp = (pmTimeval *)base;
	tvstamp.tv_sec = tvp->tv_sec;
	tvstamp.tv_usec = tvp->tv_usec;
    }

    /* walk packed event record array */
    for (r = 0; r < nrecords-1; r++) {
	if (highres) {
	    pmHighResEventRecord *hrerp = (pmHighResEventRecord *)base;
	    size = sizeof(hrerp->er_timestamp) + sizeof(hrerp->er_flags) +
		    sizeof(hrerp->er_nparams);
	    flags = hrerp->er_flags;
	    nparams = hrerp->er_nparams;
	}
	else {
	    pmEventRecord *erp = (pmEventRecord *)base;
	    size = sizeof(erp->er_timestamp) + sizeof(erp->er_flags) +
		    sizeof(erp->er_nparams);
	    flags = erp->er_flags;
	    nparams = erp->er_nparams;
	}

	if (flags & PM_EVENT_FLAG_MISSED) {
	    nmissed += nparams;
	    continue;
	}

	base += size;
	for (p = 0; p < nparams; p++) {
	    pmEventParameter *epp = (pmEventParameter *)base;
	    base += sizeof(epp->ep_pmid) + PM_PDU_SIZE_BYTES(epp->ep_len);
	}
    }
    fprintf(f, "[%d event record", nrecords);
    if (nrecords != 1)
	fputc('s', f);
    if (nmissed > 0)
	fprintf(f, " (%d missed)", nmissed);
    if (nrecords > 0) {
	fprintf(f, " timestamp");
	if (nrecords > 1)
	    fputc('s', f);
	fputc(' ', f);

	if (highres)
	    pmPrintHighResStamp(f, &tsstamp);
	else
	    pmPrintStamp(f, &tvstamp);

	if (nrecords > 1) {
	    fprintf(f, "...");
	    if (highres) {
		tsp = (pmTimespec *)base;
		tsstamp.tv_sec = tsp->tv_sec;
		tsstamp.tv_nsec = tsp->tv_nsec;
		pmPrintHighResStamp(f, &tsstamp);
	    }
	    else {
		tvp = (pmTimeval *)base;
		tvstamp.tv_sec = tvp->tv_sec;
		tvstamp.tv_usec = tvp->tv_usec;
		pmPrintStamp(f, &tvstamp);
	    }
	}
    }
    fputc(']', f);
}

/* Print single pmValue. */
void
pmPrintValue(FILE *f,			/* output stream */
             int valfmt,		/* from pmValueSet */
             int type,			/* from pmDesc */
             const pmValue *val,	/* value to print */
	     int minwidth)		/* output is at least this wide */
{
    pmAtomValue a;
    int         i;
    int         n;
    char        *p;
    int		sts;

    if (type != PM_TYPE_UNKNOWN &&
	type != PM_TYPE_EVENT &&
	type != PM_TYPE_HIGHRES_EVENT) {
	sts = pmExtractValue(valfmt, val, type, &a, type);
	if (sts < 0)
	    type = PM_TYPE_UNKNOWN;
    }

    switch (type) {
    case PM_TYPE_32:
        fprintf(f, "%*i", minwidth, a.l);
        break;

    case PM_TYPE_U32:
        fprintf(f, "%*u", minwidth, a.ul);
        break;

    case PM_TYPE_64:
        fprintf(f, "%*"PRIi64, minwidth, a.ll);
        break;

    case PM_TYPE_U64:
        fprintf(f, "%*"PRIu64, minwidth, a.ull);
        break;

    case PM_TYPE_FLOAT:
        fprintf(f, "%*.8g", minwidth, (double)a.f);
        break;

    case PM_TYPE_DOUBLE:
        fprintf(f, "%*.16g", minwidth, a.d);
        break;

    case PM_TYPE_STRING:
	n = (int)strlen(a.cp) + 2;
	while (n < minwidth) {
	    fputc(' ', f);
	    n++;
	}
        fprintf(f, "\"%s\"", a.cp);
	free(a.cp);
        break;

    case PM_TYPE_AGGREGATE:
    case PM_TYPE_UNKNOWN:
	if (valfmt == PM_VAL_INSITU) {
	    float	*fp = (float *)&val->value.lval;
	    __uint32_t	*ip = (__uint32_t *)&val->value.lval;
	    int		fp_bad = 0;
	    fprintf(f, "%*u", minwidth, *ip);
#ifdef HAVE_FPCLASSIFY
	    fp_bad = fpclassify(*fp) == FP_NAN;
#else
#ifdef HAVE_ISNANF
	    fp_bad = isnanf(*fp);
#endif
#endif
	    if (!fp_bad)
		fprintf(f, " %*.8g", minwidth, (double)*fp);
	    if (minwidth > 2)
		minwidth -= 2;
	    fprintf(f, " 0x%*x", minwidth, val->value.lval);
	}
	else if (valfmt == PM_VAL_DPTR || valfmt == PM_VAL_SPTR) {
	    int		string;
	    int		done = 0;
	    if (val->value.pval->vlen == PM_VAL_HDR_SIZE + sizeof(__uint64_t)) {
		__uint64_t	tmp;
		memcpy((void *)&tmp, (void *)val->value.pval->vbuf, sizeof(tmp));
		fprintf(f, "%*"PRIu64, minwidth, tmp);
		done = 1;
	    }
	    if (val->value.pval->vlen == PM_VAL_HDR_SIZE + sizeof(double)) {
		double		tmp;
		int		fp_bad = 0;
		memcpy((void *)&tmp, (void *)val->value.pval->vbuf, sizeof(tmp));
#ifdef HAVE_FPCLASSIFY
		fp_bad = fpclassify(tmp) == FP_NAN;
#else
#ifdef HAVE_ISNAN
		fp_bad = isnan(tmp);
#endif
#endif
		if (!fp_bad) {
		    if (done) fputc(' ', f);
		    fprintf(f, "%*.16g", minwidth, tmp);
		    done = 1;
		}
	    }
	    if (val->value.pval->vlen == PM_VAL_HDR_SIZE + sizeof(float)) {
		float	tmp;
		int	fp_bad = 0;
		memcpy((void *)&tmp, (void *)val->value.pval->vbuf, sizeof(tmp));
#ifdef HAVE_FPCLASSIFY
		fp_bad = fpclassify(tmp) == FP_NAN;
#else
#ifdef HAVE_ISNANF
		fp_bad = isnanf(tmp);
#endif
#endif
		if (!fp_bad) {
		    if (done) fputc(' ', f);
		    fprintf(f, "%*.8g", minwidth, (double)tmp);
		    done = 1;
		}
	    }
	    if (val->value.pval->vlen < PM_VAL_HDR_SIZE)
		fprintf(f, "pmPrintValue: negative length (%d) for aggregate value?\n",
		    (int)val->value.pval->vlen - PM_VAL_HDR_SIZE);
	    else {
		string = 1;
		for (n = 0; n < val->value.pval->vlen - PM_VAL_HDR_SIZE; n++) {
		    if (!isprint((int)val->value.pval->vbuf[n])) {
			string = 0;
			break;
		    }
		}
		if (string) {
		    if (done) fputc(' ', f);
		    n = (int)val->value.pval->vlen - PM_VAL_HDR_SIZE + 2;
		    while (n < minwidth) {
			fputc(' ', f);
			n++;
		    }
		    n = (int)val->value.pval->vlen - PM_VAL_HDR_SIZE;
		    fprintf(f, "\"%*.*s\"", n, n, val->value.pval->vbuf);
		    done = 1;
		}
		n = 2 * (val->value.pval->vlen - PM_VAL_HDR_SIZE) + 2;
		while (n < minwidth) {
		    fputc(' ', f);
		    n++;
		}
		if (done) fputc(' ', f);
		fputc('[', f);
		p = &val->value.pval->vbuf[0];
		for (i = 0; i < val->value.pval->vlen - PM_VAL_HDR_SIZE; i++) {
		    fprintf(f, "%02x", *p & 0xff);
		    p++;
		}
		fputc(']', f);
	    }
	}
	else {
	    fprintf(f, "pmPrintValue: bad valfmt (%d)?\n", valfmt);
	}
	if (type != PM_TYPE_UNKNOWN)
	    free(a.vbp);
	break;

    case PM_TYPE_EVENT:		/* not much we can do about minwidth */
    case PM_TYPE_HIGHRES_EVENT:
	if (valfmt == PM_VAL_INSITU) {
	    /*
	     * Special case for pmlc/pmlogger where PMLC_SET_*() macros
	     * used to set control requests / state in the lval field
	     * and the pval does not really contain a valid event record
	     * Code here comes from PrintState() in actions.c from pmlc.
	     */
	    fputs("[pmlc control ", f);
	    fputs(PMLC_GET_MAND(val->value.lval) ? "mand " : "adv ", f);
	    fputs(PMLC_GET_ON(val->value.lval) ? "on " : "off ", f);
	    if (PMLC_GET_INLOG(val->value.lval))
		fputs(PMLC_GET_AVAIL(val->value.lval) ? " " : "na ", f);
	    else
		fputs("nl ", f);
	    fprintf(f, "%d]", PMLC_GET_DELTA(val->value.lval));
	}
	else
	    print_event_summary(f, val, type != PM_TYPE_EVENT);
	break;

    case PM_TYPE_NOSUPPORT:
        fprintf(f, "pmPrintValue: bogus value, metric Not Supported\n");
	break;

    default:
        fprintf(f, "pmPrintValue: unknown value type=%d\n", type);
    }
}

void
pmNoMem(const char *where, size_t size, int fatal)
{
    char	errmsg[PM_MAXERRMSGLEN];
    pmNotifyErr(fatal ? LOG_ERR : LOG_WARNING, "%s: malloc(%zu) failed: %s",
			where, size, osstrerror_r(errmsg, sizeof(errmsg)));
    if (fatal == PM_FATAL_ERR)
	exit(1);
}

/*
 * this one is used just below the PMAPI to convert platform errors
 * into more appropriate PMAPI error codes
 */
int
__pmMapErrno(int sts)
{
    if (sts == -EBADF || sts == -EPIPE)
	sts = PM_ERR_IPC;
#ifdef IS_MINGW
    else if (sts == -EINVAL)
	sts = PM_ERR_IPC;
#endif
    return sts;
}

int
__pmGetTimespec(struct timespec *ts)
{
#if defined(IS_DARWIN)
    clock_serv_t cclock;
    mach_timespec_t mts;

    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
    return 0;
#elif defined(HAVE_CLOCK_GETTIME)
    return clock_gettime(CLOCK_REALTIME, ts);
#else
#warning "No high resolution timestamp support on this platform"
    struct timeval tv;
    int sts;

    if ((sts = gettimeofday(&tv, NULL)) == 0) {
	ts->tv_sec = tv.tv_sec;
	ts->tv_nsec = tv.tv_usec * 1000;
    }
    return sts;
#endif
}

int
pmtimespecNow(struct timespec *ts)
{
    return __pmGetTimespec(ts);
}

int
__pmGetTimestamp(__pmTimestamp *timestamp)
{
    struct timespec ts;
    int sts;

    if ((sts = pmtimespecNow(&ts)) < 0)
	return sts;
    timestamp->sec = ts.tv_sec;
    timestamp->nsec = ts.tv_nsec;
    return sts;
}

/*
 * a : b for pmTimeval ... <0 for a<b, ==0 for a==b, >0 for a>b
 */
int
__pmTimevalCmp(const pmTimeval *a, const pmTimeval *b)
{
    int res = (int)(a->tv_sec - b->tv_sec);

    if (res == 0)
	res = (int)(a->tv_usec - b->tv_usec);

    return res;
}

/*
 * a : b for __pmTimestamp ... <0 for a<b, ==0 for a==b, >0 for a>b
 */
int
__pmTimestampCmp(const __pmTimestamp *a, const __pmTimestamp *b)
{
    int res = (int)(a->sec - b->sec);

    if (res == 0)
	res = (int)(a->nsec - b->nsec);

    return res;
}

/*
 * Difference for two of the internal timestamps ...
 * Same as pmtimevalSub() in tv.c, just with pmTimeval args
 * rather than struct timeval args.
 */
double
__pmTimevalSub(const pmTimeval *ap, const pmTimeval *bp)
{
     return (double)(ap->tv_sec - bp->tv_sec + (long double)(ap->tv_usec - bp->tv_usec) / (long double)1000000);
}

/*
 * Difference for two of the internal highres timestamps ...
 * Same as pmtimespecSub() in tv.c, just with pmTimespec args
 * rather than struct timespec args.
 */
double
__pmTimespecSub(const pmTimespec *ap, const pmTimespec *bp)
{
     return (double)(ap->tv_sec - bp->tv_sec + (long double)(ap->tv_nsec - bp->tv_nsec) / (long double)1000000000);
}

/*
 * Difference for two of the universal timestamps ...
 * Same as pmtimespecSub() in tv.c, just with __pmTimestamp args
 * rather than struct timespec args.
 */
double
__pmTimestampSub(const __pmTimestamp *ap, const __pmTimestamp *bp)
{
     return (double)(ap->sec - bp->sec + (long double)(ap->nsec - bp->nsec) / (long double)1000000000);
}

/*
 * Increment a universal timestamps ...
 */
void
__pmTimestampInc(__pmTimestamp *ap, const __pmTimestamp *bp)
{
     ap->sec += bp->sec;
     ap->nsec += bp->nsec;
     if (ap->nsec > 1000000000) {
	 ap->sec++;
	 ap->nsec -= 1000000000;
    }
}

/*
 * Decrement a universal timestamps ...
 */
void
__pmTimestampDec(__pmTimestamp *ap, const __pmTimestamp *bp)
{
     ap->sec -= bp->sec;
     ap->nsec -= bp->nsec;
     if (ap->nsec < 0) {
	 ap->sec--;
	 ap->nsec += 1000000000;
    }
}

/*
 * print timeval timestamp in HH:MM:SS.XXX format
 * Note: to minimize ABI surprises, this one still reports to msec precision,
 *       but the internal and diagnostic variant __pmPrintTimeval() reports
 *       to usec precision.
 */
void
pmPrintStamp(FILE *f, const struct timeval *tp)
{
    struct tm	tmp;
    time_t	now;

    now = (time_t)tp->tv_sec;
    pmLocaltime(&now, &tmp);
    fprintf(f, "%02d:%02d:%02d.%03d", tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)(tp->tv_usec/1000));
}

/*
 * print high resolution timestamp in HH:MM:SS.XXXXXXXXX format
 */
void
pmPrintHighResStamp(FILE *f, const struct timespec *tp)
{
    struct tm	tmp;
    time_t	now;

    now = (time_t)tp->tv_sec;
    pmLocaltime(&now, &tmp);
    fprintf(f, "%02d:%02d:%02d.%09d", tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)(tp->tv_nsec));
}

/*
 * print pmTimeval timestamp in HH:MM:SS.XXXXXX format (usec precision)
 * (pmTimeval variant used in historical PDUs, archives and internally)
 */
void
__pmPrintTimeval(FILE *f, const pmTimeval *tp)
{
    struct tm	tmp;
    time_t	now;

    now = (time_t)tp->tv_sec;
    pmLocaltime(&now, &tmp);
    fprintf(f, "%02d:%02d:%02d.%06d", tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)tp->tv_usec);
}

/*
 * print pmTimespec timestamp in HH:MM:SS.XXXXXXXXX format (nsec precision)
 * (pmTimespec variant used in the latest PDUs, archives and internally)
 */
void
__pmPrintTimespec(FILE *f, const pmTimespec *tp)
{
    struct tm	tmp;
    time_t	now;

    now = (time_t)tp->tv_sec;
    pmLocaltime(&now, &tmp);
    fprintf(f, "%02d:%02d:%02d.%09d", tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)tp->tv_nsec);
}

/*
 * print universal __pmTimestamp timestamp in HH:MM:SS.XXXXXXXXX format
 */
void
__pmPrintTimestamp(FILE *f, const __pmTimestamp *tsp)
{
    struct tm	tmp;
    time_t	now;

    now = (time_t)tsp->sec;
    pmLocaltime(&now, &tmp);
    fprintf(f, "%02d:%02d:%02d.%09d", tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)tsp->nsec);
}

/*
 * must be in agreement with ordinal values for PM_TYPE_* #defines
 */
/* PM_TYPE_* -> string, max length is 20 bytes */
const char *
pmTypeStr(int type)
{
    static const char *typename[] = {
	"32", "U32", "64", "U64", "FLOAT", "DOUBLE", "STRING",
	"AGGREGATE", "AGGREGATE_STATIC", "EVENT", "HIGHRES_EVENT"
    };

    if (type >= 0 && type < sizeof(typename) / sizeof(typename[0]))
	return typename[type];
    else if (type == PM_TYPE_NOSUPPORT)
	return "NO_SUPPORT";
    else if (type == PM_TYPE_UNKNOWN)
	return "UNKNOWN";
    return "???";
}

/*
 * thread-safe version of __pmLogMetaTypeStr() ... buflen
 * should be at least 17
 */
char *
pmTypeStr_r(int type, char *buf, int buflen)
{
    pmsprintf(buf, buflen, "%s", pmTypeStr(type));
    return buf;
}

/*
 * Return the name of metadata record type
 */
const char *
__pmLogMetaTypeStr(int type)
{
    static char		*typename[] = {
	"???", "DESC", "INDOM_V2", "LABEL_V2", "TEXT", "INDOM",
	"INDOM_DELTA", "LABEL"
    };

    if (type >= 0 && type < sizeof(typename) / sizeof(typename[0]))
	return typename[type];
    return "???";
}

/*
 * thread-safe version of __pmLogMetaTypeStr() ... buflen
 * should be at least 15
 */
char *
__pmLogMetaTypeStr_r(int type, char *buf, int buflen)
{
    pmsprintf(buf, buflen, "%s", __pmLogMetaTypeStr(type));
    return buf;
}

/*
 * descriptor
 */
void
pmPrintDesc(FILE *f, const pmDesc *desc)
{
    const char		*type;
    const char		*sem;
    const char		*units;
    char		strbuf[60];

    if (desc->type == PM_TYPE_NOSUPPORT) {
	fprintf(f, "    Data Type: Not Supported\n");
	return;
    }

    switch (desc->type) {
	case PM_TYPE_32:
	    type = "32-bit int";
	    break;
	case PM_TYPE_U32:
	    type = "32-bit unsigned int";
	    break;
	case PM_TYPE_64:
	    type = "64-bit int";
	    break;
	case PM_TYPE_U64:
	    type = "64-bit unsigned int";
	    break;
	case PM_TYPE_FLOAT:
	    type = "float";
	    break;
	case PM_TYPE_DOUBLE:
	    type = "double";
	    break;
	case PM_TYPE_STRING:
	    type = "string";
	    break;
	case PM_TYPE_AGGREGATE:
	    type = "aggregate";
	    break;
	case PM_TYPE_AGGREGATE_STATIC:
	    type = "static aggregate";
	    break;
	case PM_TYPE_EVENT:
	    type = "event record array";
	    break;
	case PM_TYPE_HIGHRES_EVENT:
	    type = "highres event record array";
	    break;
	default:
	    type = "???";
	    break;
    }
    fprintf(f, "    Data Type: %s", type);
    if (strcmp(type, "???") == 0)
	fprintf(f, " (%d)", desc->type);

    fprintf(f, "  InDom: %s 0x%x\n", pmInDomStr_r(desc->indom, strbuf, sizeof(strbuf)), desc->indom);

    sem = pmSemStr_r(desc->sem, strbuf, sizeof(strbuf));
    fprintf(f, "    Semantics: %s", sem);
    if (strcmp(sem, "???") == 0)
	fprintf(f, " (%d)", desc->sem);

    fprintf(f, "  Units: ");
    units = pmUnitsStr_r(&desc->units, strbuf, sizeof(strbuf));
    if (*units == '\0')
	fprintf(f, "none\n");
    else
	fprintf(f, "%s\n", units);
}

#define DEBUG_CLEAR 0
#define DEBUG_SET 1
#define DEBUG_OLD 0
#define DEBUG_NEW 1

static int
debug(const char *spec, int action, int style)
{
    int		val = 0;
    int		tmp;
    const char	*p;
    char	*pend;
    int		i;
    int		sts = 0;	/* for DEBUG_NEW interface */

    /* save old state, so calls are additive */
    for (i = 0; i < num_debug; i++)
	debug_map[i].state = *(debug_map[i].options);

    for (p = spec; *p; ) {
	/*
	 * backwards compatibility, "string" may be a number for setting
	 * bit fields directly
	 */
	tmp = (int)strtol(p, &pend, 10);
	if (pend > p) {
	    /* found a number */
	    if (tmp == -1)
		/* special case ... -1 really means set all the bits! */
		val = INT_MAX;
	    else
		val |= tmp;
	    /* for all matching bits, set/clear the corresponding new field */
	    for (i = 0; i < num_debug; i++) {
		if (val & debug_map[i].bit)
		    debug_map[i].state = (action == DEBUG_SET ? 1 : 0);
	    }
	    if (*pend == ',') {
		p = pend + 1;
		continue;
	    }
	    else if (*pend == '\0')
		break;
	    /* something bogus after a number ... */
	    sts = PM_ERR_CONV;
	    break;
	}

	pend = strchr(p, ',');
	if (pend == NULL)
	    pend = (char *)&p[strlen(p)];

	if (pend-p == 3 && strncasecmp(p, "all", pend-p) == 0) {
	    val |= INT_MAX;
	    for (i = 0; i < num_debug; i++) {
		debug_map[i].state = (action == DEBUG_SET ? 1 : 0);
	    }
	}
	else {
	    for (i = 0; i < num_debug; i++) {
		if (pend-p == strlen(debug_map[i].name) &&
		    strncasecmp(p, debug_map[i].name, pend-p) == 0) {
		    if (debug_map[i].bit != 0)
			/* has corresponding old-stype bit field */
			val |= debug_map[i].bit;
		    debug_map[i].state = (action == DEBUG_SET ? 1 : 0);
		    break;
		}
	    }
	    if (i == num_debug) {
		sts = PM_ERR_CONV;
		break;
	    }
	}
	if (*pend == ',')
	    p = pend + 1;
	else
	    p = pend;
    }

    if (sts == 0) {
	/* all's well, now set the options and bits */
	for (i = 0; i < num_debug; i++)
	    *(debug_map[i].options) = debug_map[i].state;
	/* set/clear old-style bit mask */
	if (action == DEBUG_SET)
	    pmDebug |= val;
	else
	    pmDebug &= ~val;
    }

    if (style == DEBUG_OLD && sts == 0)
	return val;

    return sts;
}

void
__pmDumpDebug(FILE *f)
{
    int		i;
    int		nset;

    nset = 0;
    fprintf(f, "pmDebug:\t");
    if (pmDebug == 0)
	fprintf(f, "Nothing set\n");
    else {
	for (i = 0; i < num_debug; i++) {
	    if (debug_map[i].bit != 0 &&
	        (pmDebug & debug_map[i].bit) != 0) {
		nset++;
		if (nset > 1)
		    fputc(',', f);
		fprintf(f, "%s", debug_map[i].name);
	    }
	}
	fputc('\n', f);
    }

    nset = 0;
    fprintf(f, "pmDebugOptions:\t");
    for (i = 0; i < num_debug; i++) {
	if (*(debug_map[i].options)) {
	    nset++;
	    if (nset > 1)
		fputc(',', f);
	    fprintf(f, "%s", debug_map[i].name);
	}
    }
    if (nset == 0)
	fprintf(f, "Nothing set\n");
    else
	fputc('\n', f);
}

/*
 * old routine for backwards compatibility ...
 *	return 32-bit bit debug flags
 */
int
__pmParseDebug(const char *spec)
{
    if (pmDebugOptions.deprecated)
	fprintf(stderr, "Warning: deprecated __pmParseDebug() called\n");
    return debug(spec, DEBUG_SET, DEBUG_OLD);
}

/* new routine to set debug options */
int
pmSetDebug(const char *spec)
{
    return debug(spec, DEBUG_SET, DEBUG_NEW);
}

/* new routine to clear debug options */
int
pmClearDebug(const char *spec)
{
    return debug(spec, DEBUG_CLEAR, DEBUG_NEW);
}

/*
 * Interface for setting debugging options by bit-field (deprecated) rather
 * than by name (new scheme).
 * This routine is used by PMDAs that have a control metric that maps onto
 * pmDebug, e.g. sample.control or trace.control
 * For symmetry with pmSetDebug() the effects are additive, so a PMDA
 * that used to pmDebug = value now needs to pmClearDebug("all") and then
 * __pmSetDebugBits(value).
 */
void
__pmSetDebugBits(int value)
{
    int		i;

    if (pmDebugOptions.deprecated)
	fprintf(stderr, "Warning: deprecated __pmSetDebugBits() called\n");

    for (i = 0; i < num_debug; i++) {
	if (value & debug_map[i].bit) {
	    /* this option has a bit-field equivalent that is set in value */
	    pmSetDebug(debug_map[i].name);
	}
    }
}

int
__pmGetInternalState(void)
{
    return pmState;
}

void
__pmSetInternalState(int state)
{
    pmState = state;
}


/*
 * GUI output option
 */

#define MSGBUFLEN	256
static char	*msgbuf;	/* message(s) are assembled here */
static int	msgbuflen;	/* allocated length of mbuf[] */
static int	msgsize;	/* size of accumulated message(s) */
static char	*filename;	/* output filename from $PCP_STDERR */

#define PM_QUERYERR       -1
#define PM_USEDIALOG       0
#define PM_USESTDERR       1
#define PM_USEFILE         2

/*
 * called with util_lock held
 */
static int
pmfstate(int state)
{
    static int	errtype = PM_QUERYERR;
    char	errmsg[PM_MAXERRMSGLEN];

    PM_ASSERT_IS_LOCKED(util_lock);

    if (state > PM_QUERYERR)
	errtype = state;

    if (errtype == PM_QUERYERR) {
	/* one-trip initialization */
	errtype = PM_USESTDERR;
	PM_LOCK(__pmLock_extcall);
	filename = getenv("PCP_STDERR");		/* THREADSAFE */
	if (filename != NULL)
	    filename = strdup(filename);
	PM_UNLOCK(__pmLock_extcall);
	if (filename != NULL) {
	    if (strcasecmp(filename, "DISPLAY") == 0) {
		if (!xconfirm)
		    fprintf(stderr, "%s: using stderr - no PCP_XCONFIRM_PROG\n",
			    pmGetProgname());
		else {
		    char *path = strdup(xconfirm);
		    if (path == NULL) {
			pmNoMem("pmfstate", strlen(xconfirm)+1, PM_FATAL_ERR);
			/* NOTREACHED */
		    }
		    /* THREADSAFE - no locks acquired in __pmNativePath() */
		    path = __pmNativePath(path);
		    if (access(path, X_OK) < 0)
			fprintf(stderr, "%s: using stderr - cannot access %s: %s\n",
			    pmGetProgname(), xconfirm, osstrerror_r(errmsg, sizeof(errmsg)));
		    else
			errtype = PM_USEDIALOG;
		    free(path);
		}
	    }
	    else if (strcmp(filename, "") != 0)
		errtype = PM_USEFILE;
	}
    }
    return errtype;
}

static int
vpmprintf(const char *fmt, va_list arg)
{
    int		bytes;
    int		avail;
    va_list	save_arg;

#define MSGCHUNK 4096

    PM_LOCK(util_lock);
    if (msgbuf == NULL) {
	/* initial 4K allocation */
PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_ALLOC);
	msgbuf = malloc(MSGCHUNK);
	if (msgbuf == NULL) {
	    PM_UNLOCK(util_lock);
	    pmNoMem("vmprintf malloc", MSGCHUNK, PM_RECOV_ERR);
	    vfprintf(stderr, fmt, arg);
	    fputc('\n', stderr);
	    fflush(stderr);
	    return -ENOMEM;
	}
	msgbuflen = MSGCHUNK;
	msgsize = 0;
    }

    va_copy(save_arg, arg);
    va_end(save_arg);
    avail = msgbuflen - msgsize - 1;
    for ( ; ; ) {
	char	*msgbuf_tmp;
	if (avail > 0) {
	    bytes = vsnprintf(&msgbuf[msgsize], avail, fmt, arg);
	    if (bytes < avail)
		break;
	    va_copy(arg, save_arg);	/* will need to call vsnprintf() again */
	    va_end(arg);
	}
	msgbuflen += MSGCHUNK;
PM_FAULT_POINT("libpcp/" __FILE__ ":2", PM_FAULT_ALLOC);
	msgbuf_tmp = realloc(msgbuf, msgbuflen);
	if (msgbuf_tmp == NULL) {
	    int		msgbuflen_tmp = msgbuflen;
	    msgbuf[msgbuflen - MSGCHUNK - 1] = '\0';
	    fprintf(stderr, "%s", msgbuf);
	    vfprintf(stderr, fmt, arg);
	    fputc('\n', stderr);
	    fflush(stderr);
	    free(msgbuf);
	    msgbuf = NULL;
	    msgbuflen = 0;
	    msgsize = 0;
	    PM_UNLOCK(util_lock);
	    pmNoMem("vmprintf realloc", msgbuflen_tmp, PM_RECOV_ERR);
	    return -ENOMEM;
	}
	msgbuf = msgbuf_tmp;
	avail = msgbuflen - msgsize - 1;
    }
    msgsize += bytes;

    PM_UNLOCK(util_lock);
    return bytes;
}

int
pmprintf(const char *fmt, ...)
{
    va_list	arg;
    int		bytes;

    va_start(arg, fmt);
    bytes = vpmprintf(fmt, arg);
    va_end(arg);
    return bytes;
}

int
pmflush(void)
{
    int		sts = 0;
    int		state;
    FILE	*eptr = NULL;
    __pmExecCtl_t	*argp = NULL;
    char	errmsg[PM_MAXERRMSGLEN];
    char	*path = NULL;

    /* see thread-safe notes above */
    if (!xconfirm_init) {
	xconfirm = pmGetOptionalConfig("PCP_XCONFIRM_PROG");
	xconfirm_init = 1;
    }

    PM_LOCK(util_lock);
    if (msgbuf != NULL) {
	state = pmfstate(PM_QUERYERR);
	if (state == PM_USEFILE) {
	    if ((eptr = fopen(filename, "a")) == NULL) {
		fprintf(stderr, "pmflush: cannot append to file '%s' (from "
			"$PCP_STDERR): %s\n", filename, osstrerror_r(errmsg, sizeof(errmsg)));
		state = PM_USESTDERR;
	    }
	}
	switch (state) {
	case PM_USESTDERR:
	    fprintf(stderr, "%s", msgbuf);
	    break;
	case PM_USEDIALOG:
	    if (xconfirm == NULL) {
		fprintf(stderr, "%s: no PCP_XCONFIRM_PROG\n", pmGetProgname());
		sts = PM_ERR_GENERIC;
		break;
	    }
	    path = strdup(xconfirm);
	    if (path == NULL) {
		pmNoMem("pmflush", strlen(xconfirm)+1, PM_FATAL_ERR);
		/* NOTREACHED */
	    }
	    /* THREADSAFE - no locks acquired in __pmNativePath() */
	    path = __pmNativePath(path);
	    if ((sts = __pmProcessAddArg(&argp, path)) < 0)
		break;
	    if ((sts = __pmProcessAddArg(&argp, "-t")) < 0)
		break;
	    if ((sts = __pmProcessAddArg(&argp, msgbuf)) < 0)
		break;
	    if ((sts = __pmProcessAddArg(&argp, "-c")) < 0)
		break;
	    if ((sts = __pmProcessAddArg(&argp, "-B")) < 0)
		break;
	    if ((sts = __pmProcessAddArg(&argp, "OK")) < 0)
		break;
	    if ((sts = __pmProcessAddArg(&argp, "-icon")) < 0)
		break;
	    if ((sts = __pmProcessAddArg(&argp, "info")) < 0)
		break;
	    if (msgsize > 80) {
		if ((sts = __pmProcessAddArg(&argp, "-useslider")) < 0)
		    break;
	    }
	    if ((sts = __pmProcessAddArg(&argp, "-header")) < 0)
		break;
	    if ((sts = __pmProcessAddArg(&argp, "PCP Information")) < 0)
		break;
	    /* no thread-safe issue here ... we're executing xconfirm */
	    if ((sts = __pmProcessExec(&argp, PM_EXEC_TOSS_ALL, PM_EXEC_WAIT)) < 0) {
		fprintf(stderr, "%s: __pmProcessExec(%s, ...) failed: %s\n",
		    pmGetProgname(), path,
		    pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    }
	    /*
	     * Note, we don't care about the wait exit status in this case
	     */
	    break;
	case PM_USEFILE:
	    fprintf(eptr, "%s", msgbuf);
	    fclose(eptr);
	    break;
	}
	free(msgbuf);
	msgbuf = NULL;
	msgbuflen = 0;
	msgsize = 0;
    }

    PM_UNLOCK(util_lock);

    if (path != NULL)
	free(path);

    return sts;
}

/*
 * Set the pmcd client identity as exported by pmcd.client.whoami
 *
 * Identity is of the form
 *	hostname (ipaddr) <id>
 *
 * Assumes you already have a current host context.
 */
int
__pmSetClientId(const char *id)
{
    const char		*name = "pmcd.client.whoami";
    pmID		pmid;
    int			sts;
    __pmResult		store;
    pmValueSet		pmvs;
    pmValueBlock	*pmvb;
    char        	host[MAXHOSTNAMELEN];
    char        	*ipaddr = NULL;
    __pmHostEnt		*servInfo;
    int			vblen;
    __pmContext		*ctxp;

    if ((sts = pmWhichContext()) < 0) {
	return sts;
    }
    ctxp = __pmHandleToPtr(sts);
    if (ctxp == NULL)
	return PM_ERR_NOCONTEXT;

    if ((sts = pmLookupName_ctx(ctxp, PM_NOT_LOCKED, 1, &name, &pmid)) < 0) {
	PM_UNLOCK(ctxp->c_lock);
	return sts;
    }

    /*
     * Try to obtain the address and the actual host name.
     * Compute the vblen as we go.
     */
    vblen = 0;
    (void)gethostname(host, MAXHOSTNAMELEN);
    if ((servInfo = __pmGetAddrInfo(host)) != NULL) {
	__pmSockAddr	*addr;
	void		*enumIx = NULL;
	char        	*servInfoName = NULL;
	for (addr = __pmHostEntGetSockAddr(servInfo, &enumIx);
	     addr != NULL;
	     addr = __pmHostEntGetSockAddr(servInfo, &enumIx)) {
	    servInfoName = __pmGetNameInfo(addr);
	    if (servInfoName != NULL)
		break;
	    __pmSockAddrFree(addr);
	}
	__pmHostEntFree(servInfo);

	/* Did we get a name? */
	if (servInfoName == NULL) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "__pmSetClientId: __pmGetNameInfo() failed: %s\n",
		    osstrerror_r(errmsg, sizeof(errmsg)));
	}
	else {
	    strncpy(host, servInfoName, sizeof(host));
	    host[sizeof(host) - 1] = '\0';
	    free(servInfoName);
	}
	vblen = strlen(host) + strlen(id) + 2;

	/* Did we get an address? */
	if (addr != NULL) {
	    ipaddr = __pmSockAddrToString(addr);
	    __pmSockAddrFree(addr);
	    if (ipaddr == NULL) {
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "__pmSetClientId: __pmSockAddrToString() failed: %s\n",
			osstrerror_r(errmsg, sizeof(errmsg)));
	    }
	    else
		vblen += strlen(ipaddr) + 3;
	}
    }
    vblen += strlen(host) + strlen(id) + 2;

    /* build pmResult for pmStore() */
    pmvb = (pmValueBlock *)calloc(1, PM_VAL_HDR_SIZE+vblen);
    if (pmvb == NULL) {
	PM_UNLOCK(ctxp->c_lock);
	pmNoMem("__pmSetClientId", PM_VAL_HDR_SIZE+vblen, PM_RECOV_ERR);
	if (ipaddr != NULL)
	    free(ipaddr);

	return -ENOMEM;
    }
    pmvb->vtype = PM_TYPE_STRING;
    pmvb->vlen = PM_VAL_HDR_SIZE+vblen;
    strcpy(pmvb->vbuf, host);
    strcat(pmvb->vbuf, " ");
    if (ipaddr != NULL) {
	strcat(pmvb->vbuf, "(");
	strcat(pmvb->vbuf, ipaddr);
	strcat(pmvb->vbuf, ") ");
	free(ipaddr);
    }
    strcat(pmvb->vbuf, id);

    pmvs.pmid = pmid;
    pmvs.numval = 1;
    pmvs.valfmt = PM_VAL_SPTR;
    pmvs.vlist[0].value.pval = pmvb;
    pmvs.vlist[0].inst = PM_IN_NULL;

    memset(&store, 0, sizeof(store));
    store.numpmid = 1;
    store.vset[0] = &pmvs;
    sts = pmStore_ctx(ctxp, &store);
    free(pmvb);
    PM_UNLOCK(ctxp->c_lock);
    return sts;
}

char *
__pmGetClientId(int argc, char **argv)
{
    char	*clientID = NULL;
    int		a, need = 0;

    for (a = 0; a < argc; a++)
	need += strlen(argv[a]) + 1;
    if (need)
	clientID = (char *)malloc(need);
    if (clientID) {
	clientID[0] = '\0';
	for (a = 0; a < argc; a++) {
	    strcat(clientID, argv[a]);
	    if (a < argc - 1)
		strcat(clientID, " ");
	}
    }
    return clientID;
}

int
__pmSetClientIdArgv(int argc, char **argv)
{
    char	*id = __pmGetClientId(argc, argv);
    int		sts;

    if (id) {
	sts = __pmSetClientId(id);
	free(id);
	return sts;
    }
    return -ENOMEM;
}

/*
 * Support for C environments that have lame libc implementations.
 * All of these developed from first principles, so no 3rd party
 * copyright or licensing issues, else used under a licence that
 * is compatible with the PCP licence.
 */

#ifndef HAVE_BASENAME
char *
basename(char *name)
{
    char	*p = strrchr(name, '/');

    if (p == NULL)
	return(name);
    else
	return(p+1);
}
#endif /* HAVE_BASENAME */

#ifndef HAVE_DIRNAME
char *
dirname(char *name)
{
    char	*p = strrchr(name, '/');

    if (p == NULL)
	return(".");
    else {
	*p = '\0';
	return(name);
    }
}
#endif /* HAVE_DIRNAME */

/*
 * Create a directory, including all of its path components.
 */
int
__pmMakePath(const char *dir, mode_t mode)
{
    char path[MAXPATHLEN], *p;
    int sts;

    sts = access(dir, R_OK|W_OK|X_OK);
    if (sts == 0)
	return 0;
    if (sts < 0 && oserror() != ENOENT)
	return -1;

    strncpy(path, dir, sizeof(path));
    path[sizeof(path)-1] = '\0';

    for (p = path+1; *p != '\0'; p++) {
	if (*p == pmPathSeparator()) {
	    *p = '\0';
	    if (mkdir2(path, mode) < 0) {
		/* dir may already exist */
		if (oserror() != EEXIST)
		    return -1;
	    }
	    *p = pmPathSeparator();
	}
    }
    return mkdir2(path, mode);
}

#ifndef HAVE_GETDOMAINNAME
int
getdomainname(char *buffer, size_t length)
{
    FILE *fp = fopen("/etc/defaultdomain", "r");
    char domain[MAXDOMAINNAMELEN];

    if (fp) {
	int i = fscanf(fp, "%" STR(MAXDOMAINNAMELEN) "s", domain);
	fclose(fp);
	if (i != 1)
	    return -EINVAL;
	return pmsprintf(buffer, length, "%.*s", MAXDOMAINNAMELEN, domain);
    }
    return -ENOTSUP;
}
#endif

#ifndef HAVE_GETMACHINEID
int
getmachineid(char *buffer, size_t length)
{
    FILE *fp = fopen("/etc/machine-id", "r");
    char machine[MAXMACHINEIDLEN];

    if (fp) {
	int i = fscanf(fp, "%" STR(MAXMACHINEIDLEN) "s", machine);
	fclose(fp);
	if (i != 1)
	    return -EINVAL;
	return pmsprintf(buffer, length, "%.*s", MAXMACHINEIDLEN, machine);
    }
    return -ENOTSUP;
}
#endif

#ifndef HAVE_STRNDUP
char *
strndup(const char *s, size_t n)
{
    char	*buf;

    if ((buf = malloc(n + 1)) != NULL) {
	strncpy(buf, s, n);
	buf[n] = '\0';
    }
    return buf;
}
#endif /* HAVE_STRNDUP */

#ifndef HAVE_STRCHRNUL
char *
strchrnul(const char *s, int c)
{
    char	*result;

    if ((result = strchr(s, c)) == NULL)
	result = strchr(s, '\0');
    return result;
}
#endif /* HAVE_STRCHRNUL */

#ifndef HAVE_SCANDIR
/*
 * Scan the directory dirname, building an array of pointers to
 * dirent entries using malloc(3).  select() and compare() are
 * used to optionally filter and sort directory entries.
 */
int
scandir(const char *dirname, struct dirent ***namelist,
        int(*select)(const_dirent *),
        int(*compare)(const_dirent **, const_dirent **))
{
    DIR			*dirp;
    int			n = 0;
    struct dirent	**names = NULL;
    struct dirent	*dp;
    struct dirent	*tp;

    if ((dirp = opendir(dirname)) == NULL)
	return -1;

    /* dirp is an on-stack variable, so readdir() is ... */
    /* THREADSAFE */
    while ((dp = readdir(dirp)) != NULL) {
	struct dirent	**names_new;
	if (select && (*select)(dp) == 0)
	    continue;

	n++;
	if ((names_new = (struct dirent **)realloc(names, n * sizeof(dp))) == NULL) {
	    free(names);
	    closedir(dirp);
	    return -1;
	}
	names = names_new;

	if ((names[n-1] = tp = (struct dirent *)malloc(
		sizeof(*dp)-sizeof(dp->d_name)+strlen(dp->d_name)+1)) == NULL) {
	    closedir(dirp);
	    n--;
	    while (n >= 1) {
		free(names[n-1]);
		n--;
	    }
	    free(names);
	    return -1;
	}

	tp->d_ino = dp->d_ino;
#if defined(HAVE_DIRENT_D_OFF)
	tp->d_off = dp->d_off;
#else
	tp->d_reclen = dp->d_reclen;
#endif
	memcpy(tp->d_name, dp->d_name, strlen(dp->d_name)+1);
    }
    closedir(dirp);
    *namelist = names;

    if (n && compare)
	qsort(names, n, sizeof(names[0]),
			(int(*)(const void *, const void *))compare);
    return n;
}

/*
 * Alphabetical sort for default use
 */
int
alphasort(const_dirent **p, const_dirent **q)
{
    return strcmp((*p)->d_name, (*q)->d_name);
}
#endif /* HAVE_SCANDIR */

#ifndef HAVE_POW
/*
 * For PCP we have not found a platform yet that needs this, but just
 * in case, this implementation comes from
 * http://www.netlib.org/fdlibm/e_pow.c
 *
 * ====================================================
 * Copyright (C) 2003 by Sun Microsystems, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#ifdef HAVE_ENDIAN_H
#include <endian.h>
#else
#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#else
bozo!
#endif
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define __HI(x) *(1+(int*)&x)
#define __LO(x) *(int*)&x
#define __HIp(x) *(1+(int*)x)
#define __LOp(x) *(int*)x
#else
#define __HI(x) *(int*)&x
#define __LO(x) *(1+(int*)&x)
#define __HIp(x) *(int*)x
#define __LOp(x) *(1+(int*)x)
#endif

/* _pow(x,y) return x**y
 *
 *		      n
 * Method:  Let x =  2   * (1+f)
 *	1. Compute and return log2(x) in two pieces:
 *		log2(x) = w1 + w2,
 *	   where w1 has 53-24 = 29 bit trailing zeros.
 *	2. Perform y*log2(x) = n+y' by simulating muti-precision
 *	   arithmetic, where |y'|<=0.5.
 *	3. Return x**y = 2**n*exp(y'*log2)
 *
 * Special cases:
 *	1.  (anything) ** 0  is 1
 *	2.  (anything) ** 1  is itself
 *	3.  (anything) ** NAN is NAN
 *	4.  NAN ** (anything except 0) is NAN
 *	5.  +-(|x| > 1) **  +INF is +INF
 *	6.  +-(|x| > 1) **  -INF is +0
 *	7.  +-(|x| < 1) **  +INF is +0
 *	8.  +-(|x| < 1) **  -INF is +INF
 *	9.  +-1         ** +-INF is NAN
 *	10. +0 ** (+anything except 0, NAN)               is +0
 *	11. -0 ** (+anything except 0, NAN, odd integer)  is +0
 *	12. +0 ** (-anything except 0, NAN)               is +INF
 *	13. -0 ** (-anything except 0, NAN, odd integer)  is +INF
 *	14. -0 ** (odd integer) = -( +0 ** (odd integer) )
 *	15. +INF ** (+anything except 0,NAN) is +INF
 *	16. +INF ** (-anything except 0,NAN) is +0
 *	17. -INF ** (anything)  = -0 ** (-anything)
 *	18. (-anything) ** (integer) is (-1)**(integer)*(+anything**integer)
 *	19. (-anything except 0 and inf) ** (non-integer) is NAN
 *
 * Accuracy:
 *	pow(x,y) returns x**y nearly rounded. In particular
 *			pow(integer,integer)
 *	always returns the correct integer provided it is
 *	representable.
 *
 * Constants :
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */

static const double
bp[] = {1.0, 1.5,},
dp_h[] = { 0.0, 5.84962487220764160156e-01,}, /* 0x3FE2B803, 0x40000000 */
dp_l[] = { 0.0, 1.35003920212974897128e-08,}, /* 0x3E4CFDEB, 0x43CFD006 */
zero    =  0.0,
one	=  1.0,
two	=  2.0,
two53	=  9007199254740992.0,	/* 0x43400000, 0x00000000 */
huge	=  1.0e300,
tiny    =  1.0e-300,
	/* poly coefs for (3/2)*(log(x)-2s-2/3*s**3 */
L1  =  5.99999999999994648725e-01, /* 0x3FE33333, 0x33333303 */
L2  =  4.28571428578550184252e-01, /* 0x3FDB6DB6, 0xDB6FABFF */
L3  =  3.33333329818377432918e-01, /* 0x3FD55555, 0x518F264D */
L4  =  2.72728123808534006489e-01, /* 0x3FD17460, 0xA91D4101 */
L5  =  2.30660745775561754067e-01, /* 0x3FCD864A, 0x93C9DB65 */
L6  =  2.06975017800338417784e-01, /* 0x3FCA7E28, 0x4A454EEF */
P1   =  1.66666666666666019037e-01, /* 0x3FC55555, 0x5555553E */
P2   = -2.77777777770155933842e-03, /* 0xBF66C16C, 0x16BEBD93 */
P3   =  6.61375632143793436117e-05, /* 0x3F11566A, 0xAF25DE2C */
P4   = -1.65339022054652515390e-06, /* 0xBEBBBD41, 0xC5D26BF1 */
P5   =  4.13813679705723846039e-08, /* 0x3E663769, 0x72BEA4D0 */
lg2  =  6.93147180559945286227e-01, /* 0x3FE62E42, 0xFEFA39EF */
lg2_h  =  6.93147182464599609375e-01, /* 0x3FE62E43, 0x00000000 */
lg2_l  = -1.90465429995776804525e-09, /* 0xBE205C61, 0x0CA86C39 */
ovt =  8.0085662595372944372e-0017, /* -(1024-log2(ovfl+.5ulp)) */
cp    =  9.61796693925975554329e-01, /* 0x3FEEC709, 0xDC3A03FD =2/(3ln2) */
cp_h  =  9.61796700954437255859e-01, /* 0x3FEEC709, 0xE0000000 =(float)cp */
cp_l  = -7.02846165095275826516e-09, /* 0xBE3E2FE0, 0x145B01F5 =tail of cp_h*/
ivln2    =  1.44269504088896338700e+00, /* 0x3FF71547, 0x652B82FE =1/ln2 */
ivln2_h  =  1.44269502162933349609e+00, /* 0x3FF71547, 0x60000000 =24b 1/ln2*/
ivln2_l  =  1.92596299112661746887e-08; /* 0x3E54AE0B, 0xF85DDF44 =1/ln2 tail*/

double
pow(double x, double y)
{
	double z,ax,z_h,z_l,p_h,p_l;
	double y1,t1,t2,r,s,t,u,v,w;
	int i,j,k,yisint,n;
	int hx,hy,ix,iy;
	unsigned lx,ly,hshift;

	hx = __HI(x); lx = __LO(x);
	hy = __HI(y); ly = __LO(y);
	ix = hx&0x7fffffff;  iy = hy&0x7fffffff;

    /* y==zero: x**0 = 1 */
	if((iy|ly)==0) return one;

    /* +-NaN return x+y */
	if(ix > 0x7ff00000 || ((ix==0x7ff00000)&&(lx!=0)) ||
	   iy > 0x7ff00000 || ((iy==0x7ff00000)&&(ly!=0)))
		return x+y;

    /* determine if y is an odd int when x < 0
     * yisint = 0	... y is not an integer
     * yisint = 1	... y is an odd int
     * yisint = 2	... y is an even int
     */
	yisint  = 0;
	if(hx<0) {
	    if(iy>=0x43400000) yisint = 2; /* even integer y */
	    else if(iy>=0x3ff00000) {
		k = (iy>>20)-0x3ff;	   /* exponent */
		if(k>20) {
		    j = ly>>(52-k);
		    if((j<<(52-k))==ly) yisint = 2-(j&1);
		} else if(ly==0) {
		    j = iy>>(20-k);
		    if((j<<(20-k))==iy) yisint = 2-(j&1);
		}
	    }
	}

    /* special value of y */
	if(ly==0) {
	    if (iy==0x7ff00000) {	/* y is +-inf */
	        if(((ix-0x3ff00000)|lx)==0)
		    return  y - y;	/* inf**+-1 is NaN */
	        else if (ix >= 0x3ff00000)/* (|x|>1)**+-inf = inf,0 */
		    return (hy>=0)? y: zero;
	        else			/* (|x|<1)**-,+inf = inf,0 */
		    return (hy<0)?-y: zero;
	    }
	    if(iy==0x3ff00000) {	/* y is  +-1 */
		if(hy<0) return one/x; else return x;
	    }
	    if(hy==0x40000000) return x*x; /* y is  2 */
	    if(hy==0x3fe00000) {	/* y is  0.5 */
		if(hx>=0)	/* x >= +0 */
		return sqrt(x);
	    }
	}

	ax   = fabs(x);
    /* special value of x */
	if(lx==0) {
	    if(ix==0x7ff00000||ix==0||ix==0x3ff00000){
		z = ax;			/*x is +-0,+-inf,+-1*/
		if(hy<0) z = one/z;	/* z = (1/|x|) */
		if(hx<0) {
		    if(((ix-0x3ff00000)|yisint)==0) {
			z = (z-z)/(z-z); /* (-1)**non-int is NaN */
		    } else if(yisint==1)
			z = -z;		/* (x<0)**odd = -(|x|**odd) */
		}
		return z;
	    }
	}

	hshift = (unsigned)hx;
	n = (hshift>>31)+1;

    /* (x<0)**(non-int) is NaN */
	if((n|yisint)==0) return (x-x)/(x-x);

	s = one; /* s (sign of result -ve**odd) = -1 else = 1 */
	if((n|(yisint-1))==0) s = -one;/* (-ve)**(odd int) */

    /* |y| is huge */
	if(iy>0x41e00000) { /* if |y| > 2**31 */
	    if(iy>0x43f00000){	/* if |y| > 2**64, must o/uflow */
		if(ix<=0x3fefffff) return (hy<0)? huge*huge:tiny*tiny;
		if(ix>=0x3ff00000) return (hy>0)? huge*huge:tiny*tiny;
	    }
	/* over/underflow if x is not close to one */
	    if(ix<0x3fefffff) return (hy<0)? s*huge*huge:s*tiny*tiny;
	    if(ix>0x3ff00000) return (hy>0)? s*huge*huge:s*tiny*tiny;
	/* now |1-x| is tiny <= 2**-20, suffice to compute
 	   log(x) by x-x^2/2+x^3/3-x^4/4 */
	    t = ax-one;		/* t has 20 trailing zeros */
	    w = (t*t)*(0.5-t*(0.3333333333333333333333-t*0.25));
	    u = ivln2_h*t;	/* ivln2_h has 21 sig. bits */
	    v = t*ivln2_l-w*ivln2;
	    t1 = u+v;
	    __LO(t1) = 0;
	    t2 = v-(t1-u);
	} else {
	    double ss,s2,s_h,s_l,t_h,t_l;
	    n = 0;
	/* take care subnormal number */
	    if(ix<0x00100000)
		{ax *= two53; n -= 53; ix = __HI(ax); }
	    n  += ((ix)>>20)-0x3ff;
	    j  = ix&0x000fffff;
	/* determine interval */
	    ix = j|0x3ff00000;		/* normalize ix */
	    if(j<=0x3988E) k=0;		/* |x|<sqrt(3/2) */
	    else if(j<0xBB67A) k=1;	/* |x|<sqrt(3)   */
	    else {k=0;n+=1;ix -= 0x00100000;}
	    __HI(ax) = ix;

	/* compute ss = s_h+s_l = (x-1)/(x+1) or (x-1.5)/(x+1.5) */
	    u = ax-bp[k];		/* bp[0]=1.0, bp[1]=1.5 */
	    v = one/(ax+bp[k]);
	    ss = u*v;
	    s_h = ss;
	    __LO(s_h) = 0;
	/* t_h=ax+bp[k] High */
	    t_h = zero;
	    __HI(t_h)=((ix>>1)|0x20000000)+0x00080000+(k<<18);
	    t_l = ax - (t_h-bp[k]);
	    s_l = v*((u-s_h*t_h)-s_h*t_l);
	/* compute log(ax) */
	    s2 = ss*ss;
	    r = s2*s2*(L1+s2*(L2+s2*(L3+s2*(L4+s2*(L5+s2*L6)))));
	    r += s_l*(s_h+ss);
	    s2  = s_h*s_h;
	    t_h = 3.0+s2+r;
	    __LO(t_h) = 0;
	    t_l = r-((t_h-3.0)-s2);
	/* u+v = ss*(1+...) */
	    u = s_h*t_h;
	    v = s_l*t_h+t_l*ss;
	/* 2/(3log2)*(ss+...) */
	    p_h = u+v;
	    __LO(p_h) = 0;
	    p_l = v-(p_h-u);
	    z_h = cp_h*p_h;		/* cp_h+cp_l = 2/(3*log2) */
	    z_l = cp_l*p_h+p_l*cp+dp_l[k];
	/* log2(ax) = (ss+..)*2/(3*log2) = n + dp_h + z_h + z_l */
	    t = (double)n;
	    t1 = (((z_h+z_l)+dp_h[k])+t);
	    __LO(t1) = 0;
	    t2 = z_l-(((t1-t)-dp_h[k])-z_h);
	}

    /* split up y into y1+y2 and compute (y1+y2)*(t1+t2) */
	y1  = y;
	__LO(y1) = 0;
	p_l = (y-y1)*t1+y*t2;
	p_h = y1*t1;
	z = p_l+p_h;
	j = __HI(z);
	i = __LO(z);
	if (j>=0x40900000) {				/* z >= 1024 */
	    if(((j-0x40900000)|i)!=0)			/* if z > 1024 */
		return s*huge*huge;			/* overflow */
	    else {
		if(p_l+ovt>z-p_h) return s*huge*huge;	/* overflow */
	    }
	} else if((j&0x7fffffff)>=0x4090cc00 ) {	/* z <= -1075 */
	    if(((j-0xc090cc00)|i)!=0) 		/* z < -1075 */
		return s*tiny*tiny;		/* underflow */
	    else {
		if(p_l<=z-p_h) return s*tiny*tiny;	/* underflow */
	    }
	}
    /*
     * compute 2**(p_h+p_l)
     */
	i = j&0x7fffffff;
	k = (i>>20)-0x3ff;
	n = 0;
	if(i>0x3fe00000) {		/* if |z| > 0.5, set n = [z+0.5] */
	    n = j+(0x00100000>>(k+1));
	    k = ((n&0x7fffffff)>>20)-0x3ff;	/* new k for n */
	    t = zero;
	    __HI(t) = (n&~(0x000fffff>>k));
	    n = ((n&0x000fffff)|0x00100000)>>(20-k);
	    if(j<0) n = -n;
	    p_h -= t;
	}
	t = p_l+p_h;
	__LO(t) = 0;
	u = t*lg2_h;
	v = (p_l-(t-p_h))*lg2+t*lg2_l;
	z = u+v;
	w = v-(z-u);
	t  = z*z;
	t1  = z - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))));
	r  = (z*t1)/(t1-two)-(w+z*w);
	z  = one-(r-z);
	j  = __HI(z);
	j += (n<<20);
	if((j>>20)<=0) z = scalbn(z,n);	/* subnormal output */
	else __HI(z) += (n<<20);
	return s*z;
}
#endif /* HAVE_POW */

#define PROCFS_ENTRY_SIZE 40	/* encompass any size of entry for pid */

#if defined(IS_DARWIN) /* No procfs on Mac OS X */
int
__pmProcessExists(pid_t pid)
{
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[4];

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = pid;
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1)
       return 0;
    return (len > 0);
}
#elif defined(IS_FREEBSD) || defined(IS_OPENBSD)
#include <errno.h>
int
__pmProcessExists(pid_t pid)
{
    /*
     * kill(.., 0) -1 and errno == ESRCH if the process does not exist
     */
    errno = 0;
    if (kill(pid, 0) == -1 && errno == ESRCH)
	return 0;
    else
	return 1;
}
#elif !defined(IS_MINGW)
#define PROCFS			"/proc"
#define PROCFS_PATH_SIZE	(sizeof(PROCFS)+PROCFS_ENTRY_SIZE)
int
__pmProcessExists(pid_t pid)
{
    char proc_buf[PROCFS_PATH_SIZE];
    pmsprintf(proc_buf, sizeof(proc_buf), "%s/%" FMT_PID, PROCFS, pid);
    return (access(proc_buf, F_OK) == 0);
}
#endif

#if defined(HAVE_KILL)
int
__pmProcessTerminate(pid_t pid, int force)
{
    return kill(pid, force ? SIGKILL : SIGTERM);
}
#elif !defined(IS_MINGW)
!bozo!
#endif

#if defined(IS_DARWIN)
#define sbrk hack_sbrk
/*
 * cheap and nasty sbrk(0) for Mac OS X where sbrk() is deprecated
 */
void *
hack_sbrk(int incr)
{
    static size_t	get[] = { 4096, 2000, 900, 2000, 4096 };
    static int		pick = 0;
    static void		*highwater = NULL;
    void		*try;
    if (incr != 0)
	return NULL;

    try = malloc(get[pick]);
    if (try > highwater)
	highwater = try;
    free(try);

    pick++;
    if (pick == sizeof(get) / sizeof(get[0]))
	pick = 0;

    return highwater;
}
#endif

#if defined(HAVE_SBRK)
int
__pmProcessDataSize(unsigned long *size)
{
    static char *base;

    if (size && base)
	*size = ((char *)sbrk(0) - base) / 1024;
    else {
	base = (char *)sbrk(0);
	if (size)
	    *size = 0;
    }
    return 0;
}
#elif !defined(IS_MINGW) && !defined(IS_DARWIN)
#warning "Platform does not define a process datasize interface?"
int __pmProcessDataSize(unsigned long *) { return -1; }
#endif

#if defined(IS_DARWIN)
#undef sbrk
#endif

#if !defined(IS_MINGW)
int
__pmProcessRunTimes(double *usr, double *sys)
{
    struct tms tms;
    double ticks = (double)sysconf(_SC_CLK_TCK);

    if (times(&tms) == (clock_t)-1) {
	*usr = *sys = 0.0;
	return -1;
    }
    *usr = (double)tms.tms_utime / ticks;
    *sys = (double)tms.tms_stime / ticks;
    return 0;
}
#endif

#if !defined(IS_MINGW)
/*
 * fromChild - pipe used for reading from the caller, connected to the
 * standard output of the created process
 * toChild - pipe used for writing from the caller, connected to the
 * std input of the created process
 * If either is NULL, no pipe is created and created process inherits
 * stdio streams from the parent.
 */
pid_t
__pmProcessCreate(char **argv, int *fromChild, int *toChild)
{
    int		in[2];
    int		out[2];
    pid_t	pid;

    if (pipe1(in) < 0)
	return -oserror();
    if (pipe1(out) < 0)
	return -oserror();

    pid = fork();
    if (pid < 0) {
	return -1;
    }
    else if (pid) {
	/* parent */
	close(in[0]);
	close(out[1]);
	*fromChild = out[0];
	*toChild = in[1];
    }
    else {
	/* child */
	char	errmsg[PM_MAXERRMSGLEN];
	close(in[1]);
	close(out[0]);
	if (in[0] != 0) {
	    close(0);
	    dup2(in[0], 0);
	    close(in[0]);
	}
	if (out[1] != 1) {
	    close(1);
	    dup2(out[1], 1);
	    close(out[1]);
	}
	execvp(argv[0], argv);
	fprintf(stderr, "execvp: %s\n", osstrerror_r(errmsg, sizeof(errmsg)));
	exit(1);
    }
    return pid;
}

int
__pmSetSignalHandler(int sig, __pmSignalHandler func)
{
    signal(sig, func);
    return 0;
}

void
pmSetProgname(const char *program)
{
    char	*p;

    __pmInitLocks();	/* not used here, just get in early */

    if (program == NULL) {
	/* Restore the default application name */
	pmProgname = "pcp";
    } else {
	/* Trim command name of leading directory components */
	pmProgname = (char *)program;
	for (p = pmProgname; *p; p++)
	    if (*p == '/')
		pmProgname = p+1;
    }
}

int
__pmShutdown(void)
{
    int code = 0, sts;

    if ((sts = __pmShutdownLocal()) < 0 && !code)
	code = sts;
    if ((sts = __pmShutdownCertificates()) < 0 && !code)
	code = sts;
    if ((sts = __pmShutdownSecureSockets()) < 0 && !code)
	code = sts;
    return code;
}

void *
__pmMemoryMap(int fd, size_t sz, int writable)
{
    int mflags = writable ? (PROT_READ | PROT_WRITE) : PROT_READ;
    void *addr = mmap(NULL, sz, mflags, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
	return NULL;
    return addr;
}

void
__pmMemoryUnmap(void *addr, size_t sz)
{
    munmap(addr, sz);
}
#endif /* !IS_MINGW */

static char	*text_start = NULL;

void
__pmDumpStackInit(void *addr)
{
    /*
     * Inititialization call ... caller knows where their text
     * segment starts
     */
    text_start = (char *)addr;
}

#if HAVE_TRACE_BACK_STACK
#include <libexc.h>
#define MAX_DEPTH 30	/* max callback procedure depth */
#define MAX_SIZE 48	/* max function name length */

void
__pmDumpStack(void)
{
    __uint64_t	call_addr[MAX_DEPTH];
    char	*call_fn[MAX_DEPTH];
    char	names[MAX_DEPTH][MAX_SIZE];
    int		res;
    int		i;

    fprintf(stderr, "Procedure call traceback ...\n");
    for (i = 0; i < MAX_DEPTH; i++)
	call_fn[i] = names[i];
    res = trace_back_stack(MAX_DEPTH, call_addr, call_fn, MAX_DEPTH, MAX_SIZE);
    for (i = 1; i < res; i++) {
	if (sizeof(void *) == sizeof(long long))
	    fprintf(stderr, "  0x%016llx [%s]\n", call_addr[i], call_fn[i]);
	else
	    fprintf(stderr, "  0x%08lx [%s]\n", (__uint32_t)call_addr[i], call_fn[i]);
    }
}

#elif HAVE_BACKTRACE
#include <execinfo.h>

void
__pmDumpStack(void)
{
#define MAX_TRACE_DEPTH 32	/* max callback procedure depth */
#define MAX_SYMBOL_LENGTH 128	/* max length of a function name */
    void	*backaddr[MAX_TRACE_DEPTH] = { 0 };
    int		nsymbols;
#ifdef HAVE___ETEXT
    extern char	__etext;
    char	*text_end = &__etext;
#else
#ifdef HAVE__ETEXT
    extern char	_etext;
    char	*text_end = &_etext;
#else
#ifdef HAVE_ETEXT
    extern char	etext;
    char	*text_end = &etext;
#else
    char	*text_end = NULL;
#endif
#endif
#endif

    fprintf(stderr, "Procedure call traceback ...\n");
    if (text_start != NULL)
	fprintf(stderr, "executable text segment: " PRINTF_P_PFX "%p ... " PRINTF_P_PFX "%p\n", text_start, text_end);

    nsymbols = backtrace(backaddr, MAX_TRACE_DEPTH);
    if (nsymbols > 0) {
	int	fd;
	char	**symbols = NULL;
	int	i;

	/*
	 * Possible name collision here to simplify the code ... we unlink()
	 * as soon as it is created so the risk is small.
	 */
	fd = open("/tmp/dumpstack", O_CREAT|O_RDWR, 0644);
	if (fd < 0) {
	    fprintf(stderr, "Failed to create \"/tmp/dumpstack\", falling back to backtrace_symbols()\n");
	    symbols = backtrace_symbols(backaddr, nsymbols);
	}
	else {
	    /*
	     * Preferred path to avoid calling malloc() in backtrace_symbols()
	     * in case that's the real cause of a problem that got us here.
	     */
	    unlink("/tmp/dumpstack");
	    backtrace_symbols_fd(backaddr, nsymbols, fd);
	    lseek(fd, (off_t)0, SEEK_SET);
	}

	for (i = 0; i < nsymbols; i++) {
	    if (fd >= 0) {
		char	c;
		fprintf(stderr, "  ");
		for ( ; ; ) {
		    int		sts;
		    sts = read(fd, &c, 1);
		    if (sts < 0) {
			fprintf(stderr, "Botch: read() returns %d\n", sts);
			break;
		    }
		    else if (sts == 0)
			break;
		    else if (c == '\n')
			break;
		    fputc(c, stderr);
		}
	    }
	    else if (symbols != NULL)
		fprintf(stderr, "  %s", symbols[i]);
	    else
		fprintf(stderr, "  %p ??unknown??", backaddr[i]);
	    if (text_start != NULL) {
		/*
		 * report address offset from the base of the text segment
		 * ... this matches addresses from nm(1), but more importantly
		 * is the address that is needed for addr2line(1)
		 */
		if (text_start  <= (char *)backaddr[i] && 
		    (text_end == NULL || (char *)backaddr[i] <= text_end))
		    fprintf(stderr, " (0x%llx)", (long long)((char *)backaddr[i]-text_start));
	    }
	    fputc('\n', stderr);
	}
	if (fd >= 0)
	    close(fd);
	else if (symbols != NULL)
	    free(symbols);
    }
    else {
	fprintf(stderr, "backtrace() returns %d, nothing to report\n", nsymbols);
    }
    return;
}
#else	/* no known mechanism, provide a stub (called unconditionally) */
void
__pmDumpStack(void)
{
    fprintf(stderr, "[No backtrace support available]\n");
}
#endif /* HAVE_BACKTRACE */

/*
 * pmID helper functions
 */

unsigned int
pmID_item(pmID pmid)
{
    __pmID_int	*idp = (__pmID_int *)&pmid;
    return idp->item;
}

unsigned int
pmID_cluster(pmID pmid)
{
    __pmID_int	*idp = (__pmID_int *)&pmid;
    return idp->cluster;
}

unsigned int
pmID_domain(pmID pmid)
{
    __pmID_int	*idp = (__pmID_int *)&pmid;
    return idp->domain;
}

pmID
pmID_build(unsigned int domain, unsigned int cluster, unsigned int item)
{
    pmID	pmid;
    __pmID_int	pmid_int;

    pmid_int.flag = 0;
    pmid_int.domain = domain;
    pmid_int.cluster = cluster;
    pmid_int.item = item;
    memcpy(&pmid, &pmid_int, sizeof(pmid));
    return pmid;
}

unsigned int
pmInDom_domain(pmInDom indom)
{
    __pmInDom_int	*idp = (__pmInDom_int *)&indom;
    return idp->domain;
}

unsigned int
pmInDom_serial(pmInDom indom)
{
    __pmInDom_int	*idp = (__pmInDom_int *)&indom;
    return idp->serial;
}

pmInDom
pmInDom_build(unsigned int domain, unsigned int serial)
{
    pmInDom		indom;
    __pmInDom_int	indom_int;

    indom_int.flag = 0;
    indom_int.domain = domain;
    indom_int.serial = serial;
    memcpy(&indom, &indom_int, sizeof(indom));
    return indom;
}

/*
 * Convert archive feature bits into a string ... result is variable
 * length and not static, so result is malloc'd and 
 * __pmLogFeaturesStr_r() is not required.
 *
 * Return NULL on alloc error ... caller should test before calling
 * free().
 */
char *
__pmLogFeaturesStr(__uint32_t features)
{
    char	*ans = (char *)malloc(1);
    int		len = 1;
    __uint32_t	check = features;
    int		pos;
    int		unknown = 0;
    char	buf[8];		/* enough for bit_NN */

    if (ans == NULL) {
	pmNoMem("__pmLogFeaturesStr: malloc", 1, PM_RECOV_ERR);
	return NULL;
    }
    ans[0] = '\0';

    for (pos = 31; pos >= 0; pos--) {
	if (check & 0x80000000) {
	    char	*ans_tmp;
	    char	*append;
	    switch (pos) {
		case 31:	/* QA */
			append = "QA";
			break;

		default:
			append = buf;
			snprintf(buf, 8, "bit_%02d", pos);
			break;
	    }
	    len += strlen(append) + 1;
	    if (unknown > 0)
		len++;
	    ans_tmp = (char *)realloc(ans, len);
	    if (ans_tmp == NULL) {
		pmNoMem("__pmLogFeaturesStr: realloc", len, PM_RECOV_ERR);
		/* best we can do ... */
		free(ans);
		return NULL;
	    }
	    ans = ans_tmp;
	    if (unknown > 0)
		strcat(ans, " ");
	    strcat(ans, append);
	    unknown++;
	}
	check = check << 1;
    }

    return(ans);
}
