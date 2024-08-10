/*
 * Copyright (c) 2012-2015,2017,2021 Red Hat.
 * Copyright (c) 1995-2005 Silicon Graphics, Inc.  All Rights Reserved.
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
 * Thread-safe notes:
 *
 * maxsize - monotonic increasing and rarely changes, so use pdu_lock
 * 	mutex to protect updates, but allow reads without locking
 * 	as seeing an unexpected newly updated value is benign
 * tracebuf and tracenext - protected by pdu_lock
 *
 * On success, the result parameter from __pmGetPDU() points into a PDU
 * buffer that is pinned from the call to __pmFindPDUBuf().  It is the
 * responsibility of the __pmGetPDU() caller to unpin the buffer when
 * it is safe to do so.
 *
 * __pmPDUCntIn[] and __pmPDUCntOut[] are diagnostic counters that are
 * maintained with non-atomic updates ... we've decided that it is
 * acceptable for their values to be subject to possible (but unlikely)
 * missed updates
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"
#include "fault.h"
#include <assert.h>
#include <errno.h>

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	pdu_lock = PTHREAD_MUTEX_INITIALIZER;
#else
void			*pdu_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == pdu_lock
 */
int
__pmIsPduLock(void *lock)
{
    return lock == (void *)&pdu_lock;
}
#endif

/*
 * Performance Instrumentation
 *  ... counts binary PDUs received and sent by PDU type (actually type
 *      minus PDU_START so array is indexed from 0)
 *  ... trace buffer of recent PDUs successfully sent and received
 */

static unsigned int	inctrs[PDU_MAX+1];
static unsigned int	outctrs[PDU_MAX+1];
PCP_DATA unsigned int	*__pmPDUCntIn = inctrs;
PCP_DATA unsigned int	*__pmPDUCntOut = outctrs;

typedef struct {
    int		fd;
    int		xmit;		/* 1 for xmit, 0 for recv */
    int		type;
    int		len;
} trace_t;

#define NUMTRACE 8
static trace_t		tracebuf[NUMTRACE];
static unsigned int	tracenext;

static pid_t		mypid = -1;
static int              ceiling = PDU_CHUNK * 64;

static struct timeval	req_wait = { 10, 0 };
static int		req_wait_done;

#define HEADER	-1
#define BODY	0

static void
trace_insert(int fd, int xmit, __pmPDUHdr *php)
{
    unsigned int	p;
    PM_LOCK(pdu_lock);
    p = (tracenext++) % NUMTRACE;
    tracebuf[p].fd = fd;
    tracebuf[p].xmit = xmit;
    tracebuf[p].type = php->type;
    tracebuf[p].len = php->len;
    PM_UNLOCK(pdu_lock);
}

/*
 * Report recent PDUs
 */
void
__pmDumpPDUTrace(FILE *f)
{
    unsigned int	i;
    unsigned int	p;
    char		strbuf[20];

    PM_LOCK(pdu_lock);
    if (tracenext < NUMTRACE)
	i = 0;
    else
	i = tracenext - NUMTRACE;
    if (i == tracenext)
	fprintf(f, "__pmDumpPDUTrace: no PDUs so far\n");
    else {
	fprintf(f, "__pmDumpPDUTrace: recent PDUs ...\n");
	for ( ; i < tracenext; i++) {
	    p = i % NUMTRACE;
	    fprintf(f, "[%d] %s", i, tracebuf[p].xmit ? "xmit" : "recv");
	    fprintf(f, " fd=%d type=%s len=%d\n",
		tracebuf[p].fd,
		__pmPDUTypeStr_r(tracebuf[p].type, strbuf, sizeof(strbuf)),
		tracebuf[p].len);
	}
    }
    PM_UNLOCK(pdu_lock);
}

int
__pmSetRequestTimeout(double timeout)
{
    if (timeout < 0.0)
	return -EINVAL;

    PM_LOCK(pdu_lock);
    req_wait_done = 1;
    /* THREADSAFE - no locks acquired in pmtimevalFromReal() */
    pmtimevalFromReal(timeout, &req_wait);
    PM_UNLOCK(pdu_lock);
    return 0;
}

