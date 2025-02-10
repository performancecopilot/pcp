/*
 * Copyright (c) 2013-2019,2022 Red Hat.
 * Copyright (c) 1995-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "libpcp.h"
#include "fault.h"
#include "internal.h"
#include <ctype.h>
#ifdef HAVE_SECURE_SOCKETS
#include <sasl/sasl.h>
#endif

#if defined(PM_MULTI_THREAD) && !defined(IS_MINGW)
static pthread_mutex_t	err_lock = PTHREAD_MUTEX_INITIALIZER;
#else
void			*err_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == err_lock
 */
int
__pmIsErrLock(void *lock)
{
    return lock == (void *)&err_lock;
}
#endif

/*
 * Note:
 *     These error codes must match the errors defined in pmapi.h
 *     although the ordering here may be different to try and group
 *     related errors together.
 *
 *     All of the errors in pmapi.h and here are also defined for
 *     Perl applications in src/perl/PMDA/PMDA.pm (in 2 places) and
 *     for Python applications in src/python/pmapi.c.
 *
 *     To ease maintenance effort we aim to keep the _order_ of the
 *     error codes the same here and in the Perl and Python
 *     definitions.
 *
 *     And finally if you modify this table at all, be sure to check
 *     for remakes in the QA suite
 *	   $ make; sudo make install
 *         $ cd ../../pmerr; make; sudo make install
 *         $ cd ../../qa; ./check -g pmerr
 *     Expect (at least) that QA 006.out will need to be remade.
 * 
 */
