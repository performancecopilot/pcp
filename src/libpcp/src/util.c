/*
 * General Utility Routines
 *
 * Copyright (c) 2012-2015 Red Hat.
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
 * pmProgname - most likely set in main(), not worth protecting here
 * 	and impossible to capture all the read uses in other places
 *
 * base (in __pmProcessDataSize) - no real side-effects, don't bother
 *	locking
 */

#include <stdarg.h>
#include <sys/stat.h> 
#include <inttypes.h>
#include <limits.h>
#include <ctype.h>

#include "pmapi.h"
#include "impl.h"
#include "pmdbg.h"
#include "internal.h"

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

static FILE	**filelog;
static int	nfilelog;
static int	dosyslog;
static int	pmState = PM_STATE_APPL;
static int	done_exit;

PCP_DATA char	*pmProgname = "pcp";		/* the real McCoy */

static int vpmprintf(const char *, va_list);

/*
 * if onoff == 1, logging is to syslog and stderr, else logging is
 * just to stderr (this is the default)
 */
void
__pmSyslog(int onoff)
{
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    dosyslog = onoff;
    if (dosyslog)
	openlog("pcp", LOG_PID, LOG_DAEMON);
    else
	closelog();
    PM_UNLOCK(__pmLock_libpcp);
}

/*
 * This is a wrapper around syslog(3C) that writes similar messages to stderr,
 * but if __pmSyslog(1) is called, the messages will really go to syslog
 */
void
__pmNotifyErr(int priority, const char *message, ...)
{
    va_list		arg;
    char		*p;
    char		*level;
    time_t		now;

    va_start(arg, message);

    time(&now);

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (dosyslog) {
	char	syslogmsg[2048];

	vsnprintf(syslogmsg, sizeof(syslogmsg), message, arg);
	va_end(arg);
	va_start(arg, message);
	syslog(priority, "%s", syslogmsg);
    }
    PM_UNLOCK(__pmLock_libpcp);

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

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    pmprintf("[%.19s] %s(%" FMT_PID ") %s: ", ctime(&now), pmProgname, getpid(), level);
    PM_UNLOCK(__pmLock_libpcp);
    vpmprintf(message, arg);
    va_end(arg);
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
    time_t	now;
    char	host[MAXHOSTNAMELEN];

    setlinebuf(log);		/* line buffering for log files */
    gethostname(host, MAXHOSTNAMELEN);
    host[MAXHOSTNAMELEN-1] = '\0';
    time(&now);
    PM_LOCK(__pmLock_libpcp);
    fprintf(log, "Log for %s on %s %s %s\n", progname, host, act, ctime(&now));
    PM_UNLOCK(__pmLock_libpcp);
}

static void
logfooter(FILE *log, const char *act)
{
    time_t	now;

    time(&now);
    PM_LOCK(__pmLock_libpcp);
    fprintf(log, "\nLog %s %s", act, ctime(&now));
    PM_UNLOCK(__pmLock_libpcp);
}

static void
logonexit(void)
{
    int		i;

    PM_LOCK(__pmLock_libpcp);
    if (++done_exit != 1) {
	PM_UNLOCK(__pmLock_libpcp);
	return;
    }

    for (i = 0; i < nfilelog; i++)
	logfooter(filelog[i], "finished");

    PM_UNLOCK(__pmLock_libpcp);
}

/* common code shared by __pmRotateLog and __pmOpenLog */
static FILE *
logreopen(const char *progname, const char *logname, FILE *oldstream,
	    int *status)
{
    int		oldfd;
    int		dupoldfd;
    FILE	*dupoldstream = oldstream;

    /*
     * Do our own version of freopen() because the standard one closes the
     * original stream BEFORE trying to open the new one.  Once it's gone,
     * there's no way to get the closed stream back if the open fails.
     */

    fflush(oldstream);
    *status = 0;		/* set to one if all this works ... */
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

	oldstream = freopen(logname, "w", oldstream);
	if (oldstream == NULL) {
	    int	save_error = oserror();	/* need for error message */

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
		oldstream = dupoldstream;
	    }
	    pmprintf("%s: cannot open log \"%s\" for writing : %s\n",
		    progname, logname, strerror(save_error));
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
		progname, logname, strerror(errno));
	pmflush();
    }
    return oldstream;
}