double
__pmRequestTimeout(void)
{
    char	*timeout_str, *end_ptr;
    double	timeout;

    /* get optional PMCD request timeout from the environment */
    PM_LOCK(pdu_lock);
    if (!req_wait_done) {
	req_wait_done = 1;
	PM_UNLOCK(pdu_lock);
	PM_LOCK(__pmLock_extcall);
	timeout_str = getenv("PMCD_REQUEST_TIMEOUT");		/* THREADSAFE */
	if (timeout_str != NULL)
	    timeout_str = strdup(timeout_str);
	if (timeout_str != NULL) {
	    timeout = strtod(timeout_str, &end_ptr);
	    PM_UNLOCK(__pmLock_extcall);
	    if (*end_ptr != '\0' || timeout < 0.0) {
		pmNotifyErr(LOG_WARNING,
			      "ignored bad PMCD_REQUEST_TIMEOUT = '%s'\n",
			      timeout_str);
	    }
	    else
		pmtimevalFromReal(timeout, &req_wait);
	    free(timeout_str);
	}
	else
	    PM_UNLOCK(__pmLock_extcall);
	PM_LOCK(pdu_lock);
    }
    timeout = pmtimevalToReal(&req_wait);
    PM_UNLOCK(pdu_lock);
    return timeout;
}

