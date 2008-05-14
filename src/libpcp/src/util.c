/*
 * General Utility Routines
 *
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h> 
#include <dirent.h> 
#include <fcntl.h>
#include <time.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>

#include "pmapi.h"
#include "impl.h"
#include "pmdbg.h"

#if defined(HAVE_IEEEFP_H)
#include <ieeefp.h>
#endif

static FILE	**filelog = NULL;
static int	nfilelog = 0;
static int	dosyslog = 0;
static int	pmState = PM_STATE_APPL;
static int	done_exit = 0;

char		*pmProgname = "pcp";		/* the real McCoy */

static int vpmprintf(const char *, va_list);

/*
 * if onoff == 1, logging is to syslog and stderr, else logging is
 * just to stderr (this is the default)
 */
void
__pmSyslog(int onoff)
{
    dosyslog = onoff;
    if (dosyslog)
	openlog("pcp", LOG_PID, LOG_DAEMON);
    else
	closelog();
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

    if (dosyslog) {
	char	syslogmsg[2048];

	snprintf(syslogmsg, sizeof(syslogmsg), message, arg);
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

    pmprintf("[%.19s] %s(%d) %s: ", ctime(&now), pmProgname, getpid(), level);
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
    time_t	now;
    char	host[MAXHOSTNAMELEN];

    setlinebuf(log);		/* line buffering for log files */
    gethostname(host, MAXHOSTNAMELEN);
    host[MAXHOSTNAMELEN-1] = '\0';
    time(&now);
    fprintf(log, "Log for %s on %s %s %s\n", progname, host, act, ctime(&now));
}

static void
logfooter(FILE *log, const char *act)
{
    time_t	now;

    time(&now);
    fprintf(log, "\nLog %s %s", act, ctime(&now));
}

static void
logonexit(void)
{
    int		i;

    /*
     * there is a race condition here ... but the worse that can happen
     * is (a) no "Log finished" message, or (b) _two_ "Log finished"
     * messages ... neither case is serious enough to warrant a mutex guard
     */
    if (++done_exit != 1)
	return;

    for (i = 0; i < nfilelog; i++)
	logfooter(filelog[i], "finished");
}

/* common code shared by __pmRotateLog and __pmOpenLog */
static FILE *
logreopen(const char *progname, const char *logname, FILE *oldstream,
	    int *status)
{
    int		oldfd;
    int		dupoldfd;
    FILE	*dupoldstream = oldstream;
    extern int	errno;

    /*
     * Do our own version of freopen() because the standard one closes the
     * original stream BEFORE trying to open the new one.  Once it's gone,
     * there's no way to get the closed stream back if the open fails.
     */

    fflush(oldstream);
    oldfd = fileno(oldstream);
    dupoldfd = dup(oldfd);

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
	int	save_errno = errno;	/* need for error message */
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
	*status = 0;
	pmprintf("%s: cannot open log \"%s\" for writing : %s\n",
		progname, logname, strerror(save_errno));
	pmflush();
    }
    else {
	*status = 1;
    }
    close(dupoldfd);
    return oldstream;
}

FILE *
__pmOpenLog(const char *progname, const char *logname, FILE *oldstream,
	    int *status)
{
    oldstream = logreopen(progname, logname, oldstream, status);
    logheader(progname, oldstream, "started");

    nfilelog++;
    if (nfilelog == 1)
	atexit(logonexit);

    filelog = (FILE **)realloc(filelog, nfilelog * sizeof(FILE *));
    if (filelog == NULL) {
	__pmNoMem("__pmOpenLog", nfilelog * sizeof(FILE *), PM_FATAL_ERR);
    }
    filelog[nfilelog-1] = oldstream;
    return oldstream;
}

FILE *
__pmRotateLog(const char *progname, const char *logname, FILE *oldstream,
	    int *status)
{
    int		i;

    for (i = 0; i < nfilelog; i++) {
	if (oldstream == filelog[i]) {
	    logfooter(oldstream, "rotated");	/* old */
	    oldstream = logreopen(progname, logname, oldstream, status);
	    logheader(progname, oldstream, "rotated");	/* new */
	    filelog[i] = oldstream;
	    break;
	}
    }
    return oldstream;
}