FILE *
__pmOpenLog(const char *progname, const char *logname, FILE *oldstream,
	    int *status)
{
    oldstream = logreopen(progname, logname, oldstream, status);
    PM_INIT_LOCKS();
    logheader(progname, oldstream, "started");

    PM_LOCK(__pmLock_libpcp);
    nfilelog++;
    if (nfilelog == 1)
	atexit(logonexit);

    filelog = (FILE **)realloc(filelog, nfilelog * sizeof(FILE *));
    if (filelog == NULL) {
	__pmNoMem("__pmOpenLog", nfilelog * sizeof(FILE *), PM_FATAL_ERR);
    }
    filelog[nfilelog-1] = oldstream;

    PM_UNLOCK(__pmLock_libpcp);
    return oldstream;
}

FILE *
__pmRotateLog(const char *progname, const char *logname, FILE *oldstream,
	    int *status)
{
    int		i;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    for (i = 0; i < nfilelog; i++) {
	if (oldstream == filelog[i]) {
	    logfooter(oldstream, "rotated");	/* old */
	    oldstream = logreopen(progname, logname, oldstream, status);
	    logheader(progname, oldstream, "rotated");	/* new */
	    filelog[i] = oldstream;
	    break;
	}
    }
    PM_UNLOCK(__pmLock_libpcp);
    return oldstream;
}

/* pmID -> string, max length is 20 bytes */
char *
pmIDStr_r(pmID pmid, char *buf, int buflen)
{
    __pmID_int*	p = (__pmID_int*)&pmid;
    if (pmid == PM_ID_NULL)
	snprintf(buf, buflen, "%s", "PM_ID_NULL");
    else if (p->domain == DYNAMIC_PMID && p->item == 0)
	/*
	 * this PMID represents the base of a dynamic subtree in the PMNS
	 * ... identified by setting the domain field to the reserved
	 * value DYNAMIC_PMID and storing the real domain of the PMDA
	 * that can enumerate the subtree in the cluster field, while
	 * the item field is not used
	 */
	snprintf(buf, buflen, "%d.*.*", p->cluster);
    else
	snprintf(buf, buflen, "%d.%d.%d", p->domain, p->cluster, p->item);
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
	snprintf(buf, buflen, "%s", "PM_INDOM_NULL");
    else
	snprintf(buf, buflen, "%d.%d", p->domain, p->serial);
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
	    snprintf(buf, buflen, " inf?  ");
	else if (value >= 999995000000.0)
	    snprintf(buf, buflen, "%6.2fT", (double)((long double)value / (long double)1000000000000.0));
	else if (value >= 999995000.0)
	    snprintf(buf, buflen, "%6.2fG", (double)((long double)value / (long double)1000000000.0));
	else if (value >= 999995.0)
	    snprintf(buf, buflen, "%6.2fM", (double)((long double)value / (long double)1000000.0));
	else if (value >= 999.995)
	    snprintf(buf, buflen, "%6.2fK", value / 1000.0);
	else if (value >= 0.005)
	    snprintf(buf, buflen, "%6.2f ", value);
	else
	    snprintf(buf, buflen, "%6.2f ", 0.0);
    }
    else {
	if (value <= -99995000000000.0)
	    snprintf(buf, buflen, "-inf?  ");
	else if (value <= -99995000000.0)
	    snprintf(buf, buflen, "%6.2fT", (double)((long double)value / (long double)1000000000000.0));
	else if (value <= -99995000.0)
	    snprintf(buf, buflen, "%6.2fG", (double)((long double)value / (long double)1000000000.0));
	else if (value <= -99995.0)
	    snprintf(buf, buflen, "%6.2fM", (double)((long double)value / (long double)1000000.0));
	else if (value <= -99.995)
	    snprintf(buf, buflen, "%6.2fK", value / 1000.0);
	else if (value <= -0.005)
	    snprintf(buf, buflen, "%6.2f ", value);
	else
	    snprintf(buf, buflen, "%6.2f ", 0.0);
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

    if (flags & PM_EVENT_FLAG_MISSED)
	return strcpy(buf, "missed");

    buf[0] = '\0';
    if (flags & PM_EVENT_FLAG_POINT) {
	if (started++) strcat(buf, ",");
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
	__pmNoMem("__pmStringListAdd", newSize, PM_FATAL_ERR);
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
static int
save_debug(void)
{
    int saved = pmDebug;
    pmDebug = 0;
    return saved;
}

static void
restore_debug(int saved)
{
    pmDebug = saved;
}

static void
dump_valueset(FILE *f, pmValueSet *vsp)
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
    if ((n = pmNameAll(vsp->pmid, &names)) < 0)
	fprintf(f, "  %s (%s):", pmid, "<noname>");
    else {
	int	j;
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
	pmLookupDesc(vsp->pmid, &desc) < 0) {
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
		pmNameInDom(desc.indom, vp->inst, &p) >= 0) {
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
	    else
		pmPrintValue(f, vsp->valfmt, (int)vp->value.pval->vtype, vp, 1); 
	}
	fputc('\n', f);
    }
}