static int
pduread(int fd, char *buf, int len, int part, int timeout)
{
    int			socketipc = __pmSocketIPC(fd);
    int			status = 0;
    int			have = 0;
    int			onetrip = 1;
    struct timeval	dead_hand;
    struct timeval	now;

    /*
     * Regression circa Oct 2016 seems to have introduced the possibility
     * that fd may be (incorrectly) -1 here ...
     */
    assert(fd >= 0);

    if (timeout == -2 /*TIMEOUT_ASYNC*/)
	return -EOPNOTSUPP;

    /*
     * Handle short reads that may split a PDU ...
     *
     * The original logic here assumed that recv() would only split a
     * PDU at a word (__pmPDU) boundary ... with the introduction of
     * secure connections, SSL and possibly compression all lurking
     * below the socket covers, this is no longer a safe assumption.
     *
     * So, we keep nibbling at the input stream until we have all that
     * we have requested, or we timeout, or error.
     */
    while (len) {
	struct timeval	wait;

#if defined(IS_MINGW)	/* cannot select on a pipe on Win32 - yay! */
	if (!__pmSocketIPC(fd)) {
	    COMMTIMEOUTS cwait = { 0 };

	    if (timeout != TIMEOUT_NEVER)
		cwait.ReadTotalTimeoutConstant = timeout * 1000.0;
	    else
		cwait.ReadTotalTimeoutConstant = __pmRequestTimeout() * 1000.0;
	    SetCommTimeouts((HANDLE)_get_osfhandle(fd), &cwait);
	}
	else
#endif

	/*
	 * either never timeout (i.e. block forever), or timeout
	 */
	if (timeout != TIMEOUT_NEVER) {
	    if (timeout > 0) {
		wait.tv_sec = timeout;
		wait.tv_usec = 0;
	    }
	    else {
		PM_LOCK(pdu_lock);
		wait = req_wait;
		PM_UNLOCK(pdu_lock);
	    }
	    if (onetrip) {
		/*
		 * Need all parts of the PDU to be received by dead_hand
		 * This enforces a low overall timeout for the whole PDU
		 * (as opposed to just a timeout for individual calls to
		 * recv).
		 */
		gettimeofday(&dead_hand, NULL);
		dead_hand.tv_sec += wait.tv_sec;
		dead_hand.tv_usec += wait.tv_usec;
		while (dead_hand.tv_usec >= 1000000) {
		    dead_hand.tv_usec -= 1000000;
		    dead_hand.tv_sec++;
		}
		onetrip = 0;
	    }

again:
	    status = __pmSocketReady(fd, &wait);
	    if (status > 0) {
		gettimeofday(&now, NULL);
		if (now.tv_sec > dead_hand.tv_sec ||
		    (now.tv_sec == dead_hand.tv_sec &&
		     now.tv_usec > dead_hand.tv_usec))
		    status = 0;
	    }
	    if (status == 0) {
		if (__pmGetInternalState() != PM_STATE_APPL) {
		    /* special for PMCD and friends 
		     * Note, on Linux select would return 'time remaining'
		     * in timeout value, so report the expected timeout
		     */
		    int tosec, tousec;

		    if ( timeout != TIMEOUT_NEVER && timeout > 0 ) {
			tosec = (int)timeout;
			tousec = 0;
		    } else {
			PM_LOCK(pdu_lock);
			tosec = (int)req_wait.tv_sec;
			tousec = (int)req_wait.tv_usec;
			PM_UNLOCK(pdu_lock);
		    }

		    pmNotifyErr(LOG_WARNING, 
				  "pduread: timeout (after %d.%06d "
				  "sec) while attempting to read %d "
				  "bytes out of %d in %s on fd=%d",
				  tosec, tousec, len - have, len, 
				  part == HEADER ? "HDR" : "BODY", fd);
		}
		return PM_ERR_TIMEOUT;
	    }
	    else if (status < 0) {
		char	errmsg[PM_MAXERRMSGLEN];
		status = -neterror();
		if (status == -EINTR) {
		    /* interrupted __pmSocketReady() and no data ... keep trying */
		    if (pmDebugOptions.pdu && pmDebugOptions.desperate) {
			fprintf(stderr, "pduread(%d, ...): __pmSocketReady() interrupt!\n", fd);
		    }
		    goto again;
		}
		pmNotifyErr(LOG_ERR, "pduread: select() on fd=%d status=%d: %s",
		    fd, status, netstrerror_r(errmsg, sizeof(errmsg)));
		setoserror(neterror());
		return status;
	    }
	}
	if (socketipc) {
	    status = __pmRecv(fd, buf, len, 0);
	    setoserror(neterror());
	} else {
	    status = read(fd, buf, len);
	}
	__pmOverrideLastFd(fd);
	if (status <= 0) {
	    if (status < 0 && oserror() == EINTR) {
		/* interrupted read() and no data ... keep trying */
		if (pmDebugOptions.pdu && pmDebugOptions.desperate) {
		    fprintf(stderr, "pduread(%d, ...): %s() interrupt!\n",
			fd, socketipc == 0 ? "read" : "recv");
		}
		continue;
	    }
	    /* end of file or error */
	    if (pmDebugOptions.pdu && pmDebugOptions.desperate) {
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "pduread(%d, ...): eof/error %d from %s: ",
			fd, status, socketipc == 0 ? "read" : "__pmRecv");
		fprintf(stderr, "%s\n", osstrerror_r(errmsg, sizeof(errmsg)));
	    }
	    if (status == 0)
		/* return what we have, or nothing */
		break;

	    /* else error return */
	    return status;
	}

	have += status;
	buf += status;
	len -= status;
	if (pmDebugOptions.pdu && pmDebugOptions.desperate) {
	    fprintf(stderr, "pduread(%d, ...): have %d, last read %d, still need %d\n",
		fd, have, status, len);
	}
    }

    if (pmDebugOptions.pdu && pmDebugOptions.desperate)
	fprintf(stderr, "pduread(%d, ...): return %d\n", fd, have);

    return have;
}