const char *
pmIDStr(pmID pmid)
{
    static char	pbuf[20];
    __pmID_int*	p = (__pmID_int*)&pmid;
    if (pmid == PM_ID_NULL)
	return "PM_ID_NULL";
    snprintf(pbuf, sizeof(pbuf), "%d.%d.%d", p->domain, p->cluster, p->item);
    return pbuf;
}

const char *
pmInDomStr(pmInDom indom)
{
    static char	pbuf[20];
    __pmInDom_int*	p = (__pmInDom_int*)&indom;
    if (indom == PM_INDOM_NULL)
	return "PM_INDOM_NULL";
    snprintf(pbuf, sizeof(pbuf), "%d.%d", p->domain, p->serial);
    return pbuf;
}

const char *
pmNumberStr(double value)
{
    static char buf[8];

    if (value >= 0.0) {
	if (value >= 999995000000000.0)
	    strncpy(buf, " inf? ", sizeof(buf));
	else if (value >= 999995000000.0)
	    snprintf(buf, sizeof(buf), "%6.2fT", value / 1000000000000.0);
	else if (value >= 999995000.0)
	    snprintf(buf, sizeof(buf), "%6.2fG", value / 1000000000.0);
	else if (value >= 999995.0)
	    snprintf(buf, sizeof(buf), "%6.2fM", value / 1000000.0);
	else if (value >= 999.995)
	    snprintf(buf, sizeof(buf), "%6.2fK", value / 1000.0);
	else if (value >= 0.005)
	    snprintf(buf, sizeof(buf), "%6.2f ", value);
	else
	    snprintf(buf, sizeof(buf), "%6.2f ", 0.0);
    }
    else {
	if (value <= -99995000000000.0)
	    strncpy(buf, "-inf?  ", sizeof(buf));
	else if (value <= -99995000000.0)
	    snprintf(buf, sizeof(buf), "%6.2fT", value / 1000000000000.0);
	else if (value <= -99995000.0)
	    snprintf(buf, sizeof(buf), "%6.2fG", value / 1000000000.0);
	else if (value <= -99995.0)
	    snprintf(buf, sizeof(buf), "%6.2fM", value / 1000000.0);
	else if (value <= -99.995)
	    snprintf(buf, sizeof(buf), "%6.2fK", value / 1000.0);
	else if (value <= -0.005)
	    snprintf(buf, sizeof(buf), "%6.2f ", value);
	else
	    snprintf(buf, sizeof(buf), "%6.2f ", 0.0);
    }
    return buf;
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
	n = pmNameID(vsp->pmid, &p);
	if (n < 0)
	    fprintf(f,"  %s (%s):", pmIDStr(vsp->pmid), "<noname>");
	else {
	    fprintf(f,"  %s (%s):", pmIDStr(vsp->pmid), p);
	    free(p);
	}
	if (vsp->numval == 0) {
	    fprintf(f, " No values returned!\n");
	    continue;
	}
	else if (vsp->numval < 0) {
	    fprintf(f, " %s\n", pmErrStr(vsp->numval));
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

    if (type != PM_TYPE_UNKNOWN) {
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
        fprintf(f, "%*lli", minwidth, (long long)a.ll);
        break;

    case PM_TYPE_U64:
        fprintf(f, "%*llu", minwidth, (unsigned long long)a.ull);
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
	    fprintf(f, "%*u", minwidth, *ip);
#ifdef HAVE_ISNANF
	    if (!isnanf(*fp))
#endif
		fprintf(f, " %*.8g", minwidth, (double)*fp);
	    if (minwidth > 2)
		minwidth -= 2;
	    fprintf(f, " 0x%*x", minwidth, val->value.lval);
	}
	else {
	    int		string;
	    int		done = 0;
	    if (val->value.pval->vlen == PM_VAL_HDR_SIZE + sizeof(__uint64_t)) {
		__uint64_t	i;
		memcpy((void *)&i, (void *)&val->value.pval->vbuf, sizeof(__uint64_t));
		fprintf(f, "%*llu", minwidth, (unsigned long long)i);
		done = 1;
	    }
	    if (val->value.pval->vlen == PM_VAL_HDR_SIZE + sizeof(double)) {
		double	d;
		memcpy((void *)&d, (void *)&val->value.pval->vbuf, sizeof(double));
		if (!isnand(d)) {
		    if (done) fputc(' ', f);
		    fprintf(f, "%*.16g", minwidth, d);
		    done = 1;
		}
	    }
	    if (val->value.pval->vlen == PM_VAL_HDR_SIZE + sizeof(float)) {
		float	*fp = (float *)&val->value.pval->vbuf;
#ifdef HAVE_ISNANF
		if (!isnanf(*fp)) {
#endif
		    if (done) fputc(' ', f);
		    fprintf(f, "%*.8g", minwidth, (double)*fp);
		    done = 1;
#ifdef HAVE_ISNANF
		}
#endif
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
	    free(a.vp);
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
    extern int	errno;

    __pmNotifyErr(fatal ? LOG_ERR : LOG_WARNING,
		 "%s: malloc(%d) failed: %s", where, size, strerror(errno));
    if (fatal)
	exit(1);
}

/*
 * this one is used just below the PMAPI to convert some Unix errors
 * into more appropriate PMAPI error codes
 */
int
__pmMapErrno(int sts)
{
    if (sts == -EBADF || sts == -EPIPE)
	sts = PM_ERR_IPC;
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
 * timestamp, e.g. from a log in HH:MM:SS.XXX format
 */
void
__pmPrintStamp(FILE *f, const struct timeval *tp)
{
    static struct tm	tmp;
    time_t		now;

    now = (time_t)tp->tv_sec;
    pmLocaltime(&now, &tmp);
    fprintf(f, "%02d:%02d:%02d.%03d", tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)(tp->tv_usec/1000));
}

/*
 * descriptor
 */
void
__pmPrintDesc(FILE *f, const pmDesc *desc)
{
    char	*type;
    char	*sem;
    int		sem_ok;
    static char	*unknownVal = "???";

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
	default:
	    type = unknownVal;
	    break;
    }

    sem_ok = 1;
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
	    sem_ok = 0;
	    break;
    }

    fprintf(f, "    Data Type: %s  InDom: %s 0x%x\n", type, pmInDomStr(desc->indom), desc->indom);
    fprintf(f, "    Semantics: %s  Units: ", sem);
    if (sem_ok) {
	const char	*units = pmUnitsStr(&desc->units);
	if (*units == '\0')
	    fprintf(f, "none\n");
	else
	    fprintf(f, "%s\n", units);
    }
    else {
	int	*ip = (int *)&desc->units;
	fprintf(f, "0x%x???\n", *ip);
    }
}

/*
 * print times between events
 */
void
__pmEventTrace(const char *event)
{
#ifdef PCP_DEBUG
    static double last = 0;
    static double sum = 0;
    static int first = 1;
    struct timeval tv;
    double now;

    gettimeofday(&tv, NULL);
    now = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
    if (!first)
        sum += now - last;
    fprintf(stderr, "%s: +%4.2f = %4.2f -> %s\n",
			pmProgname, first ? 0 : (now-last), sum, event);
    last = now;
    first = 0;
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
static char	outbuf[MSGBUFLEN];
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

    if (errtype == PM_QUERYERR) {
	errtype = PM_USESTDERR;
	if ((ferr = getenv("PCP_STDERR")) != NULL) {
	    if (strcasecmp(ferr, "DISPLAY") == 0) {
		char * xconfirm = pmGetConfig("PCP_XCONFIRM_PROG");
		if (access(xconfirm, X_OK) < 0) {
		    fprintf(stderr, "%s: using stderr - cannot access %s: %s\n",
			    pmProgname, xconfirm, strerror(errno));
		}
		else
		    errtype = PM_USEDIALOG;
	    }
	    else if (strcmp(ferr, "") != 0)
		errtype = PM_USEFILE;
	}
    }
    return errtype;
}

static int
vpmprintf(const char *msg, va_list arg)
{
    int		lsize = 0;

    if (fptr == NULL && msgsize == 0) {		/* create scratch file */
	int	fd = -1;

	if ((fname = tmpnam(NULL)) == NULL ||
	    (fd = open(fname, O_RDWR|O_APPEND|O_CREAT|O_EXCL, 0600)) < 0 ||
	    (fptr = fdopen(fd, "a")) == NULL) {
	    fprintf(stderr, "%s: vpmprintf: failed to create \"%s\": %s\n",
		pmProgname, fname, strerror(errno));
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
    int		state;
    FILE	*eptr;

    if (fptr != NULL && msgsize > 0) {
	fflush(fptr);
	state = pmfstate(PM_QUERYERR);
	if (state == PM_USEFILE) {
	    if ((eptr = fopen(ferr, "a")) == NULL) {
		fprintf(stderr, "pmflush: cannot append to file '%s' (from "
			"$PCP_STDERR): %s\n", ferr, strerror(errno));
		state = PM_USESTDERR;
	    }
	}
	switch (state) {
	case PM_USESTDERR:
	    rewind(fptr);
	    while ((sts = (int)read(fileno(fptr), outbuf, MSGBUFLEN)) > 0)
		write(fileno(stderr), outbuf, sts);
	    break;
	case PM_USEDIALOG:
	    /* If we're here, it means xconfirm has passed access test */
	    snprintf(outbuf, sizeof(outbuf), "%s -file %s -c -B OK -icon info"
		    " %s -header 'PCP Information' >/dev/null",
		    pmGetConfig("PCP_XCONFIRM_PROG"), fname,
		    (msgsize > 80 ? "-useslider" : ""));
	    if (system(outbuf) < 0) {
		fprintf(stderr, "%s: system failed: %s\n", pmProgname,
			strerror(errno));
		sts = -errno;
	    }
	    break;
	case PM_USEFILE:
	    rewind(fptr);
	    while ((sts = (int)read(fileno(fptr), outbuf, MSGBUFLEN)) > 0)
		write(fileno(eptr), outbuf, sts);
	    fclose(eptr);
	    break;
	}
	fclose(fptr);
	fptr = NULL;
	unlink(fname);
	if (sts >= 0)
	    sts = msgsize;
    }

    msgsize = 0;

    return sts;
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
    static char	*dot = ".";

    if (p == NULL)
	return(dot);
    else {
	*p = '\0';
	return(name);
    }
}
#endif

#ifndef HAVE_ISNAND
int
isnand(double d)
{
#ifdef HAVE_ISNANF
    float	f = (float)d;
    /* not exact, but the best we can do! */
    return(isnanf(f));
#else
    /* no support, assume is _not_ NAN, i.e. OK */
    return(0);
#endif
}
#endif

#ifndef HAVE_UNSETENV
static int
unsetenv(const char *name)
{
    extern char **_environ;
    char	**ep;
    int		len = (int)strlen(name);
    int		found = 0;

    for (ep = _environ; *ep != NULL; ep++) {
	if (strncmp(*ep, name, len) == 0 && (*ep)[len] == '=') {
	    found = 1;
	}
	if (found)
	    ep[0] = ep[1];
    }
    return found;
}
#endif

#ifndef HAVE_SCANDIR
/*
 * Scan the directory dirname, building an array of pointers to
 * dirent entries using malloc(3C).  select() and dcomp() are used
 * optionally filter and sort directory entries.
 */
int
scandir(const char *dirname, struct dirent ***namelist,
	int(*select)(MYDIRENT *), int(*dcomp)(MYDIRENT **, MYDIRENT **))
{
    DIR			*dirp;
    int			n = 0;
    struct dirent	**names = NULL;
    struct dirent	*dp;
    struct dirent	*tp;

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
	tp->d_off = dp->d_off;
	tp->d_reclen = dp->d_reclen;
	memcpy(tp->d_name, dp->d_name, strlen(dp->d_name)+1);
    }
    closedir(dirp);
    *namelist = names;

    if (n && dcomp)
	qsort(names, n, sizeof(names[0]),
			(int(*)(const void *, const void *))dcomp);
    return n;
}

#if defined(HAVE_CONST_DIRENT)
#define MYDIRENT const struct dirent
#else
#define MYDIRENT struct dirent
#endif

/* 
 * Alphabetical sort for default use
 */
int
alphasort(MYDIRENT **p, MYDIRENT **q)
{
    return strcmp((*p)->d_name, (*q)->d_name);
}
#endif