void
__pmDumpResult(FILE *f, const pmResult *resp)
{
    int		i, saved;

    saved = save_debug();
    fprintf(f, "pmResult dump from " PRINTF_P_PFX "%p timestamp: %d.%06d ",
	resp, (int)resp->timestamp.tv_sec, (int)resp->timestamp.tv_usec);
    __pmPrintStamp(f, &resp->timestamp);
    fprintf(f, " numpmid: %d\n", resp->numpmid);
    for (i = 0; i < resp->numpmid; i++)
	dump_valueset(f, resp->vset[i]);
    restore_debug(saved);
}

void
__pmDumpHighResResult(FILE *f, const pmHighResResult *hresp)
{
    int		i, saved;

    saved = save_debug();
    fprintf(f, "pmHighResResult dump from " PRINTF_P_PFX "%p timestamp: %d.%09d ",
	hresp, (int)hresp->timestamp.tv_sec, (int)hresp->timestamp.tv_nsec);
    __pmPrintHighResStamp(f, &hresp->timestamp);
    fprintf(f, " numpmid: %d\n", hresp->numpmid);
    for (i = 0; i < hresp->numpmid; i++)
	dump_valueset(f, hresp->vset[i]);
    restore_debug(saved);
}

static void
print_event_summary(FILE *f, const pmValue *val, int highres)
{
    struct timespec	tsstamp;
    struct timeval	tvstamp;
    __pmTimespec 	*tsp;
    __pmTimeval 	*tvp;
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
	tsp = (__pmTimespec *)base;
	tsstamp.tv_sec = tsp->tv_sec;
	tsstamp.tv_nsec = tsp->tv_nsec;
    }
    else {
	pmEventArray *eap = (pmEventArray *)val->value.pval;
	nrecords = eap->ea_nrecords;
	base = (char *)&eap->ea_record[0];
	tvp = (__pmTimeval *)base;
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
	    __pmPrintHighResStamp(f, &tsstamp);
	else
	    __pmPrintStamp(f, &tvstamp);

	if (nrecords > 1) {
	    fprintf(f, "...");
	    if (highres) {
		tsp = (__pmTimespec *)base;
		tsstamp.tv_sec = tsp->tv_sec;
		tsstamp.tv_nsec = tsp->tv_nsec;
		__pmPrintHighResStamp(f, &tsstamp);
	    }
	    else {
		tvp = (__pmTimeval *)base;
		tvstamp.tv_sec = tvp->tv_sec;
		tvstamp.tv_usec = tvp->tv_usec;
		__pmPrintStamp(f, &tvstamp);
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
	else {
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
		fprintf(f, "pmPrintValue: negative length (%d) for aggregate value?",
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
__pmNoMem(const char *where, size_t size, int fatal)
{
    char	errmsg[PM_MAXERRMSGLEN];
    __pmNotifyErr(fatal ? LOG_ERR : LOG_WARNING,
			"%s: malloc(%d) failed: %s",
			where, (int)size, osstrerror_r(errmsg, sizeof(errmsg)));
    if (fatal)
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

/*
 * Difference for two of the internal timestamps ...
 * Same as __pmtimevalSub() in tv.c, just with __pmTimeval args
 * rather than struct timeval args.
 */
double
__pmTimevalSub(const __pmTimeval *ap, const __pmTimeval *bp)
{
     return (double)(ap->tv_sec - bp->tv_sec + (long double)(ap->tv_usec - bp->tv_usec) / (long double)1000000);
}

/*
 * print timeval timestamp in HH:MM:SS.XXX format
 */
void
__pmPrintStamp(FILE *f, const struct timeval *tp)
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
__pmPrintHighResStamp(FILE *f, const struct timespec *tp)
{
    struct tm	tmp;
    time_t	now;

    now = (time_t)tp->tv_sec;
    pmLocaltime(&now, &tmp);
    fprintf(f, "%02d:%02d:%02d.%09d", tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)(tp->tv_nsec));
}

/*
 * print __pmTimeval timestamp in HH:MM:SS.XXX format
 * (__pmTimeval variant used in PDUs, archives and internally)
 */
void
__pmPrintTimeval(FILE *f, const __pmTimeval *tp)
{
    struct tm	tmp;
    time_t	now;

    now = (time_t)tp->tv_sec;
    pmLocaltime(&now, &tmp);
    fprintf(f, "%02d:%02d:%02d.%03d", tmp.tm_hour, tmp.tm_min, tmp.tm_sec, tp->tv_usec/1000);
}

/*
 * print __pmTimespec timestamp in HH:MM:SS.XXXXXXXXX format
 * (__pmTimespec variant used in events, archives and internally)
 */
void
__pmPrintTimespec(FILE *f, const __pmTimespec *tp)
{
    struct tm	tmp;
    time_t	now;

    now = (time_t)tp->tv_sec;
    pmLocaltime(&now, &tmp);
    fprintf(f, "%02d:%02d:%02d.%09ld", tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (long)tp->tv_nsec);
}

/*
 * descriptor
 */
void
__pmPrintDesc(FILE *f, const pmDesc *desc)
{
    const char		*type;
    const char		*sem;
    static const char	*unknownVal = "???";
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
	    type = unknownVal;
	    break;
    }
    fprintf(f, "    Data Type: %s", type);
    if (type == unknownVal)
	fprintf(f, " (%d)", desc->type);

    fprintf(f, "  InDom: %s 0x%x\n", pmInDomStr_r(desc->indom, strbuf, sizeof(strbuf)), desc->indom);

    switch (desc->sem) {
	case PM_SEM_COUNTER:
	    sem = "counter";
	    break;
	case PM_SEM_INSTANT:
	    sem = "instant";
	    break;
	case PM_SEM_DISCRETE:
	    sem = "discrete";
	    break;
	default:
	    sem = unknownVal;
	    break;
    }

    fprintf(f, "    Semantics: %s", sem);
    if (sem == unknownVal)
	fprintf(f, " (%d)", desc->sem);

    fprintf(f, "  Units: ");
    units = pmUnitsStr_r(&desc->units, strbuf, sizeof(strbuf));
    if (*units == '\0')
	fprintf(f, "none\n");
    else
	fprintf(f, "%s\n", units);
}

/*
 * print times between events
 */
void
__pmEventTrace_r(const char *event, int *first, double *sum, double *last)
{
#ifdef PCP_DEBUG
    struct timeval tv;
    double now;

    __pmtimevalNow(&tv);
    now = __pmtimevalToReal(&tv);
    if (*first) {
	*first = 0;
	*sum = 0;
	*last = now;
    }
    *sum += now - *last;
    fprintf(stderr, "%s: +%4.2f = %4.2f -> %s\n",
			pmProgname, now-*last, *sum, event);
    *last = now;
#endif
}

void
__pmEventTrace(const char *event)
{
#ifdef PCP_DEBUG
    static double last;
    static double sum;
    static int first = 1;

    __pmEventTrace_r(event, &first, &sum, &last);
#endif
}

int
__pmParseDebug(const char *spec)
{
#ifdef PCP_DEBUG
    int		val = 0;
    int		tmp;
    const char	*p;
    char	*pend;
    int		i;

    for (p = spec; *p; ) {
	tmp = (int)strtol(p, &pend, 10);
	if (tmp == -1)
	    /* special case ... -1 really means set all the bits! */
	    tmp = INT_MAX;
	if (*pend == '\0') {
	    val |= tmp;
	    break;
	}
	else if (*pend == ',') {
	    val |= tmp;
	    p = pend + 1;
	}
	else {
	    pend = strchr(p, ',');
	    if (pend != NULL)
		*pend = '\0';

	    if (strcasecmp(p, "ALL") == 0) {
		val |= INT_MAX;
		if (pend != NULL) {
		    *pend = ',';
		    p = pend + 1;
		}
		else
		    p = "";		/* force termination of outer loop */
		break;
	    }

	    for (i = 0; i < num_debug; i++) {
		if (strcasecmp(p, debug_map[i].name) == 0) {
		    val |= debug_map[i].bit;
		    if (pend != NULL) {
			*pend = ',';
			p = pend + 1;
		    }
		    else
			p = "";		/* force termination of outer loop */
		    break;
		}
	    }

	    if (i == num_debug) {
		if (pend != NULL)
		    *pend = ',';
		return PM_ERR_CONV;
	    }
	}
    }

    return val;
#else
    return PM_ERR_NYI;
#endif
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
static FILE	*fptr;
static int	msgsize;
static char	*fname;		/* temporary file name for buffering errors */
static char	*ferr;		/* error output filename from PCP_STDERR */

#define PM_QUERYERR       -1
#define PM_USEDIALOG       0
#define PM_USESTDERR       1
#define PM_USEFILE         2

static int
pmfstate(int state)
{
    static int	errtype = -1;
    char	errmsg[PM_MAXERRMSGLEN];

    if (state > PM_QUERYERR)
	errtype = state;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (errtype == PM_QUERYERR) {
	errtype = PM_USESTDERR;
	if ((ferr = getenv("PCP_STDERR")) != NULL) {
	    if (strcasecmp(ferr, "DISPLAY") == 0) {
		char *xconfirm = pmGetOptionalConfig("PCP_XCONFIRM_PROG");
		if (!xconfirm)
		    fprintf(stderr, "%s: using stderr - no PCP_XCONFIRM_PROG\n",
			    pmProgname);
		else if (access(__pmNativePath(xconfirm), X_OK) < 0)
		    fprintf(stderr, "%s: using stderr - cannot access %s: %s\n",
			    pmProgname, xconfirm, osstrerror_r(errmsg, sizeof(errmsg)));
		else
		    errtype = PM_USEDIALOG;
	    }
	    else if (strcmp(ferr, "") != 0)
		errtype = PM_USEFILE;
	}
    }
    PM_UNLOCK(__pmLock_libpcp);
    return errtype;
}

static int
vpmprintf(const char *msg, va_list arg)
{
    int		lsize = 0;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (fptr == NULL && msgsize == 0) {		/* create scratch file */
	int	fd = -1;
	char	*tmpdir = pmGetOptionalConfig("PCP_TMPFILE_DIR");

	if (tmpdir && tmpdir[0] != '\0') {
	    mode_t cur_umask;

	    /*
	     * PCP_TMPFILE_DIR found in the configuration/environment,
	     * otherwise fall through to the stderr case
	     */

#if HAVE_MKSTEMP
	    fname = (char *)malloc(MAXPATHLEN+1);
	    if (fname == NULL) goto fail;
	    snprintf(fname, MAXPATHLEN, "%s/pcp-XXXXXX", tmpdir);
	    cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
	    fd = mkstemp(fname);
	    umask(cur_umask);
#else
	    fname = tempnam(tmpdir, "pcp-");
	    if (fname == NULL) goto fail;
	    cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
	    fd = open(fname, O_RDWR|O_APPEND|O_CREAT|O_EXCL, 0600);
	    umask(cur_umask);
#endif /* HAVE_MKSTEMP */

	    if (fd < 0) goto fail;
	    if ((fptr = fdopen(fd, "a")) == NULL) {
		char	errmsg[PM_MAXERRMSGLEN];
fail:
		if (fname != NULL) {
		    fprintf(stderr, "%s: vpmprintf: failed to create \"%s\": %s\n",
			pmProgname, fname, osstrerror_r(errmsg, sizeof(errmsg)));
		    unlink(fname);
		    free(fname);
		}
		else {
		    fprintf(stderr, "%s: vpmprintf: failed to create temporary file: %s\n",
			pmProgname, osstrerror_r(errmsg, sizeof(errmsg)));
		}
		fprintf(stderr, "vpmprintf msg:\n");
		if (fd >= 0)
		    close(fd);
		msgsize = -1;
		fptr = NULL;
	    }
	}
	else
	    msgsize = -1;
    }

    if (msgsize < 0) {
	vfprintf(stderr, msg, arg);
	fflush(stderr);
	lsize = 0;
    }
    else
	msgsize += (lsize = vfprintf(fptr, msg, arg));

    PM_UNLOCK(__pmLock_libpcp);
    return lsize;
}

int
pmprintf(const char *msg, ...)
{
    va_list	arg;
    int		lsize;

    va_start(arg, msg);
    lsize = vpmprintf(msg, arg);
    va_end(arg);
    return lsize;
}

int
pmflush(void)
{
    int		sts = 0;
    int		len;
    int		state;
    FILE	*eptr = NULL;
    char	*envptr;
    char	outbuf[MSGBUFLEN];
    char	errmsg[PM_MAXERRMSGLEN];

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (fptr != NULL && msgsize > 0) {
	fflush(fptr);
	state = pmfstate(PM_QUERYERR);
	if (state == PM_USEFILE) {
	    if ((eptr = fopen(ferr, "a")) == NULL) {
		fprintf(stderr, "pmflush: cannot append to file '%s' (from "
			"$PCP_STDERR): %s\n", ferr, osstrerror_r(errmsg, sizeof(errmsg)));
		state = PM_USESTDERR;
	    }
	}
	switch (state) {
	case PM_USESTDERR:
	    rewind(fptr);
	    while ((len = (int)read(fileno(fptr), outbuf, MSGBUFLEN)) > 0) {
		sts = write(fileno(stderr), outbuf, len);
		if (sts != len) {
		    fprintf(stderr, "pmflush: write() failed: %s\n", 
			osstrerror_r(errmsg, sizeof(errmsg)));
		}
		sts = 0;
	    }
	    break;
	case PM_USEDIALOG:
	    /* If we're here, it means xconfirm has passed access test */
	    if ((envptr = pmGetOptionalConfig("PCP_XCONFIRM_PROG")) == NULL) {
		fprintf(stderr, "%s: no PCP_XCONFIRM_PROG\n", pmProgname);
		sts = PM_ERR_GENERIC;
		break;
	    }
	    snprintf(outbuf, sizeof(outbuf), "%s -file %s -c -B OK -icon info"
		    " %s -header 'PCP Information' >/dev/null",
		    __pmNativePath(envptr), fname,
		    (msgsize > 80 ? "-useslider" : ""));
	    if (system(outbuf) < 0) {
		fprintf(stderr, "%s: system failed: %s\n", pmProgname,
			osstrerror_r(errmsg, sizeof(errmsg)));
		sts = -oserror();
	    }
	    break;
	case PM_USEFILE:
	    rewind(fptr);
	    while ((len = (int)read(fileno(fptr), outbuf, MSGBUFLEN)) > 0) {
		sts = write(fileno(eptr), outbuf, len);
		if (sts != len) {
		    fprintf(stderr, "pmflush: write() failed: %s\n", 
			osstrerror_r(errmsg, sizeof(errmsg)));
		}
		sts = 0;
	    }
	    fclose(eptr);
	    break;
	}
	fclose(fptr);
	fptr = NULL;
	unlink(fname);
	free(fname);
	if (sts >= 0)
	    sts = msgsize;
    }

    msgsize = 0;

    PM_UNLOCK(__pmLock_libpcp);
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
    char		*name = "pmcd.client.whoami";
    pmID		pmid;
    int			sts;
    pmResult		store;
    pmValueSet		pmvs;
    pmValueBlock	*pmvb;
    char        	host[MAXHOSTNAMELEN];
    char        	*ipaddr = NULL;
    __pmHostEnt		*servInfo;
    int			vblen;

    if ((sts = pmLookupName(1, &name, &pmid)) < 0)
	return sts;

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
    pmvb = (pmValueBlock *)malloc(PM_VAL_HDR_SIZE+vblen);
    if (pmvb == NULL) {
	__pmNoMem("__pmSetClientId", PM_VAL_HDR_SIZE+vblen, PM_RECOV_ERR);
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
    sts = pmStore(&store);
    free(pmvb);
    return sts;
}

char *
__pmGetClientId(int argc, char **argv)
{
    char	*clientID;
    int		a, need = 0;

    for (a = 0; a < argc; a++)
	need += strlen(argv[a]) + 1;
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
	if (*p == __pmPathSeparator()) {
	    *p = '\0';
	    mkdir2(path, mode);
	    *p = __pmPathSeparator();
	}
    }
    return mkdir2(path, mode);
}

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
 * dirent entries using malloc(3C).  select() and compare() are
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

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if ((dirp = opendir(dirname)) == NULL)
	return -1;

    while ((dp = readdir(dirp)) != NULL) {
	if (select && (*select)(dp) == 0)
	    continue;

	n++;
	if ((names = (struct dirent **)realloc(names, n * sizeof(dp))) == NULL) {
	    PM_UNLOCK(__pmLock_libpcp);
	    closedir(dirp);
	    return -1;
	}

	if ((names[n-1] = tp = (struct dirent *)malloc(
		sizeof(*dp)-sizeof(dp->d_name)+strlen(dp->d_name)+1)) == NULL) {
	    PM_UNLOCK(__pmLock_libpcp);
	    closedir(dirp);
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
    PM_UNLOCK(__pmLock_libpcp);
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
	unsigned lx,ly;

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
    
	n = (hx>>31)+1;

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
#elif defined(IS_FREEBSD)
int 
__pmProcessExists(pid_t pid)
{
    /*
     * kill(.., 0) returns -1 if the process exists.
     */
    if (kill(pid, 0) == -1)
	return 1;
    else
	return 0;
}
#elif defined(HAVE_PROCFS)
#define PROCFS			"/proc"
#define PROCFS_PATH_SIZE	(sizeof(PROCFS)+PROCFS_ENTRY_SIZE)
int 
__pmProcessExists(pid_t pid)
{
    char proc_buf[PROCFS_PATH_SIZE];
    snprintf(proc_buf, sizeof(proc_buf), "%s/%" FMT_PID, PROCFS, pid);
    return (access(proc_buf, F_OK) == 0);
}
#elif !defined(IS_MINGW)
!bozo!
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

#if defined(HAVE_SBRK)
int
__pmProcessDataSize(unsigned long *size)
{
    static void *base;

    if (size && base)
	*size = (sbrk(0) - base) / 1024;
    else {
	base = sbrk(0);
	if (size)
	    *size = 0;
    }
    return 0;
}
#elif !defined(IS_MINGW)
#warning "Platform does not define a process datasize interface?"
int __pmProcessDataSize(unsigned long *) { return -1; }
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
pid_t
__pmProcessCreate(char **argv, int *infd, int *outfd)
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
	*infd = out[0];
	*outfd = in[1];
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

int
__pmSetProgname(const char *program)
{
    char *p;

    /* Trim command name of leading directory components */
    if (program)
	pmProgname = (char *)program;
    for (p = pmProgname; pmProgname && *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }
    return 0;
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

#if HAVE_TRACE_BACK_STACK
#include <libexc.h>
#define MAX_DEPTH 30	/* max callback procedure depth */
#define MAX_SIZE 48	/* max function name length */

void
__pmDumpStack(FILE *f)
{
    __uint64_t	call_addr[MAX_DEPTH];
    char	*call_fn[MAX_DEPTH];
    char	names[MAX_DEPTH][MAX_SIZE];
    int		res;
    int		i;

    for (i = 0; i < MAX_DEPTH; i++)
	call_fn[i] = names[i];
    res = trace_back_stack(MAX_DEPTH, call_addr, call_fn, MAX_DEPTH, MAX_SIZE);
    for (i = 1; i < res; i++) {
#if defined(HAVE_64BIT_PTR)
	fprintf(f, "  0x%016llx [%s]\n", call_addr[i], call_fn[i]);
#else
	fprintf(f, "  0x%08lx [%s]\n", (__uint32_t)call_addr[i], call_fn[i]);
#endif
    }
}

#elif HAVE_BACKTRACE
#include <execinfo.h>
#define MAX_DEPTH 30	/* max callback procedure depth */

void
__pmDumpStack(FILE *f)
{
    int		nframe;
    void	*buf[MAX_DEPTH];
    char	**symbols;
    int		i;

    nframe = backtrace(buf, MAX_DEPTH);
    if (nframe < 1) {
	fprintf(f, "backtrace -> %d frames?\n", nframe);
	return;
    }
    symbols = backtrace_symbols(buf, nframe);
    if (symbols == NULL) {
	fprintf(f, "backtrace_symbols failed!\n");
	return;
    }
    for (i = 1; i < nframe; i++)
	fprintf(f, "  " PRINTF_P_PFX "%p [%s]\n", buf[i], symbols[i]);
}
#else	/* no known mechanism, provide a stub (called unconditionally) */
void
__pmDumpStack(FILE *f)
{
    fprintf(f, "[No backtrace support available]\n");
}
#endif /* HAVE_BACKTRACE */

#endif /* !IS_MINGW */
