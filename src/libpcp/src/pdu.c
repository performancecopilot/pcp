/*
 * Copyright (c) 2012-2015 Red Hat.
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
 * maxsize - monotonic increasing and rarely changes, so use global
 * 	mutex to protect updates, but allow reads without locking
 * 	as seeing an unexpected newly updated value is benign
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
#include "impl.h"
#include "internal.h"

PCP_DATA int	pmDebug;		/* the real McCoy */

/*
 * Performance Instrumentation
 *  ... counts binary PDUs received and sent by low 4 bits of PDU type
 */

static unsigned int	inctrs[PDU_MAX+1];
static unsigned int	outctrs[PDU_MAX+1];
PCP_DATA unsigned int	*__pmPDUCntIn = inctrs;
PCP_DATA unsigned int	*__pmPDUCntOut = outctrs;

#ifdef PCP_DEBUG
static int		mypid = -1;
#endif
static int              ceiling = PDU_CHUNK * 64;

static struct timeval	def_wait = { 10, 0 };
static double		def_timeout = 10.0;

#define HEADER	-1
#define BODY	0

const struct timeval *
__pmDefaultRequestTimeout(void)
{
    static int		done_default = 0;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (!done_default) {
	char	*timeout_str;
	char	*end_ptr;
	if ((timeout_str = getenv("PMCD_REQUEST_TIMEOUT")) != NULL) {
	    def_timeout = strtod(timeout_str, &end_ptr);
	    if (*end_ptr != '\0' || def_timeout < 0.0) {
		__pmNotifyErr(LOG_WARNING,
			      "ignored bad PMCD_REQUEST_TIMEOUT = '%s'\n",
			      timeout_str);
	    }
	    else {
		__pmtimevalFromReal(def_timeout, &def_wait);
	    }
	}
	done_default = 1;
    }
    PM_UNLOCK(__pmLock_libpcp);
    return (&def_wait);
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
		cwait.ReadTotalTimeoutConstant = def_timeout * 1000.0;
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
	    else
		wait = def_wait;
	    if (onetrip) {
		/*
		 * Need all parts of the PDU to be received by dead_hand
		 * This enforces a low overall timeout for the whole PDU
		 * (as opposed to just a timeout for individual calls to
		 * recv).  A more invasive alternative (better) approach
		 * would see all I/O performed in the main event loop,
		 * and I/O routines transformed to continuation-passing
		 * style.
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
		    int tosec, tomsec;

		    if ( timeout != TIMEOUT_NEVER && timeout > 0 ) {
			tosec = (int)timeout;
			tomsec = 0;
		    } else {
			tosec = (int)def_wait.tv_sec;
			tomsec = 1000*(int)def_wait.tv_usec;
		    }

		    __pmNotifyErr(LOG_WARNING, 
				  "pduread: timeout (after %d.%03d "
				  "sec) while attempting to read %d "
				  "bytes out of %d in %s on fd=%d",
				  tosec, tomsec, len - have, len, 
				  part == HEADER ? "HDR" : "BODY", fd);
		}
		return PM_ERR_TIMEOUT;
	    }
	    else if (status < 0) {
		char	errmsg[PM_MAXERRMSGLEN];
		__pmNotifyErr(LOG_ERR, "pduread: select() on fd=%d: %s",
			fd, netstrerror_r(errmsg, sizeof(errmsg)));
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
	if (status < 0)
	    /* error */
	    return status;
	else if (status == 0)
	    /* return what we have, or nothing */
	    break;

	have += status;
	buf += status;
	len -= status;
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    fprintf(stderr, "pduread(%d, ...): have %d, last read %d, still need %d\n",
		fd, have, status, len);
	}
#endif
    }

    return have;
}