char *
__pmPDUTypeStr_r(int type, char *buf, int buflen)
{
    char	*res;

    switch (type) {
    case PDU_ERROR:		res = "ERROR"; break;
    case PDU_RESULT:		res = "RESULT"; break;
    case PDU_PROFILE:		res = "PROFILE"; break;
    case PDU_FETCH:		res = "FETCH"; break;
    case PDU_DESC_REQ:		res = "DESC_REQ"; break;
    case PDU_DESC:		res = "DESC"; break;
    case PDU_INSTANCE_REQ:	res = "INSTANCE_REQ"; break;
    case PDU_INSTANCE:		res = "INSTANCE"; break;
    case PDU_TEXT_REQ:		res = "TEXT_REQ"; break;
    case PDU_TEXT:		res = "TEXT"; break;
    case PDU_CONTROL_REQ:	res = "CONTROL_REQ"; break;
    case PDU_CREDS:		res = "CREDS"; break;
    case PDU_PMNS_IDS:		res = "PMNS_IDS"; break;
    case PDU_PMNS_NAMES:	res = "PMNS_NAMES"; break;
    case PDU_PMNS_CHILD:	res = "PMNS_CHILD"; break;
    case PDU_PMNS_TRAVERSE:	res = "PMNS_TRAVERSE"; break;
    case PDU_LOG_CONTROL:	res = "LOG_CONTROL"; break;
    case PDU_LOG_STATUS_V2:	res = "LOG_STATUS_V2"; break;
    case PDU_LOG_STATUS:	res = "LOG_STATUS"; break;
    case PDU_LOG_REQUEST:	res = "LOG_REQUEST"; break;
    case PDU_ATTR:		res = "ATTR"; break;
    case PDU_LABEL_REQ:		res = "LABEL_REQ"; break;
    case PDU_LABEL:		res = "LABEL"; break;
    case PDU_HIGHRES_FETCH:	res = "HIGHRES_FETCH"; break;
    case PDU_HIGHRES_RESULT:	res = "HIGHRES_RESULT"; break;
    case PDU_DESC_IDS:		res = "DESC_IDS"; break;
    case PDU_DESCS:		res = "DESCS"; break;
    default:			res = NULL; break;
    }
    if (res)
	pmsprintf(buf, buflen, "%s", res);
    else
	pmsprintf(buf, buflen, "TYPE-%d?", type);
    return buf;
}

const char *
__pmPDUTypeStr(int type)
{
    static char	tbuf[20];
    __pmPDUTypeStr_r(type, tbuf, sizeof(tbuf));
    return tbuf;
}

#if defined(HAVE_SIGPIPE)
/*
 * Because the default handler for SIGPIPE is to exit, we always want a handler
 * installed to override that so that the write() just returns an error.  The
 * problem is that the user might have installed one prior to the first write()
 * or may install one at some later stage.  This doesn't matter.  As long as a
 * handler other than SIG_DFL is there, all will be well.  The first time that
 * __pmXmitPDU is called, install SIG_IGN as the handler for SIGPIPE.  If the
 * user had already changed the handler from SIG_DFL, put back what was there
 * before.
 */
void
__pmIgnoreSignalPIPE(void)
{
    static int sigpipe_done = 0;	/* First time check for installation of
					   non-default SIGPIPE handler */
    PM_LOCK(pdu_lock);
    if (!sigpipe_done) {       /* Make sure SIGPIPE is handled */
	SIG_PF  user_onpipe;
	user_onpipe = signal(SIGPIPE, SIG_IGN);
	if (user_onpipe != SIG_DFL)     /* Put user handler back */
	     signal(SIGPIPE, user_onpipe);
	sigpipe_done = 1;
    }
    PM_UNLOCK(pdu_lock);
}
#else
void __pmIgnoreSignalPIPE(void) {}
#endif