static const struct {
    int  	err;
    char	*symb;
    char	*errmess;
} errtab[] = {
    { PM_ERR_GENERIC,		"PM_ERR_GENERIC",
	"Generic error, already reported above" },
    { PM_ERR_PMNS,		"PM_ERR_PMNS",
	"Problems parsing PMNS definitions" },
    { PM_ERR_NOPMNS,		"PM_ERR_NOPMNS",
	"PMNS not accessible" },
    { PM_ERR_DUPPMNS,		"PM_ERR_DUPPMNS",
	"Attempt to reload the PMNS" },
    { PM_ERR_TEXT,		"PM_ERR_TEXT",
	"One-line or help text is not available" },
    { PM_ERR_APPVERSION,	"PM_ERR_APPVERSION",
	"Metric not supported by this version of monitored application" },
    { PM_ERR_VALUE,		"PM_ERR_VALUE",
	"Missing metric value(s)" },
    { PM_ERR_TIMEOUT,		"PM_ERR_TIMEOUT",
	"Timeout waiting for a response from PMCD" },
    { PM_ERR_NODATA,		"PM_ERR_NODATA",
	"Empty archive file" },
    { PM_ERR_RESET,		"PM_ERR_RESET",
	"PMCD reset or configuration change" },
    { PM_ERR_NAME,		"PM_ERR_NAME",
	"Unknown metric name" },
    { PM_ERR_PMID,		"PM_ERR_PMID",
	"Unknown or illegal metric identifier" },
    { PM_ERR_INDOM,		"PM_ERR_INDOM",
	"Unknown or illegal instance domain identifier" },
    { PM_ERR_INST,		"PM_ERR_INST",
	"Unknown or illegal instance identifier" },
    { PM_ERR_TYPE,		"PM_ERR_TYPE",
	"Unknown or illegal metric type" },
    { PM_ERR_UNIT,		"PM_ERR_UNIT",
	"Illegal pmUnits specification" },
    { PM_ERR_CONV,		"PM_ERR_CONV",
	"Impossible value or scale conversion" },
    { PM_ERR_TRUNC,		"PM_ERR_TRUNC",
	"Truncation in value conversion" },
    { PM_ERR_SIGN,		"PM_ERR_SIGN",
	"Negative value in conversion to unsigned" },
    { PM_ERR_PROFILE,		"PM_ERR_PROFILE",
	"Explicit instance identifier(s) required" },
    { PM_ERR_IPC,		"PM_ERR_IPC",
	"IPC protocol failure" },
    { PM_ERR_TLS,		"PM_ERR_TLS",
	"TLS protocol failure" },
    { PM_ERR_EOF,		"PM_ERR_EOF",
	"IPC channel closed" },
    { PM_ERR_NOTHOST,		"PM_ERR_NOTHOST",
	"Operation requires context with host source of metrics" },
    { PM_ERR_EOL,		"PM_ERR_EOL",
	"End of PCP archive" },
    { PM_ERR_MODE,		"PM_ERR_MODE",
	"Illegal mode specification" },
    { PM_ERR_LABEL,		"PM_ERR_LABEL",
	"Illegal label record at start of a PCP archive file" },
    { PM_ERR_LOGREC,		"PM_ERR_LOGREC",
	"Corrupted record in a PCP archive" },
    { PM_ERR_LOGFILE,		"PM_ERR_LOGFILE",
	"Missing PCP archive file" },
    { PM_ERR_NOTARCHIVE,	"PM_ERR_NOTARCHIVE",
	"Operation requires context with archive source of metrics" },
    { PM_ERR_NOCONTEXT,		"PM_ERR_NOCONTEXT",
	"Attempt to use an illegal context" },
    { PM_ERR_PROFILESPEC,	"PM_ERR_PROFILESPEC",
	"NULL pmInDom with non-NULL instlist" },
    { PM_ERR_PMID_LOG,		"PM_ERR_PMID_LOG",
	"Metric not defined in the PCP archive" },
    { PM_ERR_INDOM_LOG,		"PM_ERR_INDOM_LOG",
	"Instance domain identifier not defined in the PCP archive" },
    { PM_ERR_INST_LOG,		"PM_ERR_INST_LOG",
	"Instance identifier not defined in the PCP archive" },
    { PM_ERR_NOPROFILE,		"PM_ERR_NOPROFILE",
	"Missing profile - protocol botch" },
    { PM_ERR_NOAGENT,		"PM_ERR_NOAGENT",
	"No PMCD agent for domain of request" },
    { PM_ERR_PERMISSION,	"PM_ERR_PERMISSION",
	"No permission to perform requested operation" },
    { PM_ERR_CONNLIMIT,		"PM_ERR_CONNLIMIT",
	"PMCD connection limit for this host exceeded" },
    { PM_ERR_AGAIN,		"PM_ERR_AGAIN",
	"Try again. Information not currently available" },
    { PM_ERR_ISCONN,		"PM_ERR_ISCONN",
	"Already Connected" },
    { PM_ERR_NOTCONN,		"PM_ERR_NOTCONN",
	"Not Connected" },
    { PM_ERR_NEEDPORT,		"PM_ERR_NEEDPORT",
	"A non-null port name is required" },
    { PM_ERR_NONLEAF,		"PM_ERR_NONLEAF",
	"Metric name is not a leaf in PMNS" },
    { PM_ERR_PMDANOTREADY,	"PM_ERR_PMDANOTREADY",
	"PMDA is not yet ready to respond to requests" },
    { PM_ERR_PMDAREADY,		"PM_ERR_PMDAREADY",
	"PMDA is now responsive to requests" },
    { PM_ERR_BOTCH,		"PM_ERR_BOTCH",
	"Internal inconsistency detected or assertion failed" },
    { PM_ERR_TOOSMALL,		"PM_ERR_TOOSMALL",
	"Insufficient elements in list" },
    { PM_ERR_TOOBIG,		"PM_ERR_TOOBIG",
	"Result size exceeded" },
    { PM_ERR_FAULT,		"PM_ERR_FAULT",
	"QA fault injected" },
    { PM_ERR_THREAD,		"PM_ERR_THREAD",
	"Operation not supported for multi-threaded applications" },
    { PM_ERR_NOCONTAINER,	"PM_ERR_NOCONTAINER",
	"Container not found" },
    { PM_ERR_BADSTORE,		"PM_ERR_BADSTORE",
	"Bad input to pmstore" },
    { PM_ERR_LOGOVERLAP,	"PM_ERR_LOGOVERLAP",
	"Archives overlap in time" },
    { PM_ERR_LOGHOST,		"PM_ERR_LOGHOST",
	"Archives differ by host" },
    { PM_ERR_LOGCHANGETYPE,	"PM_ERR_LOGCHANGETYPE",
	"The type of a metric has changed in an archive" },
    { PM_ERR_LOGCHANGESEM,	"PM_ERR_LOGCHANGESEM",
	"The semantics of a metric has changed in an archive" },
    { PM_ERR_LOGCHANGEINDOM,	"PM_ERR_LOGCHANGEINDOM",
	"The instance domain of a metric has changed in an archive" },
    { PM_ERR_LOGCHANGEUNITS,	"PM_ERR_LOGCHANGEUNITS",
	"The units of a metric have changed in an archive" },
    { PM_ERR_NEEDCLIENTCERT,	"PM_ERR_NEEDCLIENTCERT",
	"PMCD requires a client certificate" },
    { PM_ERR_BADDERIVE,		"PM_ERR_BADDERIVE",
	"Derived metric definition failed" },
    { PM_ERR_NOLABELS,		"PM_ERR_NOLABELS",
	"No support for metric label metadata" },
    { PM_ERR_PMDAFENCED,	"PM_ERR_PMDAFENCED",
	"PMDA is currently fenced and unable to respond to requests" },
    { PM_ERR_RECTYPE,		"PM_ERR_RECTYPE",
	"Incorrect record type in an archive" },
    { PM_ERR_FEATURE,		"PM_ERR_FEATURE",
	"Archive feature not supported" },
    { PM_ERR_ARG,		"PM_ERR_ARG",
	"Bad value for function argument" },
    /* insert new libpcp error codes above this line */
    { PM_ERR_NYI,		"PM_ERR_NYI",
	"Functionality not yet implemented" },
    /* do not use values smaller than NYI */
    { 0,			"",
	"" }
};

#define BADCODE "No such PMAPI error code (%d)"

#ifndef IS_MINGW
/*
 * handle non-determinism in the GNU implementation of strerror_r()
 */
