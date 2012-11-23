/*
 * General Utility Routines
 *
 * Copyright (c) 1995-2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
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
#include <math.h>

#include "pmapi.h"
#include "impl.h"
#include "pmdbg.h"

#if defined(HAVE_SYS_TIMES_H)
#include <sys/times.h>
#endif
#if defined(HAVE_SYS_MMAN_H)
#include <sys/mman.h> 
#endif
#if defined(HAVE_IEEEFP_H)
#include <ieeefp.h>
#endif
#if defined(HAVE_PWD_H)
#include <pwd.h>
#endif

static FILE	**filelog;
static int	nfilelog;
static int	dosyslog;
static int	pmState = PM_STATE_APPL;
static int	done_exit;

INTERN char	*pmProgname = "pcp";		/* the real McCoy */

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

	snprintf(syslogmsg, sizeof(syslogmsg), message, arg);
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
	    if (dup(dupoldfd) != oldfd)
		/* fd juggling failed! */
		oldstream = NULL;
	    else
		/* oldfd now re-instated as at entry */
		oldstream = fdopen(oldfd, "w");
	    if (oldstream == NULL) {
		/* serious trouble ... choose least obnoxious alternative */
		if (dupoldstream == stderr)
		    oldstream = fdopen(fileno(stdout), "w");
		else
		    oldstream = fdopen(fileno(stderr), "w");
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
	    snprintf(buf, buflen, "%6.2fT", value / 1000000000000.0);
	else if (value >= 999995000.0)
	    snprintf(buf, buflen, "%6.2fG", value / 1000000000.0);
	else if (value >= 999995.0)
	    snprintf(buf, buflen, "%6.2fM", value / 1000000.0);
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
	    snprintf(buf, buflen, "%6.2fT", value / 1000000000000.0);
	else if (value <= -99995000.0)
	    snprintf(buf, buflen, "%6.2fG", value / 1000000000.0);
	else if (value <= -99995.0)
	    snprintf(buf, buflen, "%6.2fM", value / 1000000.0);
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

void
__pmDumpResult(FILE *f, const pmResult *resp)
{
    int		i;
    int		j;
    int		n;
    int		saveDebug;
    char	*p;
    pmDesc	desc;
    int		have_desc;

    /* tracing PDUs really messes this up when pmNameInDom is called below */
    saveDebug = pmDebug;
    pmDebug = 0;

    fprintf(f,"pmResult dump from " PRINTF_P_PFX "%p timestamp: %d.%06d ",
        resp, (int)resp->timestamp.tv_sec, (int)resp->timestamp.tv_usec);
    __pmPrintStamp(f, &resp->timestamp);
    fprintf(f, " numpmid: %d\n", resp->numpmid);
    for (i = 0; i < resp->numpmid; i++) {
	pmValueSet	*vsp = resp->vset[i];
	char		strbuf[20];
	n = pmNameID(vsp->pmid, &p);
	if (n < 0)
	    fprintf(f,"  %s (%s):", pmIDStr_r(vsp->pmid, strbuf, sizeof(strbuf)), "<noname>");
	else {
	    fprintf(f,"  %s (%s):", pmIDStr_r(vsp->pmid, strbuf, sizeof(strbuf)), p);
	    free(p);
	}
	if (vsp->numval == 0) {
	    fprintf(f, " No values returned!\n");
	    continue;
	}
	else if (vsp->numval < 0) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(f, " %s\n", pmErrStr_r(vsp->numval, errmsg, sizeof(errmsg)));
	    continue;
	}
	if (__pmGetInternalState() == PM_STATE_PMCS || pmLookupDesc(vsp->pmid, &desc) < 0) {
	    /* don't know, so punt on the most common cases */
	    desc.indom = PM_INDOM_NULL;
	    have_desc = 0;
	}
	else
	    have_desc = 1;
	fprintf(f, " numval: %d", vsp->numval);
	fprintf(f, " valfmt: %d vlist[]:\n", vsp->valfmt);
	for (j = 0; j < vsp->numval; j++) {
	    pmValue	*vp = &vsp->vlist[j];
	    if (vsp->numval > 1 || vp->inst != PM_INDOM_NULL) {
		fprintf(f,"    inst [%d", vp->inst);
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
    pmDebug = saveDebug;
}

static void
print_event_summary(FILE *f, const pmValue *val)
{
    pmEventArray	*eap = (pmEventArray *)val->value.pval;
    char		*base;
    struct timeval	stamp;
    __pmTimeval		*tvp;
    int			nrecords;
    int			nmissed = 0;
    int			r;	/* records */
    int			p;	/* parameters in a record ... */
    pmEventRecord	*erp;
    pmEventParameter	*epp;

    nrecords = eap->ea_nrecords;
    base = (char *)&eap->ea_record[0];
    tvp = (__pmTimeval *)base;
    stamp.tv_sec = tvp->tv_sec;
    stamp.tv_usec = tvp->tv_usec;
    /* walk packed event record array */
    for (r = 0; r < eap->ea_nrecords-1; r++) {
	erp = (pmEventRecord *)base;
	base += sizeof(erp->er_timestamp) + sizeof(erp->er_flags) + sizeof(erp->er_nparams);
	if (erp->er_flags & PM_EVENT_FLAG_MISSED) {
	    nmissed += erp->er_nparams;
	    continue;
	}
	for (p = 0; p < erp->er_nparams; p++) {
	    epp = (pmEventParameter *)base;
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
	__pmPrintStamp(f, &stamp);
	if (eap->ea_nrecords > 1) {
	    fprintf(f, "...");
	    tvp = (__pmTimeval *)base;
	    stamp.tv_sec = tvp->tv_sec;
	    stamp.tv_usec = tvp->tv_usec;
	    __pmPrintStamp(f, &stamp);
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

    if (type != PM_TYPE_UNKNOWN && type != PM_TYPE_EVENT) {
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
	    print_event_summary(f, val);
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

/*
 * difference for two on the internal timestamps
 */
double
__pmTimevalSub(const __pmTimeval *ap, const __pmTimeval *bp)
{
     return ap->tv_sec - bp->tv_sec + (double)(ap->tv_usec - bp->tv_usec)/1000000.0;
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
	default:
	    type = unknownVal;
	    break;
    }
    fprintf(f, "    Data Type: %s", type);
    if (type == unknownVal)
	fprintf(f, " (%d)", desc->type);

    fprintf(f,"  InDom: %s 0x%x\n", pmInDomStr_r(desc->indom, strbuf, sizeof(strbuf)), desc->indom);

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
    now = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
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
static FILE	*fptr = NULL;
static int	msgsize = 0;
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

    if (state > PM_QUERYERR)
	errtype = state;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (errtype == PM_QUERYERR) {
	errtype = PM_USESTDERR;
	if ((ferr = getenv("PCP_STDERR")) != NULL) {
	    if (strcasecmp(ferr, "DISPLAY") == 0) {
		char * xconfirm = pmGetConfig("PCP_XCONFIRM_PROG");
		if (access(__pmNativePath(xconfirm), X_OK) < 0) {
		    char	errmsg[PM_MAXERRMSGLEN];
		    fprintf(stderr, "%s: using stderr - cannot access %s: %s\n",
			    pmProgname, xconfirm, osstrerror_r(errmsg, sizeof(errmsg)));
		}
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

	fname = tempnam(pmGetConfig("PCP_TMP_DIR"), "pcp-");
	if (fname == NULL ||
	    (fd = open(fname, O_RDWR|O_APPEND|O_CREAT|O_EXCL, 0600)) < 0 ||
	    (fptr = fdopen(fd, "a")) == NULL) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s: vpmprintf: failed to create \"%s\": %s\n",
		pmProgname, fname, osstrerror_r(errmsg, sizeof(errmsg)));
	    fprintf(stderr, "vpmprintf msg:\n");
	    if (fd != -1)
		close(fd);
	    msgsize = -1;
	}
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
    char	outbuf[MSGBUFLEN];

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (fptr != NULL && msgsize > 0) {
	fflush(fptr);
	state = pmfstate(PM_QUERYERR);
	if (state == PM_USEFILE) {
	    if ((eptr = fopen(ferr, "a")) == NULL) {
		char	errmsg[PM_MAXERRMSGLEN];
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
		    char	errmsg[PM_MAXERRMSGLEN];
		    fprintf(stderr, "pmflush: write() failed: %s\n", 
			osstrerror_r(errmsg, sizeof(errmsg)));
		}
		sts = 0;
	    }
	    break;
	case PM_USEDIALOG:
	    /* If we're here, it means xconfirm has passed access test */
	    snprintf(outbuf, sizeof(outbuf), "%s -file %s -c -B OK -icon info"
		    " %s -header 'PCP Information' >/dev/null",
		    __pmNativePath(pmGetConfig("PCP_XCONFIRM_PROG")), fname,
		    (msgsize > 80 ? "-useslider" : ""));
	    if (system(outbuf) < 0) {
		char	errmsg[PM_MAXERRMSGLEN];
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
		    char	errmsg[PM_MAXERRMSGLEN];
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
    pmResult		store = { .numpmid = 1 };
    pmValueSet		pmvs;
    pmValueBlock	*pmvb;
    char        	host[MAXHOSTNAMELEN];
    char        	ipaddr[16] = "";	/* IPv4 xxx.xxx.xxx.xxx */
    struct hostent      *hep = NULL;
    int			vblen;

    if ((sts = pmLookupName(1, &name, &pmid)) < 0)
	return sts;

    (void)gethostname(host, MAXHOSTNAMELEN);
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    hep = gethostbyname(host);
    if (hep != NULL) {
	strcpy(host, hep->h_name);
	if (hep->h_addrtype == AF_INET) {
	    strcpy(ipaddr, inet_ntoa(*((struct in_addr *)hep->h_addr_list[0])));
	}
	vblen = strlen(host) + strlen(ipaddr) + strlen(id) + 5;
    }
    else
	vblen = strlen(host) + strlen(id) + 2;
    PM_UNLOCK(__pmLock_libpcp);

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
    if (ipaddr[0] != '\0') {
	strcat(pmvb->vbuf, "(");
	strcat(pmvb->vbuf, ipaddr);
	strcat(pmvb->vbuf, ") ");
    }
    strcat(pmvb->vbuf, id);

    pmvs.pmid = pmid;
    pmvs.numval = 1;
    pmvs.valfmt = PM_VAL_SPTR;
    pmvs.vlist[0].value.pval = pmvb;
    pmvs.vlist[0].inst = PM_IN_NULL;

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
 * copyright or licensing issues.
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
#endif

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
#endif

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
	if ((names = (struct dirent **)realloc(names, n * sizeof(dp))) == NULL)
	    return -1;

	if ((names[n-1] = tp = (struct dirent *)malloc(
		sizeof(*dp)-sizeof(dp->d_name)+strlen(dp->d_name)+1)) == NULL)
	    return -1;

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
#endif

#define PROCFS_ENTRY_SIZE 40	/* encompass any size of entry for pid */

#if defined(IS_DARWIN)	/* No procfs on Mac OS X */
#include <sys/sysctl.h>
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
__pmSetProcessIdentity(const char *username)
{
    gid_t gid;
    uid_t uid;
    struct passwd *pw;

#if defined(HAVE_GETPWNAM_R)	/* thread-safe variant first */
    struct passwd pwd;
    char buf[16*1024];
    int sts;

    sts = getpwnam_r(username, &pwd, buf, sizeof(buf), &pw);
    if (pw == NULL) {
	__pmNotifyErr(LOG_CRIT,
		"cannot find the %s user to switch to\n", username);
	exit(1);
    }
    uid = pwd.pw_uid;
    gid = pwd.pw_gid;
#elif defined(HAVE_GETPWNAM)
    if ((pw = getpwnam(username)) == 0) {
	__pmNotifyErr(LOG_CRIT,
		"cannot find the %s user to switch to\n", username);
	exit(1);
    }
    uid = pw->pw_uid;
    gid = pw->pw_gid;
#else
!bozo!
#endif

    if (setgid(gid) < 0 || setuid(uid) < 0) {
	__pmNotifyErr(LOG_CRIT,
		"cannot switch to uid/gid of %s user (%d/%d)\n", username, uid, gid);
	exit(1);
    }

    return 0;
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
    char	*p;

    /* Trim command name of leading directory components */
    if (program)
	pmProgname = (char *)program;
    for (p = pmProgname; pmProgname && *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }
    return 0;
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
#endif