char *
__pmPDUTypeStr_r(int type, char *buf, int buflen)
{
    char	*res = NULL;
    if (type == PDU_ERROR) res = "ERROR";
    else if (type == PDU_RESULT) res = "RESULT";
    else if (type == PDU_PROFILE) res = "PROFILE";
    else if (type == PDU_FETCH) res = "FETCH";
    else if (type == PDU_DESC_REQ) res = "DESC_REQ";
    else if (type == PDU_DESC) res = "DESC";
    else if (type == PDU_INSTANCE_REQ) res = "INSTANCE_REQ";
    else if (type == PDU_INSTANCE) res = "INSTANCE";
    else if (type == PDU_TEXT_REQ) res = "TEXT_REQ";
    else if (type == PDU_TEXT) res = "TEXT";
    else if (type == PDU_CONTROL_REQ) res = "CONTROL_REQ";
    else if (type == PDU_CREDS) res = "CREDS";
    else if (type == PDU_PMNS_IDS) res = "PMNS_IDS";
    else if (type == PDU_PMNS_NAMES) res = "PMNS_NAMES";
    else if (type == PDU_PMNS_CHILD) res = "PMNS_CHILD";
    else if (type == PDU_PMNS_TRAVERSE) res = "PMNS_TRAVERSE";
    else if (type == PDU_LOG_CONTROL) res = "LOG_CONTROL";
    else if (type == PDU_LOG_STATUS) res = "LOG_STATUS";
    else if (type == PDU_LOG_REQUEST) res = "LOG_REQUEST";
    else if (type == PDU_ATTR) res = "ATTR";
    if (res == NULL)
	snprintf(buf, buflen, "TYPE-%d?", type);
    else
	snprintf(buf, buflen, "%s", res);

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
static int sigpipe_done = 0;		/* First time check for installation of
					   non-default SIGPIPE handler */
void
__pmIgnoreSignalPIPE(void)
{
    if (!sigpipe_done) {       /* Make sure SIGPIPE is handled */
	SIG_PF  user_onpipe;
	user_onpipe = signal(SIGPIPE, SIG_IGN);
	if (user_onpipe != SIG_DFL)     /* Put user handler back */
	     signal(SIGPIPE, user_onpipe);
	sigpipe_done = 1;
    }
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
    __pmPDUHdr	*php = (__pmPDUHdr *)pdubuf;

    __pmIgnoreSignalPIPE();

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	int	j;
	char	*p;
	int	jend = PM_PDU_SIZE(php->len);
	char	strbuf[20];

        /* clear the padding bytes, lest they contain garbage */
	p = (char *)pdubuf + php->len;
	while (p < (char *)pdubuf + jend*sizeof(__pmPDU))
	    *p++ = '~';	/* buffer end */

	if (mypid == -1)
	    mypid = (int)getpid();
	fprintf(stderr, "[%d]pmXmitPDU: %s fd=%d len=%d",
		mypid, __pmPDUTypeStr_r(php->type, strbuf, sizeof(strbuf)), fd, php->len);
	for (j = 0; j < jend; j++) {
	    if ((j % 8) == 0)
		fprintf(stderr, "\n%03d: ", j);
	    fprintf(stderr, "%8x ", pdubuf[j]);
	}
	putc('\n', stderr);
    }
#endif
    len = php->len;

    php->len = htonl(php->len);
    php->from = htonl(php->from);
    php->type = htonl(php->type);
    while (off < len) {
	char *p = (char *)pdubuf;
	int n;

	p += off;

	n = socketipc ? __pmSend(fd, p, len-off, 0) : write(fd, p, len-off);
	if (n < 0)
	    break;
	off += n;
    }
    php->len = ntohl(php->len);
    php->from = ntohl(php->from);
    php->type = ntohl(php->type);

    if (off != len) {
	if (socketipc) {
	    if (__pmSocketClosed())
		return PM_ERR_IPC;
	    return neterror() ? -neterror() : PM_ERR_IPC;
	}
	return oserror() ? -oserror() : PM_ERR_IPC;
    }

    __pmOverrideLastFd(fd);
    if (php->type >= PDU_START && php->type <= PDU_FINISH)
	__pmPDUCntOut[php->type-PDU_START]++;

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

    if ((pdubuf = __pmFindPDUBuf(maxsize)) == NULL)
	return -oserror();

    /* First read - try to read the header */
    len = pduread(fd, (void *)pdubuf, sizeof(__pmPDUHdr), HEADER, timeout);
    php = (__pmPDUHdr *)pdubuf;

    if (len < (int)sizeof(__pmPDUHdr)) {
	if (len == -1) {
	    if (__pmSocketClosed()) {
		len = 0;
	    } else {
		char	errmsg[PM_MAXERRMSGLEN];
		__pmNotifyErr(LOG_ERR, "__pmGetPDU: fd=%d hdr read: len=%d: %s", fd, len, pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
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
	else if (len == PM_ERR_TIMEOUT) {
	    __pmUnpinPDUBuf(pdubuf);
	    return PM_ERR_TIMEOUT;
	}
	else if (len < 0) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    __pmNotifyErr(LOG_ERR, "__pmGetPDU: fd=%d hdr read: len=%d: %s", fd, len, pmErrStr_r(len, errmsg, sizeof(errmsg)));
	    __pmUnpinPDUBuf(pdubuf);
	    return PM_ERR_IPC;
	}
	else if (len > 0) {
	    __pmNotifyErr(LOG_ERR, "__pmGetPDU: fd=%d hdr read: bad len=%d", fd, len);
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
	__pmNotifyErr(LOG_ERR, "__pmGetPDU: fd=%d illegal PDU len=%d in hdr", fd, php->len);
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
	__pmNotifyErr(LOG_ERR, "__pmGetPDU: fd=%d bad PDU len=%d in hdr exceeds maximum client PDU size (%d)",
		      fd, php->len, ceiling);

	__pmUnpinPDUBuf(pdubuf);
	return PM_ERR_TOOBIG;
    }

    if (len < php->len) {
	/*
	 * need to read more ...
	 */
	int		tmpsize;
	int		have = len;

	PM_INIT_LOCKS();
	PM_LOCK(__pmLock_libpcp);
	if (php->len > maxsize) {
	    tmpsize = PDU_CHUNK * ( 1 + php->len / PDU_CHUNK);
	    maxsize = tmpsize;
	}
	else
	    tmpsize = maxsize;
	PM_UNLOCK(__pmLock_libpcp);

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
	    else if (len < 0) {
		char	errmsg[PM_MAXERRMSGLEN];
		__pmNotifyErr(LOG_ERR, "__pmGetPDU: fd=%d data read: len=%d: %s", fd, len, pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
	    }
	    else
		__pmNotifyErr(LOG_ERR, "__pmGetPDU: fd=%d data read: have %d, want %d, got %d", fd, have, need, len);
	    /*
	     * only report header fields if you've read enough bytes
	     */
	    if (len > 0)
		have += len;
	    if (have >= (int)(sizeof(php->len)+sizeof(php->type)+sizeof(php->from)))
		__pmNotifyErr(LOG_ERR, "__pmGetPDU: PDU hdr: len=0x%x type=0x%x from=0x%x", php->len, (unsigned)ntohl(php->type), (unsigned)ntohl(php->from));
	    else if (have >= (int)(sizeof(php->len)+sizeof(php->type)))
		__pmNotifyErr(LOG_ERR, "__pmGetPDU: PDU hdr: len=0x%x type=0x%x", php->len, (unsigned)ntohl(php->type));
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
	__pmNotifyErr(LOG_ERR, "__pmGetPDU: fd=%d illegal PDU type=%d in hdr", fd, php->type);
	__pmUnpinPDUBuf(pdubuf);
	return PM_ERR_IPC;
    }
    php->from = ntohl((unsigned int)php->from);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	int	j;
	char	*p;
	int	jend = PM_PDU_SIZE(php->len);
	char	strbuf[20];

        /* clear the padding bytes, lest they contain garbage */
	p = (char *)*result + php->len;
	while (p < (char *)*result + jend*sizeof(__pmPDU))
	    *p++ = '~';	/* buffer end */

	if (mypid == -1)
	    mypid = (int)getpid();
	fprintf(stderr, "[%d]pmGetPDU: %s fd=%d len=%d from=%d",
		mypid, __pmPDUTypeStr_r(php->type, strbuf, sizeof(strbuf)), fd, php->len, php->from);
	for (j = 0; j < jend; j++) {
	    if ((j % 8) == 0)
		fprintf(stderr, "\n%03d: ", j);
	    fprintf(stderr, "%8x ", (*result)[j]);
	}
	putc('\n', stderr);
    }
#endif
    if (php->type >= PDU_START && php->type <= PDU_FINISH)
	__pmPDUCntIn[php->type-PDU_START]++;

    /*
     * Note php points into the PDU buffer pdubuf that remains pinned
     * and php is returned via the result parameter ... see the
     * thread-safe comments above
     */
    return php->type;
}

int
__pmGetPDUCeiling(void)
{
    return ceiling;
}

int
__pmSetPDUCeiling(int newceiling)
{
    if (newceiling > 0)
	return (ceiling = newceiling);
    return ceiling;
}

void
__pmSetPDUCntBuf(unsigned *in, unsigned *out)
{
    __pmPDUCntIn = in;
    __pmPDUCntOut = out;
}