static void
strerror_x(int code, char *buf, int buflen)
{
#ifdef HAVE_STRERROR_R_PTR
    char	*p;
    p = strerror_r(code, buf, buflen);
    if (p != buf) {
	strncpy(buf, p, buflen);
	buf[buflen-1] = '\0';
    }
#else
    /*
     * the more normal POSIX and XSI compliant variants always fill buf[]
     */
    strerror_r(code, buf, buflen);
#endif
}
#endif

char *
pmErrStr_r(int code, char *buf, int buflen)
{
    int		i;
#ifndef IS_MINGW
    static int	first = 1;
    static char	*unknown = NULL;
#else
    static char	unknown[] = "Unknown error";
#endif

    if (code == 0) {
	strncpy(buf, "No error", buflen);
	buf[buflen-1] = '\0';
	return buf;
    }

    /*
     * Is the code from a library wrapped by libpcp?  (e.g. SASL)
     * By good fortune, these libraries use error codes with no overlap.
     */
    if (code < PM_ERR_NYI) {
#ifdef HAVE_SECURE_SOCKETS
#define DECODE_SECURE_SOCKETS_ERROR(c)	((c) - PM_ERR_NYI)	/* negative */
#define DECODE_SASL_SPECIFIC_ERROR(c)	((c) < -1000 ? 0 : (c))

	int error = DECODE_SECURE_SOCKETS_ERROR(code);
	if (DECODE_SASL_SPECIFIC_ERROR(error)) {
	    PM_LOCK(__pmLock_extcall);
	    pmsprintf(buf, buflen, "Authentication - %s", sasl_errstring(error, NULL, NULL));	/* THREADSAFE */
	    PM_UNLOCK(__pmLock_extcall);
	} else {
	    pmsprintf(buf, buflen, "Unknown secure sockets error %d (%d)", code, error);
	}
	buf[buflen-1] = '\0';
	return buf;
#endif
    }

#ifndef IS_MINGW
    PM_LOCK(err_lock);
    if (first) {
	/*
	 * reference message for an unrecognized error code.
	 * For IRIX, strerror() returns NULL in this case.
	 */
	strerror_x(-1, buf, buflen);
	if (buf[0] != '\0') {
	    /*
	     * For Linux et al, strip the last word, expected to be the
	     * error number as in ...
	     *    Unknown error -1
	     * or
	     *    Unknown error 4294967295
	     */
	    char *sp = strrchr(buf, ' ');
	    char *p;

	    if (sp != NULL) {
		sp++;
		if (*sp == '-') sp++;
		for (p = sp; *p != '\0'; p++) {
		    if (!isdigit((int)*p)) break;
		}

		if (*p == '\0') {
PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_ALLOC);
		    *sp = '\0';
		    if ((unknown = strdup(buf)) != NULL)
			unknown[sp - buf] = '\0';
		}
	    }
	}
	first = 0;
    }
    PM_UNLOCK(err_lock);
    if (code < 0 && code > -PM_ERR_BASE) {
	/* intro(2) / errno(3) errors, maybe */
	strerror_x(-code, buf, buflen);
	if (unknown == NULL) {
	    if (buf[0] != '\0')
		return buf;
	}
	else {
	    /* The intention here is to catch variants of "Unknown
	     * error XXX" - in this case we're going to fail the
	     * stncmp() below, fall through and return a pcp error
	     * message, otherwise return the system error message
	     */
	    if (strncmp(buf, unknown, strlen(unknown)) != 0)
		return buf;
	}
    }
#else	/* WIN32 */
    if (code > -PM_ERR_BASE || code < -PM_ERR_NYI) {
	const char	*bp;
	if ((bp = wsastrerror(-code)) != NULL) {
	    strncpy(buf, bp, buflen);
	    buf[buflen-1] = '\0';
	}
	else {
	    /* No strerror_r in MinGW, so need to lock */
	    char	*tbp;
	    PM_LOCK(__pmLock_extcall);
	    tbp = strerror(-code);		/* THREADSAFE */
	    strncpy(buf, tbp, buflen);
	    buf[buflen-1] = '\0';
	    PM_UNLOCK(__pmLock_extcall);
	}

	if (strncmp(buf, unknown, strlen(unknown)) != 0)
	    return buf;
    }
#endif

    for (i = 0; errtab[i].err; i++) {
	if (errtab[i].err == code) {
	    strncpy(buf, errtab[i].errmess, buflen);
	    buf[buflen-1] = '\0';
	    return buf;
	}
    }

    /* failure */
    pmsprintf(buf, buflen, BADCODE,  code);
    return buf;
}

char *
pmErrStr(int code)
{
    static char	errmsg[PM_MAXERRMSGLEN];
    pmErrStr_r(code, errmsg, sizeof(errmsg));
    return errmsg;
}

void
__pmDumpErrTab(FILE *f)
{
    int	i;

    fprintf(f, "  Code  Symbolic Name          Message\n");
    for (i = 0; errtab[i].err; i++)
	fprintf(f, "%6d  %-22s %s\n",
	    errtab[i].err, errtab[i].symb, errtab[i].errmess);
}