int
__pmXmitPDU(int fd, __pmPDU *pdubuf)
{
    int		socketipc = __pmSocketIPC(fd);
    int		off = 0;
    int		len;
    int		sts;
    __pmPDUHdr	*php = (__pmPDUHdr *)pdubuf;

    if (fd < 0)
	return -EBADF;

    __pmIgnoreSignalPIPE();

    if (pmDebugOptions.pdu) {
	int	j;
	char	*p;
	int	jend = PM_PDU_SIZE(php->len);
	char	strbuf[20];

        /* clear the padding bytes, lest they contain garbage */
	p = (char *)pdubuf + php->len;
	while (p < (char *)pdubuf + jend*sizeof(__pmPDU))
	    *p++ = '~';	/* buffer end */

	if (mypid == -1)
	    mypid = getpid();
	fprintf(stderr, "[%" FMT_PID "]%s: %s fd=%d len=%d", mypid, "pmXmitPDU",
		__pmPDUTypeStr_r(php->type, strbuf, sizeof(strbuf)), fd, php->len);
	for (j = 0; j < jend; j++) {
	    if ((j % 8) == 0)
		fprintf(stderr, "\n%03d: ", j);
	    fprintf(stderr, "%8x ", pdubuf[j]);
	}
	putc('\n', stderr);
    }
    len = php->len;

    php->len = htonl(php->len);
    php->from = htonl(php->from);
    php->type = htonl(php->type);
    while (off < len) {
	char *p = (char *)pdubuf;
	int n;

	p += off;

	n = socketipc ? __pmSend(fd, p, len-off, 0) : write(fd, p, len-off);
	if (n < 0) {
	    if (pmDebugOptions.pdu) {
		if (socketipc)
		    fprintf(stderr, "%s: socket __pmSend() result %d != %d\n",
				    "__pmXmitPDU", n, len-off);
		else
		    fprintf(stderr, "%s: non-socket write() result %d != %d\n",
				    "__pmXmitPDU", n, len-off);
	    }
	    break;
	}
	off += n;
    }
    php->len = ntohl(php->len);
    php->from = ntohl(php->from);
    php->type = ntohl(php->type);

    if (off != len) {
	if (socketipc) {
	    sts = -neterror();
	    if (__pmSocketClosed()) {
		if (pmDebugOptions.pdu)
		    fprintf(stderr, "%s: PM_ERR_IPC because __pmSocketClosed() "
				    "(maybe error %d from oserror())\n",
				    "__pmXmitPDU", oserror());
		return PM_ERR_IPC;
	    }
	    if (sts != 0) {
		if (pmDebugOptions.pdu)
		    fprintf(stderr, "%s: error %d from neterror()\n",
				    "__pmXmitPDU", sts);
		return sts;
	    }
	    else {
		if (pmDebugOptions.pdu)
		    fprintf(stderr, "%s: PM_ERR_IPC on socket path, reason unknown\n",
				    "__pmXmitPDU");
		return PM_ERR_IPC;
	    }
	}
	sts = -oserror();
	if (sts != 0) {
	    if (pmDebugOptions.pdu)
		fprintf(stderr, "%s: error %d from oserror()\n", "__pmXmitPDU", sts);
	    return sts;
	}
	else {
	    if (pmDebugOptions.pdu)
		fprintf(stderr, "%s: PM_ERR_IPC on non-socket path, reason unknown\n",
				"__pmXmitPDU");
	    return PM_ERR_IPC;
	}
    }

    __pmOverrideLastFd(fd);
    if (php->type >= PDU_START && php->type <= PDU_FINISH)
	__pmPDUCntOut[php->type-PDU_START]++;
    trace_insert(fd, 1, php);

    return off;
}

/* result is pinned on successful return */
int
__pmGetPDU(int fd, int mode, int timeout, __pmPDU **result)
{
    int			need;
    int			len;
    static int		maxsize = PDU_CHUNK;
    char		*handle;
    __pmPDU		*pdubuf;
    __pmPDU		*pdubuf_prev;
    __pmPDUHdr		*php;

PM_FAULT_RETURN(PM_ERR_TIMEOUT);

    if ((pdubuf = __pmFindPDUBuf(maxsize)) == NULL)
	return -oserror();

    /* First read - try to read the header */
    len = pduread(fd, (void *)pdubuf, sizeof(__pmPDUHdr), HEADER, timeout);
    php = (__pmPDUHdr *)pdubuf;

    if (len < (int)sizeof(__pmPDUHdr)) {
	if (len == -1) {
	    if (! __pmSocketClosed()) {
		char	errmsg[PM_MAXERRMSGLEN];
		if (pmDebugOptions.pdu)
		    pmNotifyErr(LOG_ERR, "%s: fd=%d hdr read: len=%d: %s",
			        "__pmGetPDU", fd, len,
			        pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
	    }
	}
	else if (len >= (int)sizeof(php->len)) {
	    /*
	     * Have part of a PDU header.  Enough for the "len"
	     * field to be valid, but not yet all of it - save
	     * what we have received and try to read some more.
	     * Note this can only happen once per PDU, so the
	     * ntohl() below will _only_ be done once per PDU.
	     */
	    goto check_read_len;	/* continue, do not return */
	}
	else if (len == PM_ERR_TIMEOUT || len == PM_ERR_TLS || len == -EINTR) {
	    __pmUnpinPDUBuf(pdubuf);
	    return len;
	}
	else if (len < 0) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    if (pmDebugOptions.pdu)
		pmNotifyErr(LOG_ERR, "%s: fd=%d hdr read: len=%d: %s",
			    "__pmGetPDU", fd, len,
			    pmErrStr_r(len, errmsg, sizeof(errmsg)));
	    __pmUnpinPDUBuf(pdubuf);
	    return PM_ERR_IPC;
	}
	else if (len > 0) {
	    if (pmDebugOptions.pdu)
		pmNotifyErr(LOG_ERR, "%s: fd=%d hdr read: bad len=%d",
			    "__pmGetPDU", fd, len);
	    __pmUnpinPDUBuf(pdubuf);
	    return PM_ERR_IPC;
	}

	/*
	 * end-of-file with no data
	 */
	__pmUnpinPDUBuf(pdubuf);
	return 0;
    }

check_read_len:
    php->len = ntohl(php->len);
    if (php->len < (int)sizeof(__pmPDUHdr)) {
	/*
	 * PDU length indicates insufficient bytes for a PDU header
	 * ... looks like DOS attack like PV 935490
	 */
	if (pmDebugOptions.pdu)
	    pmNotifyErr(LOG_ERR, "%s: fd=%d illegal PDU len=%d in hdr",
			"__pmGetPDU", fd, php->len);
	__pmUnpinPDUBuf(pdubuf);
	return PM_ERR_IPC;
    }
    else if (mode == LIMIT_SIZE && php->len > ceiling) {
	/*
	 * Guard against denial of service attack ... don't accept PDUs
	 * from clients that are larger than 64 Kbytes (ceiling)
	 * (note, pmcd and pmdas have to be able to _send_ large PDUs,
	 * e.g. for a pmResult or instance domain enquiry)
	 */
	if (pmDebugOptions.pdu) {
	    if (len < (int)(sizeof(php->len) + sizeof(php->type)))
		/* PDU too short to provide a valid type */
		pmNotifyErr(LOG_ERR, "%s: fd=%d bad PDU len=%d in hdr"
			    " exceeds maximum client PDU size (%d)",
			    "__pmGetPDU", fd, php->len, ceiling);
	    else
		pmNotifyErr(LOG_ERR, "%s: fd=%d type=0x%x bad PDU len=%d in hdr"
			    " exceeds maximum client PDU size (%d)",
			    "__pmGetPDU", fd, (unsigned)ntohl(php->type),
			    php->len, ceiling);
	}
	__pmUnpinPDUBuf(pdubuf);
	return PM_ERR_TOOBIG;
    }

    if (len < php->len) {
	/*
	 * need to read more ...
	 */
	int		tmpsize;
	int		have = len;

	PM_LOCK(pdu_lock);
	if (php->len > maxsize) {
	    tmpsize = PDU_CHUNK * ( 1 + php->len / PDU_CHUNK);
	    maxsize = tmpsize;
	}
	else
	    tmpsize = maxsize;
	PM_UNLOCK(pdu_lock);

	pdubuf_prev = pdubuf;
	if ((pdubuf = __pmFindPDUBuf(tmpsize)) == NULL) {
	    __pmUnpinPDUBuf(pdubuf_prev);
	    return -oserror();
	}

	memmove((void *)pdubuf, (void *)php, len);
	__pmUnpinPDUBuf(pdubuf_prev);

	php = (__pmPDUHdr *)pdubuf;
	need = php->len - have;
	handle = (char *)pdubuf;
	/* block until all of the PDU is received this time */
	len = pduread(fd, (void *)&handle[len], need, BODY, timeout);
	if (len != need) {
	    if (len == PM_ERR_TIMEOUT) {
		__pmUnpinPDUBuf(pdubuf);
		return PM_ERR_TIMEOUT;
	    }
	    else if (!pmDebugOptions.pdu) {
		__pmUnpinPDUBuf(pdubuf);
	        return PM_ERR_IPC;
	    }
	    else if (len < 0) {
		char	errmsg[PM_MAXERRMSGLEN];
		pmNotifyErr(LOG_ERR, "%s: fd=%d data read: len=%d: %s",
			    "__pmGetPDU", fd, len,
			    pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
	    }
	    else
		pmNotifyErr(LOG_ERR, "%s: fd=%d data read: have %d, want %d, got %d",
			    "__pmGetPDU", fd, have, need, len);
	    /*
	     * only report header fields if you've read enough bytes
	     */
	    if (len > 0)
		have += len;
	    if (have >= (int)(sizeof(php->len)+sizeof(php->type)+sizeof(php->from)))
		pmNotifyErr(LOG_ERR, "%s: PDU hdr: len=0x%x type=0x%x from=0x%x",
			    "__pmGetPDU", php->len, (unsigned)ntohl(php->type),
			    (unsigned)ntohl(php->from));
	    else if (have >= (int)(sizeof(php->len)+sizeof(php->type)))
		pmNotifyErr(LOG_ERR, "%s: PDU hdr: len=0x%x type=0x%x",
			    "__pmGetPDU", php->len, (unsigned)ntohl(php->type));
	    __pmUnpinPDUBuf(pdubuf);
	    return PM_ERR_IPC;
	}
    }

    *result = (__pmPDU *)php;
    php->type = ntohl((unsigned int)php->type);
    if (php->type < 0) {
	/*
	 * PDU type is bad ... could be a possible mem leak attack like
	 * https://bugzilla.redhat.com/show_bug.cgi?id=841319
	 */
	if (pmDebugOptions.pdu)
	    pmNotifyErr(LOG_ERR, "%s: fd=%d illegal PDU type=%d in hdr",
			"__pmGetPDU", fd, php->type);
	__pmUnpinPDUBuf(pdubuf);
	return PM_ERR_IPC;
    }
    php->from = ntohl((unsigned int)php->from);
    if (pmDebugOptions.pdu) {
	int	j;
	char	*p;
	int	jend = PM_PDU_SIZE(php->len);
	char	strbuf[20];

        /* clear the padding bytes, lest they contain garbage */
	p = (char *)*result + php->len;
	while (p < (char *)*result + jend*sizeof(__pmPDU))
	    *p++ = '~';	/* buffer end */

	if (mypid == -1)
	    mypid = getpid();
	fprintf(stderr, "[%" FMT_PID "]%s: %s fd=%d len=%d from=%d", mypid, "pmGetPDU",
		__pmPDUTypeStr_r(php->type, strbuf, sizeof(strbuf)), fd, php->len, php->from);
	for (j = 0; j < jend; j++) {
	    if ((j % 8) == 0)
		fprintf(stderr, "\n%03d: ", j);
	    fprintf(stderr, "%8x ", (*result)[j]);
	}
	putc('\n', stderr);
    }
    if (php->type >= PDU_START && php->type <= PDU_FINISH)
	__pmPDUCntIn[php->type-PDU_START]++;
    trace_insert(fd, 0, php);

    /*
     * Note php points into the PDU buffer pdubuf that remains pinned
     * and php is returned via the result parameter ... see the
     * thread-safe comments above
     */
    return php->type;
}

int
__pmSetPDUCeiling(int newceiling)
{
    int		oldceiling = ceiling;
    if (newceiling > 0) {
	ceiling = newceiling;
	return oldceiling;
    }
    else
	return -1;

}

void
__pmSetPDUCntBuf(unsigned *in, unsigned *out)
{
    __pmPDUCntIn = in;
    __pmPDUCntOut = out;
}

/*
 * report PDU counts
 */
void
__pmDumpPDUCnt(FILE *f)
{
    int			i;
    unsigned int	pduin = 0;
    unsigned int	pduout = 0;

    for (i = 0; i <= PDU_MAX; i++) {
	if (__pmPDUCntIn[i] != 0 || __pmPDUCntOut[i] != 0)
	    break;
    }
    if (i > PDU_MAX) {
	fprintf(f, "PDU stats ... no PDU activity\n");
	return;
    }
    fprintf(f, "PDU stats ...\n");
    fprintf(f, "%-20.20s %6s %6s\n", "Type", "Xmit", "Recv");
    for (i = 0; i <= PDU_MAX; i++) {
	pduin += __pmPDUCntIn[i];
	pduout += __pmPDUCntOut[i];
	if (__pmPDUCntIn[i] == 0 && __pmPDUCntOut[i] == 0)
	    continue;
	fprintf(f, "%-20.20s %6d %6d\n", __pmPDUTypeStr(i+PDU_START), __pmPDUCntOut[i], __pmPDUCntIn[i]);
    }
    fprintf(f, "%-20.20s %6d %6d\n", "Total", pduout, pduin);
}
